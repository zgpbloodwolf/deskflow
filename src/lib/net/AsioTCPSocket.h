/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "io/StreamBuffer.h"
#include "net/IDataSocket.h"
#include "net/NetworkAddress.h"

#include <asio.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

class IEventQueue;

//! Asio TCP 数据套接字
/*!
基于 Standalone Asio 的异步 TCP socket 实现。
每个连接拥有独立的 io_context 线程 (D-04)。
Asio 完成回调通过桥接层投递事件到 EventQueue (D-06)。
*/
class AsioTCPSocket : public IDataSocket, public std::enable_shared_from_this<AsioTCPSocket>
{
public:
  explicit AsioTCPSocket(IEventQueue *events);
  AsioTCPSocket(IEventQueue *events, asio::ip::tcp::socket socket);
  ~AsioTCPSocket() override;

  // 禁止拷贝和移动
  AsioTCPSocket(const AsioTCPSocket &) = delete;
  AsioTCPSocket &operator=(const AsioTCPSocket &) = delete;

  // ISocket 接口实现
  void bind(const NetworkAddress &) override;
  void close() override;
  void *getEventTarget() const override;

  // IStream 接口实现
  uint32_t read(void *buffer, uint32_t n) override;
  void write(const void *buffer, uint32_t n) override;
  void flush() override;
  void shutdownInput() override;
  void shutdownOutput() override;
  bool isReady() const override;
  bool isFatal() const override;
  uint32_t getSize() const override;

  // IDataSocket 接口实现
  void connect(const NetworkAddress &) override;

private:
  void startAsyncRead();
  void onReadComplete(const std::error_code &ec, size_t bytesTransferred);
  void onWriteComplete(const std::error_code &ec, size_t bytesTransferred);
  void startAsyncWrite();
  void sendEvent(EventTypes type);

  asio::io_context m_ioContext;
  asio::ip::tcp::socket m_socket;
  asio::strand<asio::io_context::executor_type> m_strand;
  asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
  std::thread m_ioThread;

  IEventQueue *m_events;
  mutable std::mutex m_mutex;

  StreamBuffer m_inputBuffer;
  StreamBuffer m_outputBuffer;

  std::array<char, 4096> m_readBuffer{};
  std::atomic<bool> m_connected{false};
  std::atomic<bool> m_writable{false};

  // flush() 同步等待 -- 使用 condition_variable 模式
  std::mutex m_flushMutex;
  std::condition_variable m_flushCV;
  std::atomic<bool> m_flushed{true};
};
