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

AsioTCPSocketFactory::AsioTCPSocketFactory(IEventQueue *events) : m_events(events)
{
  LOG_DEBUG("创建 Asio TCP socket factory");
}

IDataSocket *AsioTCPSocketFactory::create(
    [[maybe_unused]] IArchNetwork::AddressFamily family,
    [[maybe_unused]] SecurityLevel securityLevel
) const
{
  // Phase 1 忽略 SecurityLevel 参数（TLS 在 Phase 2 移除）
  // AddressFamily 参数暂不使用，Asio 将使用 IPv4
  return new AsioTCPSocket(m_events);
}

IListenSocket *AsioTCPSocketFactory::createListen(
    [[maybe_unused]] IArchNetwork::AddressFamily family,
    [[maybe_unused]] SecurityLevel securityLevel
) const
{
  return new AsioTCPListenSocket(m_events);
}
