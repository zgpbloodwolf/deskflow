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

  //! 发送事件到 EventQueue
  void sendEvent(deskflow::EventTypes type);

  IEventQueue *m_events;
  mutable std::mutex m_mutex;

  std::unique_ptr<BtBackend> m_backend;
  std::shared_ptr<BtDataSocket> m_pendingSocket;
  std::thread m_acceptThread;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_accepting{false};
};
