/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusBar.h"
#include "common/Constants.h"
#include "common/Settings.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

StatusBar::StatusBar(QWidget *parent)
    : QStatusBar{parent},
      m_lblStatus{new QLabel(this)},
      m_btnUpdate{new QPushButton(this)},
      m_retryTimer{new QTimer(this)}
{
  static const auto btnHeight = height() - 2;
  static const auto btnSize = QSize(btnHeight, btnHeight);
  static const auto iconSize = QSize(fontMetrics().height() + 2, fontMetrics().height() + 2);

  m_lblStatus->setText(tr("%1 未运行").arg(kAppName));
  insertPermanentWidget(0, m_lblStatus, 1);

  m_btnUpdate->setVisible(false);
  m_btnUpdate->setFlat(true);
  m_btnUpdate->setLayoutDirection(Qt::RightToLeft);
  m_btnUpdate->setIcon(QIcon::fromTheme(QStringLiteral("software-updates-release")));
  m_btnUpdate->setFixedHeight(btnHeight);
  m_btnUpdate->setIconSize(iconSize);
  insertPermanentWidget(1, m_btnUpdate);
  connect(m_btnUpdate, &QPushButton::clicked, this, &StatusBar::requestUpdateVersion);

  m_retryTimer->setInterval(1000);
  m_retryTimer->setSingleShot(false);
  connect(m_retryTimer, &QTimer::timeout, this, &StatusBar::updateTimerLabel);

  updateText();
  adjustSize();
}

// clang-format off
void StatusBar::setStatus(ConnectionState connectionState, ProcessState processState, bool isServer)
{
  if (m_retryTimer->isActive())
    m_retryTimer->stop();
  switch (processState) {
    using enum ProcessState;
    case Starting:
      m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 正在启动...").arg(kAppName));
      break;

    case RetryPending:
      m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 稍后将重试...").arg(kAppName));
      break;

    case Stopping:
        m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 正在停止...").arg(kAppName));
      break;

    case Stopped:
      m_connectionInterval = -1;
      m_lblStatus->setText(tr("%1 未运行").arg(kAppName));
      break;

    case Started: {
      switch (connectionState) {
        using enum ConnectionState;

        case Listening: {
          if (isServer) {
            m_lblStatus->setText(tr("%1 正在等待客户端连接").arg(kAppName));
          }
          break;
        }

        case Connecting:
          updateTimerLabel();
          if (Settings::value(Settings::Client::DynamicConnectionRetry).toBool())
            m_retryTimer->start();
          break;

        case Connected: {
          m_connectionInterval = -1;
          if (!isServer) {
            m_lblStatus->setText(tr("%1 已作为客户端连接到 %2")
                                     .arg(kAppName, Settings::value(Settings::Client::RemoteHost).toString()));
          }
          break;
        }

        case Disconnected:
          m_lblStatus->setText(tr("%1 已断开连接").arg(kAppName));
          m_connectionInterval = -1;
          break;
      }
    }
  }
}
// clang-format on
void StatusBar::setServerClients(const QStringList &clients)
{
  if (clients.isEmpty()) {
    m_lblStatus->setText(tr("%1 正在等待客户端连接").arg(kAppName));
    m_lblStatus->setToolTip("");
    return;
  }
  const auto clientCount = static_cast<int>(clients.size());
  static const auto comma = QStringLiteral(", ");
  static const auto newLine = QStringLiteral("\n");
  const auto text = tr("%1 已连接，共有 %n 个客户端：%2", "", clientCount).arg(kAppName, clients.join(comma));
  m_lblStatus->setText(text);

  const auto toolTipString = clientCount == 1 ? "" : tr("客户端：\n %1").arg(clients.join(newLine));
  m_lblStatus->setToolTip(toolTipString);
}

void StatusBar::setConnectionInterval(int newInterval)
{
  m_connectionInterval = newInterval;
}

void StatusBar::updateFound(const QString &version)
{
  m_btnUpdate->setVisible(true);
  m_btnUpdate->setToolTip(tr("发现新版本 v%1").arg(version));
}

void StatusBar::changeEvent(QEvent *e)
{
  QStatusBar::changeEvent(e);
  if (e->type() == QEvent::LanguageChange)
    updateText();
}

void StatusBar::updateText()
{
  m_btnUpdate->setText(tr("有更新可用"));
}

void StatusBar::updateTimerLabel()
{
  QString text;
  if (m_connectionInterval < 2 || !Settings::value(Settings::Client::DynamicConnectionRetry).toBool()) {
    text = tr("%1 正在连接...").arg(kAppName);
  } else {
    text = tr("%1 将在 %2 秒后重试").arg(kAppName, QString::number(m_connectionInterval));
    m_connectionInterval--;
  }
  m_lblStatus->setText(text);
}

