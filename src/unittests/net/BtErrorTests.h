/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

//! 蓝牙错误码分类工具的单元测试（仅 Windows，classifyBtError 依赖 WSA 符号）
class BtErrorTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void pairingFailedErrors();
  void stackUnavailableErrors();
  void retryableErrors();
  void unknownErrors();
};
