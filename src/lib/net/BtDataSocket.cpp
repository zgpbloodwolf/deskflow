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
}

BtDataSocket::~BtDataSocket()
{
  close();

  // 等待 I/O 线程结束
  m_running = false;
  if (m_ioThread.joinable()) {
    m_ioThread.join();
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
    sendEvent(EventTypes::DataSocketConnectionFailed);
    return;
  }

  m_connected = true;
  m_writable = true;
  m_disconnectNotified = false;

  // 启动 I/O 线程
  m_running = true;
  m_ioThread = std::thread(&BtDataSocket::ioThreadFunc, this);

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
  if (m_backend) {
    m_backend->close();
  }
  m_connected = false;
  m_writable = false;
  m_running = false;
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

void BtDataSocket::handleDisconnect()
{
  if (m_disconnectNotified.exchange(true)) {
    return; // 防止重复发送断连事件
  }

  m_connected = false;
  m_writable = false;

  // 释放所有按下的键
  releaseAllKeys();

  sendEvent(EventTypes::SocketDisconnected);
  LOG_INFO("蓝牙 socket：连接已断开");

  // 自动重连
  if (m_autoReconnect && m_targetAddress.isValid()) {
    scheduleReconnect();
  }
}

void BtDataSocket::releaseAllKeys()
{
  // 通知上层释放所有按键状态
  m_keyState.releaseAll();
}

void BtDataSocket::scheduleReconnect()
{
  if (m_reconnectAttempts >= kMaxReconnectAttempts) {
    LOG_ERR("蓝牙 socket：已达最大重连次数 (%d)", kMaxReconnectAttempts);
    return;
  }

  // 指数退避：0s -> 1s -> 2s -> 4s -> ... -> 30s
  int delaySec = m_reconnectAttempts == 0 ? 0 : std::min(1 << (m_reconnectAttempts - 1), kMaxReconnectDelaySec);
  m_reconnectAttempts++;

  LOG_INFO("蓝牙 socket：将在 %d 秒后重连（第 %d 次）", delaySec, m_reconnectAttempts);

  std::this_thread::sleep_for(std::chrono::seconds(delaySec));
  doReconnect();
}

void BtDataSocket::doReconnect()
{
  LOG_INFO("蓝牙 socket：正在重连...");

  // 关闭旧连接
  if (m_backend) {
    m_backend->close();
  }
  m_connected = false;
  m_writable = false;
  m_disconnectNotified = false;

  // 重新连接
  m_backend = createBtBackend();
  m_backend->connect(m_targetAddress.address(), m_targetAddress.channel());

  if (!m_backend->isConnected()) {
    LOG_ERR("蓝牙 socket：重连失败");
    scheduleReconnect();
    return;
  }

  m_connected = true;
  m_writable = true;
  m_reconnectAttempts = 0;

  // 重新启动 I/O 线程
  if (!m_running) {
    m_running = true;
    if (m_ioThread.joinable()) {
      m_ioThread.join();
    }
    m_ioThread = std::thread(&BtDataSocket::ioThreadFunc, this);
  }

  sendEvent(EventTypes::DataSocketConnected);
  LOG_INFO("蓝牙 socket：重连成功");
}
