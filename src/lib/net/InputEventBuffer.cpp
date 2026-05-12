/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/InputEventBuffer.h"

//
// InputEventBuffer -- 简单委托到内部 SPSC 组件
// 使用 .cpp 编译隔离，避免模板实例化扩散到其他编译单元
//

void InputEventBuffer::storeMousePosition(int32_t x, int32_t y) noexcept
{
  m_mouseSlot.store(x, y);
}

bool InputEventBuffer::loadMousePosition(int32_t &x, int32_t &y) noexcept
{
  return m_mouseSlot.load(x, y);
}

bool InputEventBuffer::pushKeyboardEvent(const KeyboardEvent &event) noexcept
{
  return m_keyboardFifo.push(event);
}

bool InputEventBuffer::popKeyboardEvent(KeyboardEvent &event) noexcept
{
  return m_keyboardFifo.pop(event);
}

bool InputEventBuffer::isKeyboardEmpty() const noexcept
{
  return m_keyboardFifo.empty();
}
