/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyTypes.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

/// 按键释放信息 -- 断连时遍历发送 keyUp 使用
struct KeyReleaseInfo
{
  KeyID key;
  KeyModifierMask mask;
  KeyButton button;
};

/// 发送端按键状态追踪表（RESEARCH.md Pattern 5）
/// 正常路径: recordKeyDown/Up 仅从 EventQueue 线程写入
/// 断连路径: releaseAll() 从 Asio io_context 线程读取（低频，mutex 保护足够）
class KeyStateTable
{
public:
  /// 记录按键按下（EventQueue 线程调用）
  void recordKeyDown(KeyID key, KeyModifierMask mask, KeyButton button);

  /// 记录按键释放（EventQueue 线程调用）
  void recordKeyUp(KeyID key);

  /// 断连时: 遍历所有按下的键，返回释放列表，然后清空
  /// Asio io_context 线程调用（在断连回调中）
  std::vector<KeyReleaseInfo> releaseAll();

  /// 是否有按键处于按下状态
  bool hasPressedKeys() const;

private:
  struct KeyInfo
  {
    KeyModifierMask mask;
    KeyButton button;
  };

  mutable std::mutex m_mutex; // 保护: EventQueue 线程写, Asio 线程读（断连时）
  std::unordered_map<KeyID, KeyInfo> m_pressedKeys;
};
