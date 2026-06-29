/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Logger.h"
#include "common/Settings.h"

#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QTime>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

namespace deskflow::gui {

const auto kForceDebugMessages = QStringList{
    QStringLiteral("Retrying to obtain clipboard."), QStringLiteral("Unable to obtain clipboard.")
};

QString printLine(FILE *out, const QString &type, const QString &message, const QString &fileLine = {})
{
  const auto datetime = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
  auto logLine = QStringLiteral("[%1] %2: %3").arg(datetime, type, message);

  if (!fileLine.isEmpty()) {
    logLine.append(QStringLiteral("\n\t%1").arg(fileLine));
  }

  // We must return a non-terminated log line, but before returning,
  // stdout/stderr and Windows debug output all expect a terminated line.
  const auto terminatedLogLine = QStringLiteral("%1\n").arg(logLine);

#if defined(Q_OS_WIN)
  // Debug output is viewable using either VS Code, Visual Studio, DebugView, or
  // DbgView++ (only one can be used at once). It's important to send output to
  // the debug output API, because it's difficult to view stdout and stderr from
  // a Windows GUI app.
  OutputDebugStringA(terminatedLogLine.toLocal8Bit().constData());
#else
  QTextStream(out) << terminatedLogLine;
#endif

  return logLine;
}

void Logger::handleMessage(const QtMsgType type, const QString &fileLine, const QString &message)
{
  // 重入保护：emit newLine 后下游 GUI 槽（QPlainTextEdit::appendPlainText）在
  // 内部布局时会再次触发 Qt 自身的 qWarning（例如
  // "QTextLayout::beginLayout: Called while already doing layout"），这些 qWarning
  // 会再次进入 messageHandler → handleMessage → emit newLine → appendPlainText，
  // 形成无限递归直至 Qt 内部状态损坏、GUI 闪退。
  // 重入时仅执行 printLine（输出到 stderr/OutputDebugString），跳过 emit，
  // 从而打断递归链路；m_handlingMessage 由最外层调用负责清理。
  if (m_handlingMessage) {
    auto mutatedType = type;
    if (kForceDebugMessages.contains(message)) {
      mutatedType = QtDebugMsg;
    }
    switch (mutatedType) {
    case QtDebugMsg:
      if (!m_guiDebug)
        return;
      printLine(stdout, QStringLiteral("DEBUG"), message, fileLine);
      break;
    case QtInfoMsg:
      printLine(stdout, QStringLiteral("INFO"), message, fileLine);
      break;
    case QtWarningMsg:
      printLine(stderr, QStringLiteral("WARNING"), message, fileLine);
      break;
    case QtCriticalMsg:
      printLine(stderr, QStringLiteral("CRITICAL"), message, fileLine);
      break;
    case QtFatalMsg:
      printLine(stderr, QStringLiteral("FATAL"), message, fileLine);
      break;
    }
    return;
  }

  m_handlingMessage = true;

  auto mutatedType = type;
  if (kForceDebugMessages.contains(message)) {
    mutatedType = QtDebugMsg;
  }

  QString typeString;
  auto out = stdout;
  switch (mutatedType) {
  case QtDebugMsg:
    if (!m_guiDebug) {
      m_handlingMessage = false;
      return;
    }
    typeString = "DEBUG";
    break;
  case QtInfoMsg:
    typeString = "INFO";
    break;
  case QtWarningMsg:
    typeString = "WARNING";
    out = stderr;
    break;
  case QtCriticalMsg:
    typeString = "CRITICAL";
    out = stderr;
    break;
  case QtFatalMsg:
    typeString = "FATAL";
    out = stderr;
    break;
  }

  const auto logLine = printLine(out, typeString, message, fileLine);
  Q_EMIT newLine(logLine);
  m_handlingMessage = false;
}

Logger::Logger()
{
  m_guiDebug = Settings::value(Settings::Log::GuiDebug).toBool();
  connect(Settings::instance(), &Settings::settingsChanged, this, &Logger::settingChanged);
}

Logger::~Logger()
{
  disconnect(Settings::instance(), &Settings::settingsChanged, this, &Logger::settingChanged);
}

void Logger::settingChanged(const QString &key)
{
  if (key != Settings::Log::GuiDebug)
    return;
  m_guiDebug = Settings::value(Settings::Log::GuiDebug).toBool();
}

} // namespace deskflow::gui
