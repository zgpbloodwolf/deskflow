/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

namespace deskflow::gui {

class Logger : public QObject
{
  Q_OBJECT

public:
  static Logger *instance()
  {
    static Logger m;
    return &m;
  }

  void handleMessage(const QtMsgType type, const QString &fileLine, const QString &message);

Q_SIGNALS:
  void newLine(const QString &line);

private:
  explicit Logger();
  ~Logger() override;
  void settingChanged(const QString &key);
  bool m_guiDebug = false;
  // 重入保护：QPlainTextEdit::appendPlainText 等 GUI 槽在 layout 时可能再次触发
  // Qt 自身的 qWarning（如 "QTextLayout::beginLayout: Called while already doing layout"），
  // 这些 qWarning 又走 messageHandler → handleMessage → emit newLine → appendPlainText，
  // 形成无限递归直至栈/Qt 内部状态损坏。该标志用于打断递归。
  bool m_handlingMessage = false;
};

} // namespace deskflow::gui
