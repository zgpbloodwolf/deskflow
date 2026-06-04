/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class I18NTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void creationTest();
  void currentLangTest();
  void setLangTest_invalidLang();
  void setLangTest_currentLang();

private:
  inline static const QString m_settingsPathTemp = QStringLiteral("tmp/test");
  inline static const QString m_settingsFile = QStringLiteral("%1/Deskflow.conf").arg(m_settingsPathTemp);
  inline static const QString m_stateFile = QStringLiteral("%1/Deskflow.state").arg(m_settingsPathTemp);
};
