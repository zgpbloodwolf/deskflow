/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "io/StreamBuffer.h"
#include "net/BtAddress.h"
#include "net/BtBackend.h"
#include "net/IDataSocket.h"
#include "net/InputEventBuffer.h"
#include "net/KeyStateTable.h"
#include "net/NetworkAddress.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

class IEventQueue;

//! 蓝牙 RFCOMM 数据套接字
/*!
基于蓝牙 RFCOMM 的数据传输 socket 实现。
参照 AsioTCPSocket 的设计模式：
- 客户端模式：拥有独立 I/O 线程
- 服务端模式：共享 listener 的 I/O 线程或使用后端内置回调
- 使用 StreamBuffer 做输入/输出缓冲
- 使用 InputEventBuffer 做 SPSC 键鼠事件队列
- 通过 IEventQueue 发送与 TCP 相同的事件类型
*/
class BtDataSocket : public IDataSocket
{
public:
  //! 客户端构造函数（主动连接）
  explicit BtDataSocket(IEventQueue *events);

  //! 服务端构造函数（从已接受的后端构造）
  BtDataSocket(IEventQueue *events, std::unique_ptr<BtBackend> backend);

  ~BtDataSocket() override;

  // 禁止拷贝和移动
  BtDataSocket(const BtDataSocket &) = delete;
  BtDataSocket &operator=(const BtDataSocket &) = delete;

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

  //! 蓝牙专用连接方法
  void connectBt(const BtAddress &address);

  //! 设置是否启用自动重连
  void setAutoReconnect(bool enabled)
  {
    m_autoReconnect = enabled;
  }

  //! 获取 SPSC 事件缓冲区引用
  InputEventBuffer &eventBuffer()
  {
    return m_eventBuffer;
  }

private:
  //! I/O 线程主循环
  void ioThreadFunc();

  //! 发送事件到 EventQueue
  void sendEvent(EventTypes type);

  //! 发送连接失败事件，携带 ConnectionFailedInfo（Client::handleConnectionFailed 会读取并 delete）
  void sendConnectionFailed(const char *reason, BtErrorCategory category = BtErrorCategory::Unknown);

  //! 统一断连处理
  void handleDisconnect();

  //! 释放所有按下的键
  void releaseAllKeys();

  //! 自动重连守护线程主循环（独立于 ioThread，避免线程内自 join）
  void reconnectLoop();

  //! 执行重连（由 reconnectLoop 调用，不可由 ioThread 直接调用）
  void doReconnect();

  IEventQueue *m_events;
  mutable std::mutex m_mutex;

  std::unique_ptr<BtBackend> m_backend;
  std::thread m_ioThread;
  std::thread m_reconnectThread; // 重连守护线程，负责 join 旧 ioThread 并重建连接
  std::mutex m_reconnectMutex;
  std::condition_variable m_reconnectCV;
  std::atomic<bool> m_reconnectRequested{false};
  std::atomic<bool> m_destroying{false};
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_shutdownInput{false};
  std::atomic<bool> m_shutdownOutput{false};

  StreamBuffer m_inputBuffer;
  StreamBuffer m_outputBuffer;

  InputEventBuffer m_eventBuffer; // SPSC 事件缓冲区
  KeyStateTable m_keyState;       // 按键状态追踪

  // 自动重连相关
  BtAddress m_targetAddress;
  std::atomic<bool> m_autoReconnect{false};
  int m_reconnectAttempts{0};
  static constexpr int kMaxReconnectAttempts = 10;
  static constexpr int kMaxReconnectDelaySec = 30;
  static constexpr int kIoPollTimeoutMs = 5; // I/O 轮询超时

  std::atomic<bool> m_connected{false};
  std::atomic<bool> m_writable{false};
  std::atomic<bool> m_fatal{false};
  std::atomic<bool> m_disconnectNotified{false};

  // flush() 同步等待
  std::mutex m_flushMutex;
  std::condition_variable m_flushCV;
  std::atomic<bool> m_flushed{true};
};
