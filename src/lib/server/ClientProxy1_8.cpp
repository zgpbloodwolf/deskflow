/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 - 2021 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/Log.h"
#include "deskflow/KeyboardLayoutManager.h"
#include "deskflow/ProtocolUtil.h"
#include "net/AsioTCPSocket.h"
#include "net/InputEventBuffer.h"

#include <cstring>

#include "ClientProxy1_8.h"

ClientProxy1_8::ClientProxy1_8(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_7(name, adoptedStream, server, events)
{
  synchronizeLanguages();
}

void ClientProxy1_8::synchronizeLanguages() const
{
  deskflow::KeyboardLayoutManager layoutManager;
  auto localLayouts = layoutManager.getSerializedLocalLayouts();
  if (!localLayouts.empty()) {
    LOG_DEBUG1("send server languages to the client: %s", localLayouts.c_str());
    ProtocolUtil::writef(getStream(), kMsgDLanguageSynchronisation, &localLayouts);
  } else {
    LOG_ERR("failed to read server languages");
  }
}

void ClientProxy1_8::keyDown(KeyID key, KeyModifierMask mask, KeyButton button, const std::string &language)
{
  LOG(
      (CLOG_DEBUG1 "send key down to \"%s\" id=%d, mask=0x%04x, button=0x%04x, layout=%s", getName().c_str(), key, mask,
       button, language.c_str())
  );

  // 通过 SPSC 缓冲区路由键盘事件 (D-05)
  // CR-01 修复：检查 push 返回值，队列满时回退到直接发送
  auto *asioSocket = dynamic_cast<AsioTCPSocket *>(getStream());
  if (asioSocket) {
    KeyboardEvent evt;
    evt.type = KeyboardEvent::Type::Down;
    evt.key = key;
    evt.mask = mask;
    evt.button = button;
    // WR-04 修复：将 std::string 复制到固定大小 char 数组
    strncpy(evt.language, language.c_str(), sizeof(evt.language) - 1);
    evt.language[sizeof(evt.language) - 1] = '\0';
    if (!asioSocket->eventBuffer().pushKeyboardEvent(evt)) {
      LOG_WARN("keyboard FIFO full, falling back to direct send");
      ProtocolUtil::writef(getStream(), kMsgDKeyDownLang, key, mask, button, &language);
    }
  } else {
    ProtocolUtil::writef(getStream(), kMsgDKeyDownLang, key, mask, button, &language);
  }
}
