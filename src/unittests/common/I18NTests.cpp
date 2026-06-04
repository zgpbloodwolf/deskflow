/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "I18NTests.h"

#include "common/I18N.h"
#include "common/Settings.h"
#include <QFile>
#include <QSignalSpy>

void I18NTests::initTestCase()
{
  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();
  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void I18NTests::creationTest()
{
  QVERIFY(I18N::instance());
}

// 固定中文界面，当前语言应为 zh_CN
void I18NTests::currentLangTest()
{
  QCOMPARE(I18N::currentLanguage(), QStringLiteral("zh_CN"));
}

void I18NTests::setLangTest_invalidLang()
{
  QSignalSpy spy(I18N::instance(), &I18N::languageChanged);
  I18N::setLanguage("INVALID-LANGUAGE");
  QCOMPARE(spy.count(), 0);
  QCOMPARE(I18N::currentLanguage(), QStringLiteral("zh_CN"));
}

void I18NTests::setLangTest_currentLang()
{
  QSignalSpy spy(I18N::instance(), &I18N::languageChanged);
  I18N::setLanguage(I18N::currentLanguage());
  QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(I18NTests)
