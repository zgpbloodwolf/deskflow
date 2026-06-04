/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QIcon>
#include <QPalette>
#include <QStyleHints>
#include <QTextStream>

#include "common/Constants.h"

namespace deskflow::gui {

/**
 * @brief Detects dark mode in a universal manner (all Qt versions).
 * Until better platform support is added, it's more reliable to use the old way (compare text and window lightness),
 * because the newer versions in Qt 6.5+ are not always correct and some return `UnknownScheme`.
 * https://www.qt.io/blog/dark-mode-on-windows-11-with-qt-6.5
 */
inline bool isDarkMode()
{
  const QPalette defaultPalette;
  const auto text = defaultPalette.color(QPalette::WindowText);
  const auto window = defaultPalette.color(QPalette::Window);
  return text.lightness() > window.lightness();
}
/**
 * @brief get a string for the iconMode
 * @returns "dark" or "light"
 */
inline QString iconMode()
{
  return isDarkMode() ? QStringLiteral("dark") : QStringLiteral("light");
}

inline void updateIconTheme()
{
  // Sets the fallback icon path and fallback theme
  const auto themeName = QStringLiteral("%1-%2").arg(kAppId, iconMode());
  if (QIcon::themeName().isEmpty() || QIcon::themeName().startsWith(kAppId))
    QIcon::setThemeName(themeName);
  else
    QIcon::setFallbackThemeName(themeName);
  QIcon::setFallbackSearchPaths({QStringLiteral(":/icons/%1").arg(themeName)});
}

/**
 * @brief 根据当前系统主题加载对应的 QSS 样式表内容
 * @return 样式表字符串，加载失败时返回空字符串
 */
inline QString loadStyleSheet()
{
  const auto theme = isDarkMode() ? QStringLiteral("dark") : QStringLiteral("light");
  QFile file(QStringLiteral(":/themes/%1.qss").arg(theme));
  if (file.open(QFile::ReadOnly | QFile::Text)) {
    return QTextStream(&file).readAll();
  }
  return {};
}

/**
 * @brief 应用与系统主题匹配的 QSS 样式表
 * 调用 qApp->setStyleSheet() 全局生效，可在 PaletteChange 事件中重复调用以实现动态切换
 */
inline void applyTheme()
{
  qApp->setStyleSheet(loadStyleSheet());
}
} // namespace deskflow::gui
