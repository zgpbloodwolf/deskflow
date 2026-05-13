/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "io/StreamBuffer.h"
#include "net/IDataSocket.h"
#include "net/InputEventBuffer.h"
#include "net/KeyStateTable.h"
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
  //! 已接受连接的构造函数，共享外部 io_context（CR-02 修复）
  /*!
  接受的 socket 必须使用与 acceptor 相同的 io_context，
  否则 Asio 回调会在错误的线程上执行。
  */
  AsioTCPSocket(IEventQueue *events, asio::ip::tcp::socket socket, asio::io_context &externalIoContext);
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

  // SPSC 缓冲区相关方法 (D-05/D-08/D-10)
  void startMouseSendTimer();                                  // 200Hz 定时读取最新鼠标位置
  void onMouseTimer(const std::error_code &ec);                // 200Hz 定时器回调
  void drainKeyboardFifo();                                    // 排空键盘 FIFO 并发送
  void sendKeyboardEvent(const KeyboardEvent &evt);            // 序列化单个键盘事件

  // 断连处理和自动重连 (D-09/D-10/D-11)
  void handleDisconnect(const std::error_code &ec);            // 统一断连处理入口
  void releaseAllKeys();                                       // 释放所有按下的键和鼠标按钮
  void scheduleReconnect();                                    // 指数退避重连调度
  void doReconnect();                                          // 执行重连
  void onReconnectSuccess();                                   // 重连成功后恢复输入流

  asio::io_context m_ioContext;                  // 自有 io_context（仅客户端主动连接时使用）
  asio::io_context *m_ioContextPtr;              // 指向实际使用的 io_context（自有或外部）
  bool m_ownsIoContext;                          // 是否拥有 io_context（决定析构时是否停止/等待线程）
  asio::ip::tcp::socket m_socket;
  asio::strand<asio::io_context::executor_type> m_strand;
  asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
  std::thread m_ioThread;

  IEventQueue *m_events;
  mutable std::mutex m_mutex;

  StreamBuffer m_inputBuffer;
  StreamBuffer m_outputBuffer;

  InputEventBuffer m_eventBuffer;      // SPSC 事件缓冲区 (D-05)
  KeyStateTable m_keyState;            // 按键状态追踪表 (D-10)
  asio::steady_timer m_mouseTimer;     // 200Hz 鼠标发送定时器 (D-08)
  asio::steady_timer m_keyboardPollTimer; // 键盘 FIFO 轮询定时器

  // 自动重连相关成员 (D-11)
  asio::steady_timer m_reconnectTimer;                    // 重连退避定时器
  std::chrono::seconds m_reconnectDelay{0};               // 当前退避延迟 (0=首次立即重试)
  NetworkAddress m_targetAddress;                         // 重连目标地址
  bool m_autoReconnect{false};                            // 是否启用自动重连（客户端模式）

  std::array<char, 4096> m_readBuffer{};
  std::atomic<bool> m_connected{false};
  std::atomic<bool> m_writable{false};

  // flush() 同步等待 -- 使用 condition_variable 模式
  std::mutex m_flushMutex;
  std::condition_variable m_flushCV;
  std::atomic<bool> m_flushed{true};

public:
  //! 获取 SPSC 事件缓冲区引用，供上层路由输入事件
  InputEventBuffer &eventBuffer() { return m_eventBuffer; }

  //! 设置是否启用自动重连（客户端模式启用，服务端不启用）
  void setAutoReconnect(bool enabled) { m_autoReconnect = enabled; }
};
