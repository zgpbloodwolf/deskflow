/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <string>

//! 蓝牙地址类
/*!
封装蓝牙设备 MAC 地址和 RFCOMM 通道号。
蓝牙 MAC 地址格式为 "XX:XX:XX:XX:XX:XX"（大写十六进制）。
RFCOMM 通道号范围为 1-30。
*/
class BtAddress
{
public:
  BtAddress() = default;
  BtAddress(const std::string &address, int channel = kDefaultChannel);

  //! 从字符串解析蓝牙地址（格式："AA:BB:CC:DD:EE:FF"）
  static BtAddress parse(const std::string &address, int channel = kDefaultChannel);

  //! 验证 MAC 地址格式是否合法
  static bool isValidAddress(const std::string &address);

  //! 验证通道号是否合法（1-30）
  static bool isValidChannel(int channel);

  //! 获取 MAC 地址字符串
  const std::string &address() const
  {
    return m_address;
  }

  //! 获取 RFCOMM 通道号
  int channel() const
  {
    return m_channel;
  }

  //! 地址是否有效
  bool isValid() const;

  //! 转换为字符串表示（用于日志）
  std::string toString() const;

  bool operator==(const BtAddress &other) const;
  bool operator!=(const BtAddress &other) const
  {
    return !(*this == other);
  }

  static constexpr int kDefaultChannel = 10; // 避免常用通道 1
  static constexpr int kMinChannel = 1;
  static constexpr int kMaxChannel = 30;

private:
  std::string m_address; // "AA:BB:CC:DD:EE:FF" 格式
  int m_channel = kDefaultChannel;
};
