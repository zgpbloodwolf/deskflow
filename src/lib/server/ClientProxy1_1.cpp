/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_1.h"

#include "base/Log.h"
#include "deskflow/ProtocolUtil.h"
#include "net/AsioTCPSocket.h"
#include "net/InputEventBuffer.h"

#include <cstring>

//
// ClientProxy1_1
//

ClientProxy1_1::ClientProxy1_1(const std::string &name, deskflow::IStream *stream, IEventQueue *events)
    : ClientProxy1_0(name, stream, events)
{
  // do nothing
}

void ClientProxy1_1::keyDown(KeyID key, KeyModifierMask mask, KeyButton button, const std::string &)
{
  LOG_DEBUG1("send key down to \"%s\" id=%d, mask=0x%04x, button=0x%04x", getName().c_str(), key, mask, button);

  // 通过 SPSC 缓冲区路由键盘事件 (D-05)
  // CR-01 修复：检查 push 返回值，队列满时回退到直接发送
  auto *asioSocket = dynamic_cast<AsioTCPSocket *>(getStream());
  if (asioSocket) {
    KeyboardEvent evt;
    evt.type = KeyboardEvent::Type::Down;
    evt.key = key;
    evt.mask = mask;
    evt.button = button;
    if (!asioSocket->eventBuffer().pushKeyboardEvent(evt)) {
      LOG_WARN("keyboard FIFO full, falling back to direct send");
      ProtocolUtil::writef(getStream(), kMsgDKeyDown, key, mask, button);
    }
  } else {
    ProtocolUtil::writef(getStream(), kMsgDKeyDown, key, mask, button);
  }
}

void ClientProxy1_1::keyRepeat(
    KeyID key, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang
)
{
  LOG(
      (CLOG_DEBUG1 "send key repeat to \"%s\" id=%d, mask=0x%04x, count=%d, "
                   "button=0x%04x, lang=\"%s\"",
       getName().c_str(), key, mask, count, button, lang.c_str())
  );

  // 通过 SPSC 缓冲区路由键盘事件 (D-05)
  // CR-01 修复：检查 push 返回值，队列满时回退到直接发送
  auto *asioSocket = dynamic_cast<AsioTCPSocket *>(getStream());
  if (asioSocket) {
    KeyboardEvent evt;
    evt.type = KeyboardEvent::Type::Repeat;
    evt.key = key;
    evt.mask = mask;
    evt.button = button;
    // WR-04 修复：将 std::string 复制到固定大小 char 数组
    strncpy(evt.language, lang.c_str(), sizeof(evt.language) - 1);
    evt.language[sizeof(evt.language) - 1] = '\0';
    if (!asioSocket->eventBuffer().pushKeyboardEvent(evt)) {
      LOG_WARN("keyboard FIFO full, falling back to direct send");
      ProtocolUtil::writef(getStream(), kMsgDKeyRepeat, key, mask, count, button, &lang);
    }
  } else {
    ProtocolUtil::writef(getStream(), kMsgDKeyRepeat, key, mask, count, button, &lang);
  }
}

void ClientProxy1_1::keyUp(KeyID key, KeyModifierMask mask, KeyButton button)
{
  LOG_DEBUG1("send key up to \"%s\" id=%d, mask=0x%04x, button=0x%04x", getName().c_str(), key, mask, button);

  // 通过 SPSC 缓冲区路由键盘事件 (D-05)
  // CR-01 修复：检查 push 返回值，队列满时回退到直接发送
  auto *asioSocket = dynamic_cast<AsioTCPSocket *>(getStream());
  if (asioSocket) {
    KeyboardEvent evt;
    evt.type = KeyboardEvent::Type::Up;
    evt.key = key;
    evt.mask = mask;
    evt.button = button;
    if (!asioSocket->eventBuffer().pushKeyboardEvent(evt)) {
      LOG_WARN("keyboard FIFO full, falling back to direct send");
      ProtocolUtil::writef(getStream(), kMsgDKeyUp, key, mask, button);
    }
  } else {
    ProtocolUtil::writef(getStream(), kMsgDKeyUp, key, mask, button);
  }
}
