/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/AsioTCPSocket.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "net/NetworkAddress.h"
#include "net/SocketException.h"

#include <array>
#include <cstring>
#include <mutex>

// 输入缓冲区最大容量限制（1MB），与 TCPSocket 保持一致
static const std::size_t s_maxInputBufferSize = 1024 * 1024;

//
// AsioTCPSocket
//

AsioTCPSocket::AsioTCPSocket(IEventQueue *events)
    : IDataSocket(events),
      m_socket(m_ioContext),
      m_strand(asio::make_strand(m_ioContext)),
      m_workGuard(m_ioContext.get_executor()),
      m_mouseTimer(m_ioContext),
      m_keyboardPollTimer(m_ioContext),
      m_reconnectTimer(m_ioContext),
      m_events(events)
{
  LOG_DEBUG("创建 Asio TCP socket (新连接)");
}

AsioTCPSocket::AsioTCPSocket(IEventQueue *events, asio::ip::tcp::socket socket)
    : IDataSocket(events),
      m_socket(std::move(socket)),
      m_strand(asio::make_strand(m_ioContext)),
      m_workGuard(m_ioContext.get_executor()),
      m_mouseTimer(m_ioContext),
      m_keyboardPollTimer(m_ioContext),
      m_reconnectTimer(m_ioContext),
      m_events(events)
{
  // 已接受的连接，socket 已经从 acceptor 转移过来
  // 注意：被接受的 socket 来自另一个 io_context，需要移动到当前 io_context
  // 由于 Asio 的移动语义，tcp::socket 可以在不同 io_context 间转移
  LOG_DEBUG("创建 Asio TCP socket (已接受连接)");
}

AsioTCPSocket::~AsioTCPSocket()
{
  try {
    close();
  } catch (...) {
    LOG_DEBUG("Asio TCP socket 析构时发生错误");
  }

  // 停止 io_context 并等待线程结束
  m_ioContext.stop();
  if (m_ioThread.joinable()) {
    m_ioThread.join();
  }
}

void AsioTCPSocket::bind(const NetworkAddress &addr)
{
  // TCP 数据 socket 通常不需要 bind，但接口要求实现
  // 如果需要绑定（例如用于特定本地地址），可以在这里实现
  try {
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), addr.getPort());
    m_socket.open(asio::ip::tcp::v4());
    m_socket.bind(endpoint);
  } catch (const std::system_error &e) {
    throw SocketBindException(e.what());
  }
}

void AsioTCPSocket::close()
{
  if (m_connected.exchange(false)) {
    sendEvent(EventTypes::SocketDisconnected);
  }

  // 取消定时器（停止鼠标和键盘定时器循环）
  m_mouseTimer.cancel();
  m_keyboardPollTimer.cancel();

  std::error_code ec;
  m_socket.close(ec);
  if (ec) {
    LOG_WARN("关闭 Asio socket 时出错: %s", ec.message().c_str());
  }
}

void *AsioTCPSocket::getEventTarget() const
{
  return const_cast<void *>(static_cast<const void *>(this));
}

uint32_t AsioTCPSocket::read(void *buffer, uint32_t n)
{
  // 从输入缓冲区拷贝数据，与 TCPSocket::read 模式相同
  std::lock_guard<std::mutex> lock(m_mutex);
  if (uint32_t size = m_inputBuffer.getSize(); n > size) {
    n = size;
  }
  if (buffer != nullptr && n != 0) {
    std::memcpy(buffer, m_inputBuffer.peek(n), n);
  }
  m_inputBuffer.pop(n);
  return n;
}

void AsioTCPSocket::write(const void *buffer, uint32_t n)
{
  if (n == 0) {
    return;
  }

  // 检查是否可写
  if (!m_writable.load()) {
    sendEvent(EventTypes::StreamOutputError);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool wasEmpty = (m_outputBuffer.getSize() == 0);
    m_outputBuffer.write(buffer, n);
    m_flushed = false;
  }

  // 通过 asio::post 将写操作调度到 io_context 线程
  asio::post(m_strand, [this]() { startAsyncWrite(); });
}

void AsioTCPSocket::flush()
{
  // 等待输出缓冲区所有数据被写完
  // 使用 condition_variable 实现同步等待，防止死锁
  std::unique_lock<std::mutex> lock(m_flushMutex);
  m_flushCV.wait_for(lock, std::chrono::seconds(30), [this]() { return m_flushed.load(); });
}

