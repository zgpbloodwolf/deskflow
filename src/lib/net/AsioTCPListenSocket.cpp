/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/AsioTCPListenSocket.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "net/AsioTCPSocket.h"
#include "net/NetworkAddress.h"
#include "net/SocketException.h"

#include <system_error>

//
// AsioTCPListenSocket
//

AsioTCPListenSocket::AsioTCPListenSocket(IEventQueue *events)
    : m_acceptor(m_ioContext),
      m_workGuard(m_ioContext.get_executor()),
      m_events(events)
{
  LOG_DEBUG("创建 Asio TCP listen socket");
}

AsioTCPListenSocket::~AsioTCPListenSocket()
{
  try {
    close();
  } catch (...) {
    LOG_DEBUG("Asio TCP listen socket 析构时发生错误");
  }

  m_ioContext.stop();
  if (m_ioThread.joinable()) {
    m_ioThread.join();
  }
}

void AsioTCPListenSocket::bind(const NetworkAddress &addr)
{
  LOG_DEBUG("绑定监听地址: %s:%d", addr.getHostname().c_str(), addr.getPort());

  try {
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), addr.getPort());

    // 打开 acceptor
    m_acceptor.open(endpoint.protocol());

    // Unix 平台设置 SO_REUSEADDR，允许快速重启服务
#ifndef _WIN32
    m_acceptor.set_option(asio::socket_base::reuse_address(true));
#endif

    // 绑定并监听
    m_acceptor.bind(endpoint);
    m_acceptor.listen(asio::socket_base::max_listen_connections);

    // 启动 io_context 线程
    if (!m_ioThread.joinable()) {
      m_ioThread = std::thread([this]() { m_ioContext.run(); });
    }

    // 开始异步接受连接
    startAsyncAccept();
  } catch (const std::system_error &e) {
    if (e.code() == asio::error::address_in_use) {
      throw SocketAddressInUseException(e.what());
    }
    throw SocketBindException(e.what());
  }
}

void AsioTCPListenSocket::close()
{
  std::error_code ec;
  m_acceptor.close(ec);
  if (ec) {
    LOG_WARN("关闭 Asio acceptor 时出错: %s", ec.message().c_str());
  }
}

void *AsioTCPListenSocket::getEventTarget() const
{
  return const_cast<void *>(static_cast<const void *>(this));
}

std::unique_ptr<IDataSocket> AsioTCPListenSocket::accept()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_pendingSocket) {
    // 有待处理的已接受连接，返回它
    // 释放裸指针的所有权给 unique_ptr
    auto socket = std::unique_ptr<IDataSocket>(m_pendingSocket.release());
    // 继续接受下一个连接
    startAsyncAccept();
    return socket;
  }
  return nullptr;
}

void AsioTCPListenSocket::startAsyncAccept()
{
  if (!m_acceptor.is_open()) {
    return;
  }

  m_acceptor.async_accept(
      [this](const std::error_code &ec, asio::ip::tcp::socket socket) {
        if (ec) {
          if (ec == asio::error::operation_aborted) {
            // acceptor 已关闭，正常退出
            return;
          }
          LOG_WARN("接受连接时出错: %s", ec.message().c_str());
          // 出错后继续尝试接受
          startAsyncAccept();
          return;
        }

        LOG_DEBUG("Asio acceptor 接受到新连接");

        // 创建 AsioTCPSocket 包装已接受的 socket
        // 注意：接受的 socket 需要在独立的 io_context 上运行
        auto newSocket = std::make_unique<AsioTCPSocket>(m_events, std::move(socket));

        // 设置 TCP_NODELAY 和 keep-alive
        // （已在 AsioTCPSocket 的 connect 完成回调中设置，这里为已接受 socket 额外设置）
        // AsioTCPSocket 构造函数不设置这些选项，需要在 accept 后设置
        // 但由于 socket 已被移动到 AsioTCPSocket，需要通过其 io_context 访问

        {
          std::lock_guard<std::mutex> lock(m_mutex);
          m_pendingSocket = std::move(newSocket);
        }

        // 通知上层有新连接等待接受
        m_events->addEvent(Event(EventTypes::ListenSocketConnecting, getEventTarget()));
      }
  );
}
