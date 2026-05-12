/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/IListenSocket.h"
#include "net/NetworkAddress.h"

#include <asio.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

class IEventQueue;
class AsioTCPSocket;

//! Asio TCP 监听套接字
/*!
基于 Standalone Asio 的异步 TCP listen socket 实现。
使用独立的 io_context 线程处理 accept 操作。
*/
class AsioTCPListenSocket : public IListenSocket
{
public:
  explicit AsioTCPListenSocket(IEventQueue *events);
  ~AsioTCPListenSocket() override;

  // 禁止拷贝和移动
  AsioTCPListenSocket(const AsioTCPListenSocket &) = delete;
  AsioTCPListenSocket &operator=(const AsioTCPListenSocket &) = delete;

  // ISocket 接口实现
  void bind(const NetworkAddress &) override;
  void close() override;
  void *getEventTarget() const override;

  // IListenSocket 接口实现
  std::unique_ptr<IDataSocket> accept() override;

private:
  void startAsyncAccept();

  asio::io_context m_ioContext;
  asio::ip::tcp::acceptor m_acceptor;
  asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
  std::thread m_ioThread;

  IEventQueue *m_events;
  std::mutex m_mutex;
  std::unique_ptr<AsioTCPSocket> m_pendingSocket;
  std::atomic<bool> m_accepting{false};
};
