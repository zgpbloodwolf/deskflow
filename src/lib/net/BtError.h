/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

//! 蓝牙连接错误类别（跨平台）
/*!
对底层蓝牙 socket 的原始错误码（Windows 为 WSA 码，macOS 为 IOReturn）
进行归类，供上层 ClientApp 按类别决定重连策略：

- \c PairingFailed：配对/认证类失败（对端拒绝或无响应）。
  应停止自动重试，提示用户去系统蓝牙重新配对。
- \c StackUnavailable：本机蓝牙栈/无线电不可用。
  应长退避（如 30s）继续重试，等待蓝牙自恢复。
- \c Retryable：可重试的临时错误（超时、设备临时掉线等）。
  走正常阶梯退避。
- \c Unknown：未归类，按可重试处理。

分组思路参考 ArchNetworkWinsock::throwError 的 WSA→异常映射。
*/
enum class BtErrorCategory
{
  Unknown,
  PairingFailed,
  StackUnavailable,
  Retryable
};

#ifdef _WIN32
//! 把 Windows WSA 裸错误码归类为蓝牙错误类别
/*!
\param wsaErr WSAGetLastError() 返回的原始错误码
\return 归类后的错误类别（未匹配返回 Unknown）
实现见 BtBackendWindows.cpp（依赖 winsock2.h 的 WSA* 符号，故仅在 Windows 编译）。
*/
BtErrorCategory classifyBtError(int wsaErr);
#endif
