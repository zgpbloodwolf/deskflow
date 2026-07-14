/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/BtDataSocket.h"

#include "base/IEventQueue.h"
#include "base/Log.h"

#include <chrono>
#include <cstring>
#include <random>

//
// BtDataSocket
//

BtDataSocket::BtDataSocket(IEventQueue *events) : IDataSocket(events), m_events(events)
{
  LOG_DEBUG("蓝牙数据 socket 创建（客户端模式）");
}

BtDataSocket::BtDataSocket(IEventQueue *events, std::unique_ptr<BtBackend> backend)
    : IDataSocket(events), m_events(events), m_backend(std::move(backend))
{
  LOG_DEBUG("蓝牙数据 socket 创建（服务端已连接模式）");
  m_connected = true;
  m_writable = true;

  // 服务端已接受的连接，启动 I/O 线程
  m_running = true;
  m_ioThread = std::thread(&BtDataSocket::ioThreadFunc, this);

  // 启动重连守护线程（即便服务端默认不自动重连，也常驻以便析构时统一 join）
  m_reconnectThread = std::thread(&BtDataSocket::reconnectLoop, this);
}

BtDataSocket::~BtDataSocket()
{
  // 标记析构，阻止重连与断连事件
  m_destroying = true;
  m_running = false;

  // 持锁关闭 backend（避免与重连线程并发操作 backend），唤醒 ioThread 上阻塞的 recv/select
  {
    std::lock_guard<std::mutex> lk(m_reconnectMutex);
    if (m_backend) {
      m_backend->close();
    }
    m_connected = false;
    m_writable = false;
  }
  // 唤醒重连线程的 wait/wait_for，使其检测 m_destroying 并退出
  m_reconnectCV.notify_all();

  // 等待 I/O 线程结束
  if (m_ioThread.joinable()) {
    m_ioThread.join();
  }
  // 等待重连线程结束（若它正卡在 doReconnect 的 connect，需等待其返回后释放 m_reconnectMutex）
  if (m_reconnectThread.joinable()) {
    m_reconnectThread.join();
  }
}

void BtDataSocket::connect(const NetworkAddress &)
{
  // TCP 地址在蓝牙模式下不适用，使用 connectBt() 代替
  LOG_ERR("蓝牙 socket 不支持 TCP 地址连接，请使用 connectBt()");
}

void BtDataSocket::connectBt(const BtAddress &address)
{
  LOG_INFO("蓝牙 socket：正在连接到 %s", address.toString().c_str());
  m_targetAddress = address;

  // 创建后端并连接
  m_backend = createBtBackend();
  m_backend->connect(address.address(), address.channel());

  if (!m_backend->isConnected()) {
    LOG_ERR("蓝牙 socket：连接失败");
    // 携带后端归类的错误类别，供上层 ClientApp 决定重连策略
    sendConnectionFailed("bluetooth connect failed", m_backend->lastErrorCategory());
    return;
  }

  m_connected = true;
  m_writable = true;
  m_disconnectNotified = false;

  // 启动 I/O 线程
  m_running = true;
  m_ioThread = std::thread(&BtDataSocket::ioThreadFunc, this);

  // 启动重连守护线程
  if (!m_reconnectThread.joinable()) {
    m_reconnectThread = std::thread(&BtDataSocket::reconnectLoop, this);
  }

  // 发送连接成功事件
  sendEvent(EventTypes::DataSocketConnected);
  LOG_INFO("蓝牙 socket：已连接");
}

void BtDataSocket::bind(const NetworkAddress &)
{
  // 蓝牙 socket 不支持 TCP bind
  LOG_WARN("蓝牙 socket 不支持 TCP bind");
}

void BtDataSocket::close()
{
  // 主动关闭：让 ioThread 退出循环，并持锁关闭 backend 避免与重连线程并发
  m_running = false;
  std::lock_guard<std::mutex> lk(m_reconnectMutex);
  if (m_backend) {
    m_backend->close();
  }
  m_connected = false;
  m_writable = false;
}

void *BtDataSocket::getEventTarget() const
{
  return const_cast<void *>(static_cast<const void *>(this));
}

uint32_t BtDataSocket::read(void *buffer, uint32_t n)
{
  if (m_shutdownInput || !m_connected) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_inputBuffer.getSize() == 0) {
    return 0;
  }
  uint32_t toRead = std::min(n, m_inputBuffer.getSize());
  const void *data = m_inputBuffer.peek(toRead);
  memcpy(buffer, data, toRead);
  m_inputBuffer.pop(toRead);
  return toRead;
}

