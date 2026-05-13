/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/TCPSocketFactory.h"
#include "net/TCPListenSocket.h"
#include "net/TCPSocket.h"

//
// TCPSocketFactory
//

TCPSocketFactory::TCPSocketFactory(IEventQueue *events, SocketMultiplexer *socketMultiplexer)
    : m_events(events),
      m_socketMultiplexer(socketMultiplexer)
{
  // do nothing
}

IDataSocket *TCPSocketFactory::create(IArchNetwork::AddressFamily family) const
{
  return new TCPSocket(m_events, m_socketMultiplexer, family);
}

IListenSocket *TCPSocketFactory::createListen(IArchNetwork::AddressFamily family) const
{
  return new TCPListenSocket(m_events, m_socketMultiplexer, family);
}
