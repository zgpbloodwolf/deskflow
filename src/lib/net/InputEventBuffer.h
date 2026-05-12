/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyTypes.h"
#include "net/SpscQueue.h"

#include <cstdint>
#include <string>

/// 键盘事件结构 -- 存储在 FIFO 队列中
struct KeyboardEvent
{
  enum class Type : uint8_t
  {
    Down,
    Up,
    Repeat
  };

  Type type = Type::Down;
  KeyID key = 0;
  KeyModifierMask mask = 0;
  KeyButton button = 0;
  std::string language; // 仅 Down 事件需要语言代码
};

/// 组合缓冲区 -- 键盘 FIFO + 鼠标原子最新位置槽
/// 生产者 (EventQueue 线程) 写入鼠标位置和键盘事件
/// 消费者 (Asio io_context 线程) 读取鼠标位置和键盘事件
/// 两者通过无锁 SPSC 缓冲区解耦（D-05）
class InputEventBuffer
{
public:
  /// 存储鼠标位置（生产者调用，EventQueue 线程）
  void storeMousePosition(int32_t x, int32_t y) noexcept;

  /// 读取最新鼠标位置（消费者调用，Asio io_context 线程）
  /// 返回 true 表示有新位置，false 表示无新数据
  bool loadMousePosition(int32_t &x, int32_t &y) noexcept;

  /// 推入键盘事件（生产者调用，EventQueue 线程）
  /// 返回 true 成功，false 队列满
  bool pushKeyboardEvent(const KeyboardEvent &event) noexcept;

  /// 弹出键盘事件（消费者调用，Asio io_context 线程）
  /// 返回 true 成功，false 队列空
  bool popKeyboardEvent(KeyboardEvent &event) noexcept;

  /// 键盘队列是否为空
  bool isKeyboardEmpty() const noexcept;

private:
  MousePositionSlot m_mouseSlot;                // 鼠标原子位置槽（覆盖旧值）
  SpscFifo<KeyboardEvent, 256> m_keyboardFifo; // 键盘 FIFO（256 容量，满足 Pitfall 6 推荐）
};