void BtDataSocket::write(const void *buffer, uint32_t n)
{
  if (m_shutdownOutput || !m_writable) {
    LOG_ERR("蓝牙 socket：写入失败，输出已关闭或不可写");
    sendEvent(EventTypes::StreamOutputError);
    return;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_outputBuffer.write(static_cast<const uint8_t *>(buffer), n);
  m_flushed = false;
}

void BtDataSocket::flush()
{
  std::unique_lock<std::mutex> lock(m_flushMutex);
  m_flushCV.wait_for(lock, std::chrono::seconds(30), [this] { return m_flushed.load(); });
}

void BtDataSocket::shutdownInput()
{
  m_shutdownInput = true;
}

void BtDataSocket::shutdownOutput()
{
  m_shutdownOutput = true;
}

bool BtDataSocket::isReady() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_inputBuffer.getSize() > 0;
}

bool BtDataSocket::isFatal() const
{
  return m_fatal;
}

uint32_t BtDataSocket::getSize() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return static_cast<uint32_t>(m_inputBuffer.getSize());
}

void BtDataSocket::ioThreadFunc()
{
  LOG_DEBUG("蓝牙 I/O 线程启动");

  // 读取缓冲区
  std::array<uint8_t, 4096> readBuf{};

  while (m_running && m_connected) {
    // 1. 从蓝牙读取数据
    if (m_backend->pollRead(kIoPollTimeoutMs)) {
      int bytesRead = m_backend->read(readBuf.data(), readBuf.size());
      if (bytesRead > 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_inputBuffer.write(readBuf.data(), bytesRead);

        // 检查缓冲区溢出（1MB 上限）
        if (m_inputBuffer.getSize() > 1024 * 1024) {
          LOG_ERR("蓝牙 socket：输入缓冲区溢出");
          m_fatal = true;
          break;
        }

        sendEvent(EventTypes::StreamInputReady);
      } else if (bytesRead < 0) {
        LOG_ERR("蓝牙 socket：读取错误");
        break;
      }
      // bytesRead == 0 表示无数据，继续轮询
    }

    // 2. 写入待发送数据
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_outputBuffer.getSize() > 0) {
        uint32_t outSize = m_outputBuffer.getSize();
        const void *outData = m_outputBuffer.peek(outSize);
        int bytesWritten = m_backend->write(outData, outSize);
        if (bytesWritten < 0) {
          LOG_ERR("蓝牙 socket：写入错误");
          m_connected = false;
        } else {
          m_outputBuffer.pop(static_cast<uint32_t>(bytesWritten));
          if (m_outputBuffer.getSize() == 0) {
            m_flushed = true;
            m_flushCV.notify_one();
          }
        }
      }
    }

    if (!m_connected) {
      break;
    }
  }

  // 线程退出时处理断连
  if (m_connected || !m_disconnectNotified) {
    handleDisconnect();
  }

  LOG_DEBUG("蓝牙 I/O 线程退出");
}

void BtDataSocket::sendEvent(EventTypes type)
{
  m_events->addEvent(Event(type, getEventTarget()));
}

void BtDataSocket::sendConnectionFailed(const char *reason, BtErrorCategory category)
{
  // 必须携带 ConnectionFailedInfo：Client::handleConnectionFailed 会读取 event data
  // 并 delete，缺失会导致空指针解引用（与 AsioTCPSocket 行为对齐）。
  // 用 DontFreeData，由上层 Client 负责 delete。
  auto *info = new IDataSocket::ConnectionFailedInfo(reason, category);
  m_events->addEvent(Event(EventTypes::DataSocketConnectionFailed, getEventTarget(), info,
                           Event::EventFlags::DontFreeData));
}

void BtDataSocket::handleDisconnect()
{
  // 析构触发的关闭不再通知断连，也不重连
  if (m_destroying) {
    return;
  }

  if (m_disconnectNotified.exchange(true)) {
    return; // 防止重复发送断连事件
  }

  m_connected = false;
  m_writable = false;

  // 释放所有按下的键
  releaseAllKeys();

  sendEvent(EventTypes::SocketDisconnected);
  LOG_INFO("蓝牙 socket：连接已断开");

  // 自动重连：通知独立的重连守护线程（ioThread 不能在自己线程内 join 自己去重连）
  if (m_autoReconnect && m_targetAddress.isValid()) {
    std::lock_guard<std::mutex> lk(m_reconnectMutex);
    m_reconnectRequested = true;
    m_reconnectCV.notify_one();
  }
}

