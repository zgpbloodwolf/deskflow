/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/KeyStateTable.h"

//
// KeyStateTable -- 发送端按键状态追踪表
// recordKeyDown/Up: 仅 EventQueue 线程写入（正常路径）
// releaseAll: Asio io_context 线程读取（断连时，低频）
// mutex 保护足够，无需 lock-free（RESEARCH.md Pattern 5）
//

void KeyStateTable::recordKeyDown(KeyID key, KeyModifierMask mask, KeyButton button)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_pressedKeys[key] = {mask, button};
}

void KeyStateTable::recordKeyUp(KeyID key)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_pressedKeys.erase(key);
}

std::vector<KeyReleaseInfo> KeyStateTable::releaseAll()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<KeyReleaseInfo> releases;
  releases.reserve(m_pressedKeys.size());

  for (const auto &[key, info] : m_pressedKeys) {
    releases.push_back({key, info.mask, info.button});
  }

  m_pressedKeys.clear();
  return releases;
}

bool KeyStateTable::hasPressedKeys() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return !m_pressedKeys.empty();
}
