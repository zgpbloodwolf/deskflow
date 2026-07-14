/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BtErrorTests.h"

#include "net/BtError.h"

#include <winsock2.h>

// 配对/认证类错误：对端存在但拒绝或无响应，应停止重试并提示重新配对
void BtErrorTests::pairingFailedErrors()
{
  QCOMPARE(classifyBtError(WSAEHOSTDOWN), BtErrorCategory::PairingFailed);    // 10064
  QCOMPARE(classifyBtError(WSAECONNREFUSED), BtErrorCategory::PairingFailed); // 10061
  QCOMPARE(classifyBtError(WSAEACCES), BtErrorCategory::PairingFailed);       // 10013
}

// 本机蓝牙栈/无线电不可用：应长退避继续重试
void BtErrorTests::stackUnavailableErrors()
{
  QCOMPARE(classifyBtError(WSAENETUNREACH), BtErrorCategory::StackUnavailable); // 10051
  QCOMPARE(classifyBtError(WSAENETDOWN), BtErrorCategory::StackUnavailable);    // 10050
  QCOMPARE(classifyBtError(WSAENETRESET), BtErrorCategory::StackUnavailable);   // 10052
  QCOMPARE(classifyBtError(WSAESHUTDOWN), BtErrorCategory::StackUnavailable);   // 10058
}

// 可重试的临时错误：正常阶梯退避
void BtErrorTests::retryableErrors()
{
  QCOMPARE(classifyBtError(WSAETIMEDOUT), BtErrorCategory::Retryable);    // 10060
  QCOMPARE(classifyBtError(WSAECONNABORTED), BtErrorCategory::Retryable); // 10053
  QCOMPARE(classifyBtError(WSAECONNRESET), BtErrorCategory::Retryable);   // 10054
  QCOMPARE(classifyBtError(WSAEHOSTUNREACH), BtErrorCategory::Retryable); // 10065
}

// 未归类错误：默认 Unknown（按可重试处理）
void BtErrorTests::unknownErrors()
{
  QCOMPARE(classifyBtError(0), BtErrorCategory::Unknown);
  QCOMPARE(classifyBtError(99999), BtErrorCategory::Unknown);
  QCOMPARE(classifyBtError(WSAEWOULDBLOCK), BtErrorCategory::Unknown); // 未归类的常见码
}

QTEST_MAIN(BtErrorTests)
