/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Messages.h"

#include "Logger.h"

#include "common/Settings.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>
#include <memory>

namespace deskflow::gui::messages {

struct Errors
{
  static std::unique_ptr<QMessageBox> s_criticalMessage;
  static QStringList s_ignoredErrors;
};

std::unique_ptr<QMessageBox> Errors::s_criticalMessage;
QStringList Errors::s_ignoredErrors;

void raiseCriticalDialog()
{
  if (Errors::s_criticalMessage) {
    Errors::s_criticalMessage->raise();
  }
}

void showErrorDialog(const QString &message, const QString &fileLine, QtMsgType type)
{
  auto errorType = QtFatalMsg ? QObject::tr("致命错误") : QObject::tr("错误");
  auto title = QStringLiteral("%1 %2").arg(kAppName, errorType);
  auto text = QObject::tr(
                  R"(<p>请<a href="%1">报告问题</a>)"
                  " and copy/paste the following error:</p><pre>v%2\n%3\n%4</pre>"
  )
                  .arg(kUrlHelp, kVersion, message, fileLine);

  if (type == QtFatalMsg) {
    text.prepend(QObject::tr("<p>抱歉，发生了致命错误，应用程序必须退出。</p>\n"));
    // create a blocking message box for fatal errors, as we want to wait
    // until the dialog is dismissed before aborting the app.
    QMessageBox::critical(nullptr, title, text, QMessageBox::Abort);
    return;
  }

  text.prepend(QObject::tr("<p>抱歉，发生了严重错误。</p>\n"));
  if (!Errors::s_ignoredErrors.contains(message)) {
    // prevent message boxes piling up by deleting the last one if it exists.
    // if none exists yet, then nothing will happen.
    Errors::s_criticalMessage.reset();

    // as we don't abort for critical messages, create a new non-blocking
    // message box. this is so that we don't block the message handler; if we
    // did, we would prevent new messages from being logged properly.
    // the memory will stay allocated until the app exits, which is acceptable.
    Errors::s_criticalMessage =
        std::make_unique<QMessageBox>(QMessageBox::Critical, title, text, QMessageBox::Ok | QMessageBox::Ignore);

    QObject::connect(
        Errors::s_criticalMessage.get(), &QMessageBox::finished, //
        [message](int result) {
          if (result == QMessageBox::Ignore) {
            Errors::s_ignoredErrors.append(message);
          }
        }
    );
    Errors::s_criticalMessage->open();
  }
}

QString fileLine(const QMessageLogContext &context)
{
  if (!context.file) {
    return {};
  }
  return QStringLiteral("%1:%2").arg(context.file, QString::number(context.line));
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
  const auto fileLine = messages::fileLine(context);
  Logger::instance()->handleMessage(type, fileLine, message);

  if (type == QtFatalMsg || type == QtCriticalMsg) {
    showErrorDialog(message, fileLine, type);
  }

  if (type == QtFatalMsg) {
    // developers: if you hit this line in your debugger, traverse the stack
    // to find the cause of the fatal error. important: crash the app on fatal
    // error to prevent the app being used in a broken state.
    //
    // hint: if you don't want to crash, but still want to show an error
    // message, use `qCritical()` instead of `qFatal()`. you should use
    // fatal errors when the app is in an unrecoverable state; i.e. it cannot
    // function correctly in it's current state and must be restarted.
    abort();
  }
}

void showCloseReminder(QWidget *parent)
{
  auto message = QObject::tr(
                     "<p>%1 will continue to run in the background and can be accessed via the %1 icon in your "
                     "system notifications area. This setting can be disabled.</p>"
  )
                     .arg(kAppName);

  QMessageBox::information(parent, kAppName, message);
}

void showFirstServerStartMessage(QWidget *parent)
{
  QMessageBox::information(
      parent, QObject::tr("%1 服务器").arg(kAppName),
      QObject::tr(
          "<p>很好，%1 服务器正在运行。</p>"
          "<p>现在你可以将客户端电脑连接到此服务器。当新客户端尝试连接时，服务器上会弹出提示。</p>"
      )
          .arg(kAppName)
  );
}

void showFirstConnectedMessage(QWidget *parent)
{
  auto message = QObject::tr("<p>%1 已连接！</p>").arg(kAppName);

  if (Settings::value(Settings::Core::CoreMode).value<Settings::CoreMode>() == Settings::Server) {
    message.append(
        QObject::tr(
            "<p>试试将鼠标移到另一台电脑上，然后输入一些内容。</p>\n"            "<p>别忘了，你还可以在电脑之间复制粘贴。</p>"
        )
    );
  } else {
    message.append(QObject::tr("<p>试试远程控制这台电脑。</p>"));
  }

  using ProcessMode = Settings::ProcessMode;

  if (Settings::value(Settings::Core::ProcessMode).value<ProcessMode>() == ProcessMode::Desktop &&
      !Settings::value(Settings::Gui::CloseToTray).toBool()) {
    message.append(
        QObject::tr(
            "<p>由于你没有开启 %1 后台运行设置， "
            "需要保持此窗口打开或最小化 "
            "才能让 %1 继续运行。</p>"
        )
            .arg(kAppName)
    );
  } else {
    message.append(
        QObject::tr(
            "<p>你现在可以关闭此窗口，%1 将继续在后台运行。 "
            "此设置可以关闭。</p>"
        )
            .arg(kAppName)
    );
  }

  const auto title = QObject::tr("%1 已连接").arg(kAppName);
  QMessageBox::information(parent, title, message);
}

bool showNewClientPrompt(QWidget *parent, const QString &clientName)
{
  QMessageBox message(parent);
  message.addButton(QObject::tr("忽略"), QMessageBox::RejectRole);
  message.addButton(QObject::tr("添加客户端"), QMessageBox::AcceptRole);
  message.setText(QObject::tr("一个名为 '%1' 的新客户端想要连接").arg(clientName));
  message.exec();
  return message.buttonRole(message.clickedButton()) == QMessageBox::AcceptRole;
}

bool showClearSettings(QWidget *parent)
{
  const auto title = QObject::tr("清除 %1 设置").arg(kAppName);
  const auto message = QObject::tr(
                           "<p>确定要清除所有设置并重启 %1 吗？</p>"
                           "<p>此操作无法撤销。</p>"
  )
                           .arg(kAppName);
  return QMessageBox::question(parent, title, message) == QMessageBox::Yes;
}

void showReadOnlySettings(QWidget *parent, const QString &systemSettingsPath)
{
  const auto title = QObject::tr("%1 设置为只读").arg(kAppName);
  const auto message = QObject::tr(
                           "<p>设置为只读，因为你只有文件的读取权限： "
                           "</p><p>%1</p>"
  )
                           .arg(QDir::toNativeSeparators(systemSettingsPath));
  QMessageBox::information(parent, title, message);
}

bool showUpdateCheckOption(QWidget *parent)
{
  QMessageBox message(parent);
  message.addButton(QObject::tr("不用了"), QMessageBox::RejectRole);
  const auto checkButton = message.addButton(QObject::tr("检查更新"), QMessageBox::AcceptRole);
  message.setText(
      QObject::tr(
          "<p>是否要在 %1 启动时检查更新？</p>"
          "<p>检查更新需要网络连接。</p>"
          "<p>地址：<pre>%2</pre></p>"
      )
          .arg(kAppName, Settings::value(Settings::Gui::UpdateCheckUrl).toString())
  );

  message.exec();
  return message.clickedButton() == checkButton;
}

bool showDaemonOffline(QWidget *parent)
{
  QMessageBox message(parent);
  message.setIcon(QMessageBox::Warning);
  message.setWindowTitle(QObject::tr("后台服务离线"));

  message.addButton(QObject::tr("重试"), QMessageBox::AcceptRole);
  const auto ignore = message.addButton(QObject::tr("忽略"), QMessageBox::RejectRole);
  const auto disable = message.addButton(QObject::tr("禁用"), QMessageBox::NoRole);

  message.setText(
      QObject::tr(
          "<p>找不到 %1 后台服务（守护进程）。</p>"
          "<p>后台服务使 %1 能在 UAC 提示和登录界面下工作。</p>"
          "<p>如果你不想使用后台服务并手动停止了它，可以禁用此功能。</p> "
          ""
          "<p>如果你没有手动停止后台服务，可能存在问题。请重试或从 Windows 服务管理器中重启 %1 服务。</p> "
          ""
      )
          .arg(kAppName)
  );
  message.exec();

  if (message.clickedButton() == ignore) {
    return false;
  } else if (message.clickedButton() == disable) {
    Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);
    return false;
  }

  return true;
}

} // namespace deskflow::gui::messages
