/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogWidget.h"
#include "common/PlatformInfo.h"

#include <gui/Logger.h>

#include <QFont>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>

LogWidget::LogWidget(QWidget *parent) : QWidget{parent}, m_textLog{new QPlainTextEdit(this)}
{
  m_textLog->setReadOnly(true);
  m_textLog->setMaximumBlockCount(10000);
  m_textLog->setLineWrapMode(QPlainTextEdit::NoWrap);

  // 设置日志等宽字体。
  // 不要使用 QFontDatabase::systemFont(FixedFont):Windows 上该 API 返回注册表
  // HKEY_CURRENT_USER\Control Panel\Desktop\WindowMetrics 中的 FixedFont,常被解析为
  // "Fixedsys" 等位图字体;Qt 6 默认使用 DirectWrite 渲染,DirectWrite 不支持位图字体,
  // 会触发 "CreateFontFaceFromHDC() failed" 和 "QTextLayout::beginLayout: Called while
  // already doing layout",Qt 内部布局状态被污染后会段错误。
  // 这里显式指定一组矢量等宽字体作为 fallback 列表,Qt 会按顺序选用首个可用项。
  QFont monoFont;
  monoFont.setFamilies({"Consolas", "Menlo", "DejaVu Sans Mono", "Courier New"});
  monoFont.setStyleHint(QFont::Monospace);
  if (deskflow::platform::isMac()) {
    monoFont.setPixelSize(12);
  }
  m_textLog->setFont(monoFont);

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_textLog);

  setLayout(layout);

  connect(
      deskflow::gui::Logger::instance(), &deskflow::gui::Logger::newLine, m_textLog, &QPlainTextEdit::appendPlainText,
      Qt::QueuedConnection
  );
}

void LogWidget::appendLine(const QString &msg)
{
  m_textLog->appendPlainText(msg);
}

void LogWidget::findNext(const QString &text)
{
  if (text.isEmpty())
    return;

  if (!m_textLog->find(text)) {
    m_textLog->moveCursor(QTextCursor::Start);
    m_textLog->find(text);
  }
}

void LogWidget::findPrevious(const QString &text)
{
  if (text.isEmpty())
    return;

  if (!m_textLog->find(text, QTextDocument::FindBackward)) {
    m_textLog->moveCursor(QTextCursor::End);
    m_textLog->find(text, QTextDocument::FindBackward);
  }
}

void LogWidget::scrollToBottom() const
{
  auto sb = m_textLog->verticalScrollBar();
  sb->setValue(sb->maximum());
}