void BtDataSocket::releaseAllKeys()
{
  // 通知上层释放所有按键状态
  m_keyState.releaseAll();
}

// 退避间隔 ±20% 抖动，避免多个客户端在同一时刻同步重连（惊群）
static int btReconnectJitter(int baseSec)
{
  if (baseSec <= 1)
    return baseSec;
  static std::mt19937 gen{std::random_device{}()};
  const int lo = static_cast<int>(baseSec * 0.8);
  const int hi = static_cast<int>(baseSec * 1.2);
  std::uniform_int_distribution<int> dist(lo, hi);
  return dist(gen);
}

void BtDataSocket::reconnectLoop()
{
  while (true) {
    int delaySec = 0;
    {
      // 等待重连请求或析构信号
      std::unique_lock<std::mutex> lk(m_reconnectMutex);
      m_reconnectCV.wait(lk, [this] { return m_destroying.load() || m_reconnectRequested.load(); });
      if (m_destroying) {
        return;
      }
      m_reconnectRequested = false;

      // 指数退避：0s -> 1s -> 2s -> 4s -> ... -> 30s（封顶）
      // 用乘法推导而非 `1 << n`，避免大 n 时有符号 int 移位的未定义行为；首次立即重试
      if (m_reconnectAttempts == 0) {
        delaySec = 0;
      } else {
        delaySec = 1;
        for (int i = 1; i < m_reconnectAttempts; ++i) {
          delaySec = std::min(delaySec * 2, kMaxReconnectDelaySec);
          if (delaySec >= kMaxReconnectDelaySec)
            break;
        }
      }
      m_reconnectAttempts++;
      LOG_INFO("蓝牙 socket：将在 %d 秒后重连（第 %d 次）", delaySec, m_reconnectAttempts);

      // 退避等待（带 ±20% 抖动），析构时可被提前唤醒
      if (delaySec > 0) {
        m_reconnectCV.wait_for(
            lk, std::chrono::seconds(btReconnectJitter(delaySec)), [this] { return m_destroying.load(); }
        );
      }
      if (m_destroying) {
        return;
      }
    } // 释放锁，doReconnect 内部自行加锁，避免递归加锁

    doReconnect();
  }
}

void BtDataSocket::doReconnect()
{
  // 由重连守护线程调用（而非 ioThread 自身），因此 join m_ioThread 不会发生自 join 死锁
  std::lock_guard<std::mutex> lk(m_reconnectMutex);
  LOG_INFO("蓝牙 socket：正在重连...");

  // 1. 回收旧 I/O 线程（此时它已退出，join 立即返回）
  m_running = false;
  if (m_ioThread.joinable()) {
    m_ioThread.join();
  }

  // 2. 关闭并释放旧 backend
  if (m_backend) {
    m_backend->close();
    m_backend.reset();
  }
  m_connected = false;
  m_writable = false;
  m_disconnectNotified = false;

  // 析构竞争：拿锁前若对象已开始析构，放弃重连
  if (m_destroying) {
    return;
  }

  // 3. 重新创建后端并连接
  m_backend = createBtBackend();
  m_backend->connect(m_targetAddress.address(), m_targetAddress.channel());

  // connect 阻塞期间若对象开始析构，放弃重连
  if (m_destroying) {
    if (m_backend) {
      m_backend->close();
      m_backend.reset();
    }
    return;
  }

  if (!m_backend->isConnected()) {
    LOG_ERR("蓝牙 socket：重连失败");
    if (m_reconnectAttempts < kMaxReconnectAttempts) {
      // 排队下一次重连（循环会立即重新检查并按退避等待）
      m_reconnectRequested = true;
    } else {
      LOG_ERR("蓝牙 socket：已达最大重连次数 (%d)", kMaxReconnectAttempts);
      sendConnectionFailed("bluetooth reconnect exhausted");
    }
    return;
  }

  m_connected = true;
  m_writable = true;
  m_reconnectAttempts = 0;

  // 4. 启动新的 I/O 线程
  m_running = true;
  m_ioThread = std::thread(&BtDataSocket::ioThreadFunc, this);

  sendEvent(EventTypes::DataSocketConnected);
  LOG_INFO("蓝牙 socket：重连成功");
}