void AsioTCPSocket::shutdownInput()
{
  std::error_code ec;
  m_socket.shutdown(asio::ip::tcp::socket::shutdown_receive, ec);
  if (ec) {
    LOG_WARN("关闭 socket 接收端时出错: %s", ec.message().c_str());
  }
  sendEvent(EventTypes::StreamInputShutdown);
}

void AsioTCPSocket::shutdownOutput()
{
  std::error_code ec;
  m_socket.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
  if (ec) {
    LOG_WARN("关闭 socket 发送端时出错: %s", ec.message().c_str());
  }
  sendEvent(EventTypes::StreamOutputShutdown);

  // 输出关闭后标记为已刷写
  {
    std::lock_guard<std::mutex> lock(m_flushMutex);
    m_flushed = true;
    m_flushCV.notify_all();
  }
}

bool AsioTCPSocket::isReady() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_inputBuffer.getSize() > 0;
}

bool AsioTCPSocket::isFatal() const
{
  // TCP socket 不存在致命状态（与 TCPSocket::isFatal 一致）
  return false;
}

uint32_t AsioTCPSocket::getSize() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_inputBuffer.getSize();
}

void AsioTCPSocket::connect(const NetworkAddress &addr)
{
  if (m_connected.load()) {
    auto *info = new ConnectionFailedInfo("busy");
    m_events->addEvent(
        Event(EventTypes::DataSocketConnectionFailed, getEventTarget(), info, Event::EventFlags::DontFreeData)
    );
    return;
  }

  // 保存目标地址用于重连 (D-11)，重置退避延迟
  m_targetAddress = addr;
  m_reconnectDelay = std::chrono::seconds(0);

  LOG_DEBUG("Asio socket 正在连接 %s:%d", addr.getHostname().c_str(), addr.getPort());

  // 启动 io_context 线程（如果尚未启动）
  if (!m_ioThread.joinable()) {
    m_ioThread = std::thread([this]() { m_ioContext.run(); });
  }

  // 异步解析和连接
  auto self = shared_from_this();
  auto resolver = std::make_shared<asio::ip::tcp::resolver>(m_ioContext);
  resolver->async_resolve(
      addr.getHostname(), std::to_string(addr.getPort()),
      asio::bind_executor(
          m_strand,
          [this, self, resolver](const std::error_code &ec, asio::ip::tcp::resolver::results_type endpoints) {
            if (ec) {
              LOG_WARN("DNS 解析失败: %s", ec.message().c_str());
              auto *info = new ConnectionFailedInfo(ec.message().c_str());
              m_events->addEvent(Event(
                  EventTypes::DataSocketConnectionFailed, getEventTarget(), info, Event::EventFlags::DontFreeData
              ));
              // 自动重连模式下，连接失败后调度重连 (D-11)
              if (m_autoReconnect) {
                m_reconnectDelay = std::chrono::seconds(1);
                scheduleReconnect();
              }
              return;
            }

            // DNS 解析成功，发起异步连接
            asio::async_connect(
                m_socket, endpoints,
                asio::bind_executor(
                    m_strand,
                    [this, self](const std::error_code &connectEc, const asio::ip::tcp::endpoint &) {
                      if (connectEc) {
                        LOG_WARN("连接失败: %s", connectEc.message().c_str());
                        auto *info = new ConnectionFailedInfo(connectEc.message().c_str());
                        m_events->addEvent(Event(
                            EventTypes::DataSocketConnectionFailed, getEventTarget(), info,
                            Event::EventFlags::DontFreeData
                        ));
                        // 自动重连模式下，连接失败后调度重连 (D-11)
                        if (m_autoReconnect) {
                          m_reconnectDelay = std::chrono::seconds(1);
                          scheduleReconnect();
                        }
                        return;
                      }

                      // 连接成功
                      LOG_DEBUG("Asio socket 连接成功");

                      // 设置 TCP_NODELAY（禁用 Nagle 算法，减少延迟）
                      m_socket.set_option(asio::ip::tcp::no_delay(true));
                      // 启用 TCP keep-alive
                      m_socket.set_option(asio::socket_base::keep_alive(true));

                      bool wasReconnect = m_connected.exchange(true);
                      m_writable = true;
                      sendEvent(EventTypes::DataSocketConnected);

                      if (wasReconnect) {
                        // 重连成功，恢复输入流 (D-11)
                        onReconnectSuccess();
                      } else {
                        // 首次连接
                        startAsyncRead();
                        startMouseSendTimer();
                        drainKeyboardFifo();
                      }
                    }
                )
            );
          }
      )
  );
}

