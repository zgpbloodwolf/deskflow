/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/BtBackend.h"

#ifdef __APPLE__

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

//! macOS 蓝牙 RFCOMM 后端
/*!
使用 IOBluetooth 框架实现蓝牙 RFCOMM 通信。
因为 IOBluetooth 是 Objective-C 框架，实现文件使用 .mm（Objective-C++）。
*/
class BtBackendMacOS : public BtBackend
{
public:
  BtBackendMacOS();
  ~BtBackendMacOS() override;

  // 禁止拷贝
  BtBackendMacOS(const BtBackendMacOS &) = delete;
  BtBackendMacOS &operator=(const BtBackendMacOS &) = delete;

  // BtBackend 接口实现
  void connect(const std::string &btAddress, int channel) override;
  void listen(int channel) override;
  std::unique_ptr<BtBackend> accept() override;
  int read(void *buf, size_t len) override;
  int write(const void *buf, size_t len) override;
  void close() override;
  bool isConnected() const override;
  bool isListening() const override;
  bool pollRead(int timeoutMs) override;

  // acceptConnection 内部使用
  explicit BtBackendMacOS(void *impl);

private:
  void *m_impl = nullptr;
  bool m_connected = false;
  bool m_listening = false;

  // RunLoop 线程：客户端模式下 IOBluetooth 的 delegate 回调
  //（rfcommChannelData: 等）需要注册线程持续运行 RunLoop 才能派发。
  // EventQueue 主线程运行的是 EventQueue::loop()（非 RunLoop），
  // 因此 connect() 在专门线程上执行并运行 RunLoop。
  std::thread m_runLoopThread;
  std::mutex m_connectMutex;
  std::condition_variable m_connectCV;
  bool m_connectDone = false;
  void *m_runLoop = nullptr;        // CFRunLoopRef，用 void* 避免头文件依赖
  void *m_runLoopSource = nullptr;  // CFRunLoopSourceRef，保活源，防止 CFRunLoopRun 立即退出
};

#endif // __APPLE__
