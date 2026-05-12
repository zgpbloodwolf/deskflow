/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/ISocketFactory.h"

class IEventQueue;

//! Asio 套接字工厂
/*!
创建基于 Standalone Asio 的 TCP socket 实例。
不需要 SocketMultiplexer，每个 socket 拥有独立的 io_context。
*/
class AsioTCPSocketFactory : public ISocketFactory
{
public:
  explicit AsioTCPSocketFactory(IEventQueue *events);
  ~AsioTCPSocketFactory() override = default;

  // ISocketFactory 接口实现
  IDataSocket *create(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet,
      SecurityLevel securityLevel = SecurityLevel::PlainText
  ) const override;
  IListenSocket *createListen(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet,
      SecurityLevel securityLevel = SecurityLevel::PlainText
  ) const override;

private:
  IEventQueue *m_events;
};
