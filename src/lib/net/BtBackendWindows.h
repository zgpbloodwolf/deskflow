/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/BtBackend.h"

#ifdef _WIN32

#include <string>

//! Windows 蓝牙 RFCOMM 后端
/*!
使用 Winsock2 AF_BTH 实现蓝牙 RFCOMM 通信。
Windows SDK 提供 ws2bth.h 头文件，无需额外依赖。
*/
class BtBackendWindows : public BtBackend
{
public:
  BtBackendWindows();
  ~BtBackendWindows() override;

  // 禁止拷贝
  BtBackendWindows(const BtBackendWindows &) = delete;
  BtBackendWindows &operator=(const BtBackendWindows &) = delete;

  // BtBackend 接口实现
  void connect(const std::string &btAddress, int channel) override;
  void listen(int channel) override;
  std::unique_ptr<BtBackend> accept() override;
  int read(void *buf, size_t len) override;
  int write(const void *buf, size_t len) override;
  void close() override;
  bool isConnected() const override;
  bool pollRead(int timeoutMs) override;

private:
  //! 从已接受的 socket 构造（服务端用）
  explicit BtBackendWindows(void *acceptedSocket);

  //! 将 "AA:BB:CC:DD:EE:FF" 格式地址转换为 Windows BTH_ADDR
  static unsigned long long parseMacAddress(const std::string &btAddress);

  void *m_socket = nullptr; // SOCKET 句柄（void* 避免头文件依赖）
  bool m_connected = false;
  bool m_listening = false;
};

#endif // _WIN32
