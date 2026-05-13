/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/AsioTCPSocketFactory.h"

#include "base/Log.h"
#include "net/AsioTCPListenSocket.h"
#include "net/AsioTCPSocket.h"

#include <memory>

//
// AsioTCPSocketFactory
//

AsioTCPSocketFactory::AsioTCPSocketFactory(IEventQueue *events, bool autoReconnect)
    : m_events(events), m_autoReconnect(autoReconnect)
{
  LOG_DEBUG("创建 Asio TCP socket factory (autoReconnect=%s)", m_autoReconnect ? "true" : "false");
}

IDataSocket *AsioTCPSocketFactory::create(
    [[maybe_unused]] IArchNetwork::AddressFamily family
) const
{
  // WR-06 修复：使用 make_unique + release()，异常安全
  auto socket = std::make_unique<AsioTCPSocket>(m_events);
  socket->setAutoReconnect(m_autoReconnect);
  return socket.release();
}

IListenSocket *AsioTCPSocketFactory::createListen(
    [[maybe_unused]] IArchNetwork::AddressFamily family
) const
{
  return std::make_unique<AsioTCPListenSocket>(m_events).release();
}
