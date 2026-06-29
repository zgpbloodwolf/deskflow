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

  // 从已接受的 socket 构造（服务端用）
  explicit BtBackendWindows(void *acceptedSocket);

  //! 将 "AA:BB:CC:DD:EE:FF" 格式地址转换为 Windows BTH_ADDR
  static unsigned long long parseMacAddress(const std::string &btAddress);

  void *m_socket = nullptr; // SOCKET 句柄（void* 避免头文件依赖）
  bool m_connected = false;
  bool m_listening = false;

  // SDP 服务记录句柄（仅监听端持有），用于注销
  void *m_sdpRecordHandle = nullptr;

private:
  // 为阻塞 recv 设置接收超时（SO_RCVTIMEO），使 read() 无数据时能定时返回而非永久阻塞
  void applyRecvTimeout();

  // 通过 WSASetService 注册 SDP 服务记录，使 Windows BT 栈视为合法 RFCOMM 服务，
  // 否则未挂载 SDP 的监听 socket 在客户端连接约 1 秒后会被本机 BT 栈主动 abort（WSAECONNABORTED）。
  bool registerSdpService(int channel);
  void unregisterSdpService();
};

#endif // _WIN32
