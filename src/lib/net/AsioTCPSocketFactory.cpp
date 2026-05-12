/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// 占位实现文件 -- Task 2 将填充完整实现
#include "net/AsioTCPSocketFactory.h"
#include "net/AsioTCPListenSocket.h"
#include "net/AsioTCPSocket.h"

AsioTCPSocketFactory::AsioTCPSocketFactory(IEventQueue *events) : m_events(events)
{
}

IDataSocket *AsioTCPSocketFactory::create(IArchNetwork::AddressFamily, SecurityLevel) const
{
  return new AsioTCPSocket(m_events);
}

IListenSocket *AsioTCPSocketFactory::createListen(IArchNetwork::AddressFamily, SecurityLevel) const
{
  return new AsioTCPListenSocket(m_events);
}