void AsioTCPSocket::startAsyncRead()
{
  auto self = shared_from_this();
  m_socket.async_read_some(
      asio::buffer(m_readBuffer),
      asio::bind_executor(
          m_strand,
          [this, self](const std::error_code &ec, size_t bytesTransferred) {
            onReadComplete(ec, bytesTransferred);
          }
      )
  );
}

void AsioTCPSocket::onReadComplete(const std::error_code &ec, size_t bytesTransferred)
{
  if (ec) {
    // 统一断连处理 (D-09/D-10/D-11)
    handleDisconnect(ec);
    return;
  }

  // 将收到的数据写入输入缓冲区
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputBuffer.write(m_readBuffer.data(), static_cast<uint32_t>(bytesTransferred));

    // 缓冲区大小限制检查
    if (m_inputBuffer.getSize() > s_maxInputBufferSize) {
      LOG_WARN("输入缓冲区超过 1MB 限制，丢弃多余数据");
      m_inputBuffer.pop(m_inputBuffer.getSize() - static_cast<uint32_t>(s_maxInputBufferSize));
    }
  }

  // 桥接到 EventQueue：通知上层有数据可读
  sendEvent(EventTypes::StreamInputReady);

  // 继续异步读取
  startAsyncRead();
}

void AsioTCPSocket::startAsyncWrite()
{
  std::vector<uint8_t> dataToSend;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_outputBuffer.getSize() == 0) {
      // 输出缓冲区已空，标记为已刷写
      m_flushed = true;
      m_flushCV.notify_all();
      return;
    }
    uint32_t size = m_outputBuffer.getSize();
    const void *data = m_outputBuffer.peek(size);
    dataToSend.assign(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);
  }

  auto self = shared_from_this();
  asio::async_write(
      m_socket, asio::buffer(dataToSend),
      asio::bind_executor(
          m_strand,
          [this, self, dataToSend](const std::error_code &ec, size_t bytesTransferred) {
            if (ec) {
              LOG_WARN("Asio socket 写入错误: %s", ec.message().c_str());
              sendEvent(EventTypes::StreamOutputError);
              return;
            }
            onWriteComplete(ec, bytesTransferred);
          }
      )
  );
}

void AsioTCPSocket::onWriteComplete(const std::error_code &ec, size_t bytesTransferred)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    // 从输出缓冲区移除已写完的数据
    m_outputBuffer.pop(static_cast<uint32_t>(bytesTransferred));
  }

  if (m_outputBuffer.getSize() == 0) {
    // 所有数据已发送完毕
    sendEvent(EventTypes::StreamOutputFlushed);
    {
      std::lock_guard<std::mutex> flushLock(m_flushMutex);
      m_flushed = true;
      m_flushCV.notify_all();
    }
  } else {
    // 还有数据要发送，继续异步写入
    asio::post(m_strand, [this]() { startAsyncWrite(); });
  }
}

void AsioTCPSocket::sendEvent(EventTypes type)
{
  m_events->addEvent(Event(type, getEventTarget()));
}

//
// SPSC 缓冲区集成方法 (D-05/D-08/D-10)
//

void AsioTCPSocket::startMouseSendTimer()
{
  // 200Hz = 5ms 间隔 (D-08)
  m_mouseTimer.expires_after(std::chrono::milliseconds(5));
  m_mouseTimer.async_wait(asio::bind_executor(
      m_strand, [this, self = shared_from_this()](const std::error_code &ec) { onMouseTimer(ec); }
  ));
}

void AsioTCPSocket::onMouseTimer(const std::error_code &ec)
{
  if (ec) {
    // 定时器被取消（正常关闭）
    return;
  }

  int32_t x, y;
  if (m_eventBuffer.loadMousePosition(x, y)) {
    // 有新的鼠标位置，通过 ProtocolUtil 发送 DMMV (D-08)
    ProtocolUtil::writef(static_cast<deskflow::IStream *>(this), kMsgDMouseMove, x, y);
  }

  // 重新调度定时器
  startMouseSendTimer();
}

void AsioTCPSocket::drainKeyboardFifo()
{
  // 在 io_context 线程上运行（strand 保护）
  KeyboardEvent evt;
  while (m_eventBuffer.popKeyboardEvent(evt)) {
    sendKeyboardEvent(evt);
  }

  // 通过短超时定时器调度下一次检查（1ms），避免忙等
  m_keyboardPollTimer.expires_after(std::chrono::milliseconds(1));
  m_keyboardPollTimer.async_wait(asio::bind_executor(
      m_strand, [this, self = shared_from_this()](const std::error_code &ec) {
        if (!ec) {
          drainKeyboardFifo();
        }
      }
  ));
}

