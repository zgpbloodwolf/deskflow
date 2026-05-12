/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// 占位实现文件 -- Task 2 将填充完整实现
#include "net/AsioTCPListenSocket.h"

#include "base/IEventQueue.h"

AsioTCPListenSocket::AsioTCPListenSocket(IEventQueue *events)
    : m_acceptor(m_ioContext), m_workGuard(m_ioContext.get_executor()), m_events(events)
{
}

AsioTCPListenSocket::~AsioTCPListenSocket()
{
  close();
  m_ioContext.stop();
  if (m_ioThread.joinable()) {
    m_ioThread.join();
  }
}

void AsioTCPListenSocket::bind(const NetworkAddress &) {}

void AsioTCPListenSocket::close()
{
  std::error_code ec;
  m_acceptor.close(ec);
}

void *AsioTCPListenSocket::getEventTarget() const { return const_cast<void *>(static_cast<const void *>(this)); }

std::unique_ptr<IDataSocket> AsioTCPListenSocket::accept() { return nullptr; }

void AsioTCPListenSocket::startAsyncAccept() {}
