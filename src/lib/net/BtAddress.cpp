/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/BtAddress.h"

#include <algorithm>
#include <cctype>
#include <sstream>

BtAddress::BtAddress(const std::string &address, int channel) : m_address(address), m_channel(channel)
{
}

BtAddress BtAddress::parse(const std::string &address, int channel)
{
  return BtAddress(address, channel);
}

bool BtAddress::isValidAddress(const std::string &address)
{
  // 格式：XX:XX:XX:XX:XX:XX，共 17 个字符
  if (address.length() != 17) {
    return false;
  }

  for (int i = 0; i < 17; ++i) {
    if (i % 3 == 2) {
      // 位置 2, 5, 8, 11, 14 应该是冒号分隔符
      if (address[i] != ':') {
        return false;
      }
    } else {
      // 其他位置应该是十六进制字符
      if (!std::isxdigit(static_cast<unsigned char>(address[i]))) {
        return false;
      }
    }
  }
  return true;
}

bool BtAddress::isValidChannel(int channel)
{
  return channel >= kMinChannel && channel <= kMaxChannel;
}

bool BtAddress::isValid() const
{
  return isValidAddress(m_address) && isValidChannel(m_channel);
}

std::string BtAddress::toString() const
{
  return m_address + " 通道 " + std::to_string(m_channel);
}

bool BtAddress::operator==(const BtAddress &other) const
{
  return m_address == other.m_address && m_channel == other.m_channel;
}