void AsioTCPSocket::sendKeyboardEvent(const KeyboardEvent &evt)
{
  switch (evt.type) {
  case KeyboardEvent::Type::Down:
    m_keyState.recordKeyDown(evt.key, evt.mask, evt.button);
    ProtocolUtil::writef(
        static_cast<deskflow::IStream *>(this), kMsgDKeyDownLang, evt.key, evt.mask, evt.button,
        &evt.language
    );
    break;
  case KeyboardEvent::Type::Up:
    m_keyState.recordKeyUp(evt.key);
    ProtocolUtil::writef(
        static_cast<deskflow::IStream *>(this), kMsgDKeyUp1_0, evt.key, evt.mask
    );
    break;
  case KeyboardEvent::Type::Repeat:
    ProtocolUtil::writef(
        static_cast<deskflow::IStream *>(this), kMsgDKeyRepeat1_0, evt.key, evt.mask,
        static_cast<int16_t>(1)
    );
    break;
  }
}

//
// 断连处理和自动重连 (D-09/D-10/D-11)
//

void AsioTCPSocket::handleDisconnect(const std::error_code &ec)
{
  // 主动关闭不处理重连
  if (ec == asio::error::operation_aborted) {
    return;
  }

  if (ec == asio::error::eof || ec == asio::error::connection_reset) {
    LOG_DEBUG("Asio socket 远端断连 (EOF/connection_reset)");
  } else {
    LOG_WARN("Asio socket 连接断开: %s", ec.message().c_str());
  }

  // 1. 释放所有按下的键和鼠标按钮 (D-10)
  releaseAllKeys();

  // 2. 取消所有定时器
  m_mouseTimer.cancel();
  m_keyboardPollTimer.cancel();

  // 3. 通知上层断连 (D-06 桥接)
  m_connected.store(false, std::memory_order_release);
  m_writable.store(false, std::memory_order_release);
  sendEvent(EventTypes::SocketDisconnected);

  // 4. 自动重连（客户端模式，D-11）
  if (m_autoReconnect) {
    scheduleReconnect();
  }
}

void AsioTCPSocket::releaseAllKeys()
{
  auto releases = m_keyState.releaseAll();
  for (const auto &r : releases) {
    LOG_DEBUG("断连释放按键: key=%d, mask=0x%04x, button=0x%04x", r.key, r.mask, r.button);
    // 注意：此时 socket 可能已断开，writef 可能失败，这是预期行为
    // 重点是清空 KeyStateTable 状态
    ProtocolUtil::writef(static_cast<deskflow::IStream *>(this), kMsgDKeyUp1_0, r.key, r.mask);
  }
}

void AsioTCPSocket::scheduleReconnect()
{
  if (m_reconnectDelay == std::chrono::seconds(0)) {
    // 首次立即重试 (D-11)
    LOG_DEBUG("立即尝试重连");
    doReconnect();
    return;
  }

  LOG_DEBUG("计划 %lld 秒后重连", static_cast<long long>(m_reconnectDelay.count()));
  m_reconnectTimer.expires_after(m_reconnectDelay);
  m_reconnectTimer.async_wait(asio::bind_executor(
      m_strand, [this, self = shared_from_this()](const std::error_code &ec) {
        if (ec) {
          return; // 定时器被取消
        }
        doReconnect();
      }
  ));

  // 指数退避：1s -> 2s -> 4s -> ... -> 最大 30s (D-11)
  m_reconnectDelay = std::min(m_reconnectDelay * 2, std::chrono::seconds(30));
}

void AsioTCPSocket::doReconnect()
{
  m_reconnectDelay = std::chrono::seconds(1); // 首次失败后 1 秒
  std::error_code ignored_ec;
  m_socket.close(ignored_ec);
  connect(m_targetAddress);
}

void AsioTCPSocket::onReconnectSuccess()
{
  LOG_INFO("重连成功，恢复输入流");
  m_reconnectDelay = std::chrono::seconds(0); // 重置退避延迟
  // 重新启动 200Hz 鼠标定时器和键盘 FIFO 消费
  startAsyncRead();
  startMouseSendTimer();
  drainKeyboardFifo();
}
