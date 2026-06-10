/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/ISocketFactory.h"

class IEventQueue;

//! 蓝牙 socket 工厂
/*!
创建基于蓝牙 RFCOMM 的 socket 实例。
参照 AsioTCPSocketFactory 的设计模式。
*/
class BtSocketFactory : public ISocketFactory
{
public:
  explicit BtSocketFactory(IEventQueue *events, bool autoReconnect = false);
  ~BtSocketFactory() override = default;

  // ISocketFactory 接口实现
  IDataSocket *create(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet
  ) const override;
  IListenSocket *createListen(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet
  ) const override;

private:
  IEventQueue *m_events;
  bool m_autoReconnect; // 客户端启用自动重连，服务端不启用
};
