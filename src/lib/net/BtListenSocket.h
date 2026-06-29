/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/BtBackend.h"
#include "net/IListenSocket.h"
#include "net/NetworkAddress.h"

#include "base/EventTypes.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

class IEventQueue;
class BtDataSocket;
struct __CFRunLoopTimer; // macOS CFRunLoopTimerRef 前向声明

//! 蓝牙 RFCOMM 监听套接字
/*!
基于蓝牙 RFCOMM 的监听 socket 实现。
在指定通道上监听传入的蓝牙连接。
*/
class BtListenSocket : public IListenSocket
{
public:
  explicit BtListenSocket(IEventQueue *events);
  ~BtListenSocket() override;

  // 禁止拷贝和移动
  BtListenSocket(const BtListenSocket &) = delete;
  BtListenSocket &operator=(const BtListenSocket &) = delete;

  // ISocket 接口实现
  void bind(const NetworkAddress &) override;
  void close() override;
  void *getEventTarget() const override;

  // IListenSocket 接口实现
  std::shared_ptr<IDataSocket> accept() override;

  //! 蓝牙专用绑定方法
  void bindBt(int channel);

private:
  //! 接受线程主循环
  void acceptThreadFunc();

  //! 蓝牙接受线程主循环（在 accept 线程内持续运行 RunLoop 以派发 IOBluetooth 回调）
  void acceptThreadFuncBt();

  //! 非阻塞尝试接受蓝牙连接（由 RunLoop 定时器触发）
  void tryAcceptBt();

  //! RunLoop 定时器回调（静态，匹配 CFRunLoopTimerCallBack 签名）
  static void acceptTimerCb(struct __CFRunLoopTimer *timer, void *info);

  //! 发送事件到 EventQueue
  void sendEvent(deskflow::EventTypes type);

  IEventQueue *m_events;
  mutable std::mutex m_mutex;

  std::unique_ptr<BtBackend> m_backend;
  std::shared_ptr<BtDataSocket> m_pendingSocket;
  std::thread m_acceptThread;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_accepting{false};

  // macOS 蓝牙模式：RunLoop 定时器引用（CFRunLoopTimerRef，用 void* 避免头文件依赖 CoreFoundation）
  void *m_acceptTimer = nullptr;

  // 蓝牙监听参数（传递给 accept 线程）
  int m_btChannel = 10;
};
