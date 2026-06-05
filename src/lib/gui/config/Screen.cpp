/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Screen.h"
#include "config/ScreenConfig.h"

using enum ScreenConfig::Modifier;
using enum ScreenConfig::Fix;

Screen::Screen(const QString &name)
{
  setName(name);
}

void Screen::loadSettings(QSettingsProxy &settings)
{
  setName(settings.value("name").toString());

  if (name().isEmpty())
    return;

  readSettings(settings, aliases(), "alias", QString(""));
  readSettings(settings, modifiers(), "modifier", static_cast<int>(DefaultMod), static_cast<int>(NumModifiers));
  readSettings(settings, fixes(), "fix", 0, static_cast<int>(NumFixes));
}

void Screen::saveSettings(QSettingsProxy &settings) const
{
  settings.setValue("name", name());

  if (name().isEmpty())
    return;

  writeSettings(settings, aliases(), "alias");
  writeSettings(settings, modifiers(), "modifier");
  writeSettings(settings, fixes(), "fix");
}

QString Screen::screensSection() const
{
  const auto lineTemplate = QStringLiteral("\t\t%1 = %2\n");

  QString out = QStringLiteral("\t%1:\n").arg(name());
  for (int i = 0; i < modifiers().size(); i++) {
    if (modifier(i) != i)
      out.append(lineTemplate.arg(modifierName(i), modifierName(modifier(i))));
  }

  for (int i = 0; i < fixes().size(); i++)
    out.append(lineTemplate.arg(fixName(i), fixes().at(i) ? QStringLiteral("true") : QStringLiteral("false")));

  return out;
}

QString Screen::aliasesSection() const
{
  QString out;
  if (!aliases().isEmpty()) {
    out = QStringLiteral("\t%1:\n").arg(name());

    for (const QString &alias : aliases())
      out.append(QStringLiteral("\t\t%1\n").arg(alias));
  }
  return out;
}

bool Screen::operator==(const Screen &screen) const
{
  return m_Name == screen.m_Name && m_Aliases == screen.m_Aliases && m_Modifiers == screen.m_Modifiers &&
         m_Fixes == screen.m_Fixes && m_Swapped == screen.m_Swapped && m_isServer == screen.m_isServer;
}
