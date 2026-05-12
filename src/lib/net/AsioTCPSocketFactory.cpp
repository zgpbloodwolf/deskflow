/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/AsioTCPSocketFactory.h"

#include "base/Log.h"
#include "net/AsioTCPListenSocket.h"
#include "net/AsioTCPSocket.h"

//
// AsioTCPSocketFactory
//

AsioTCPSocketFactory::AsioTCPSocketFactory(IEventQueue *events, bool autoReconnect)
    : m_events(events), m_autoReconnect(autoReconnect)
{
  LOG_DEBUG("创建 Asio TCP socket factory (autoReconnect=%s)", m_autoReconnect ? "true" : "false");
}

IDataSocket *AsioTCPSocketFactory::create(
    [[maybe_unused]] IArchNetwork::AddressFamily family,
    [[maybe_unused]] SecurityLevel securityLevel
) const
{
  // Phase 1 忽略 SecurityLevel 参数（TLS 在 Phase 2 移除）
  // AddressFamily 参数暂不使用，Asio 将使用 IPv4
  auto *socket = new AsioTCPSocket(m_events);
  socket->setAutoReconnect(m_autoReconnect);
  return socket;
}

IListenSocket *AsioTCPSocketFactory::createListen(
    [[maybe_unused]] IArchNetwork::AddressFamily family,
    [[maybe_unused]] SecurityLevel securityLevel
) const
{
  return new AsioTCPListenSocket(m_events);
}
