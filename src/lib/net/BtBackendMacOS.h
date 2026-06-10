/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/BtBackend.h"

#ifdef __APPLE__

#include <string>

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
};

#endif // __APPLE__
