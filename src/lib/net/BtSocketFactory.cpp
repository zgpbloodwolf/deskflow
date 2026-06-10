/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/BtSocketFactory.h"

#include "base/Log.h"
#include "net/BtDataSocket.h"
#include "net/BtListenSocket.h"

#include <memory>

//
// BtSocketFactory
//

BtSocketFactory::BtSocketFactory(IEventQueue *events, bool autoReconnect)
    : m_events(events), m_autoReconnect(autoReconnect)
{
  LOG_DEBUG("创建蓝牙 socket 工厂 (autoReconnect=%s)", m_autoReconnect ? "true" : "false");
}

IDataSocket *BtSocketFactory::create(
    [[maybe_unused]] IArchNetwork::AddressFamily family
) const
{
  auto socket = std::make_unique<BtDataSocket>(m_events);
  socket->setAutoReconnect(m_autoReconnect);
  return socket.release();
}

IListenSocket *BtSocketFactory::createListen(
    [[maybe_unused]] IArchNetwork::AddressFamily family
) const
{
  return std::make_unique<BtListenSocket>(m_events).release();
}
