/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsDialog.h"
#include "common/PlatformInfo.h"
#include "ui_SettingsDialog.h"

#include "common/Settings.h"
#include "gui/Messages.h"
#include "gui/core/NetworkMonitor.h"
#include "gui/dialogs/BtDeviceDiscoveryDialog.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

using namespace deskflow::gui;

SettingsDialog::SettingsDialog(QWidget *parent, const ServerConfig &serverConfig)
    : QDialog(parent),
      ui{std::make_unique<Ui::SettingsDialog>()},
      m_serverConfig(serverConfig)
{

  ui->setupUi(this);

  updateText();

  // force the first tab, since qt creator sets the active tab as the last one
  // the developer was looking at, and it's easy to accidentally save that.
  ui->tabWidget->setCurrentIndex(0);

  // Populate the list of IP addresses
  const auto validAddresses = NetworkMonitor::validAddresses();
  for (const auto &address : validAddresses) {
    QString ipString = address;
    if (ui->comboInterface->findText(ipString) == -1) {
      ui->comboInterface->addItem(ipString, ipString);
    }
  }

  if (const auto interface = Settings::value(Settings::Core::Interface).toString();
      !interface.isEmpty() && (ui->comboInterface->findData(interface) == -1)) {
    ui->comboInterface->addItem(interface, interface);
  }

  loadFromConfig();

  adjustSize();
  QApplication::processEvents();
  setFixedHeight(height());
  setWindowFlags((windowFlags() | Qt::CustomizeWindowHint) & ~Qt::WindowMinMaxButtonsHint);

  setButtonBoxEnabledButtons();
  initConnections();
}

void SettingsDialog::changeEvent(QEvent *e)
{
  QDialog::changeEvent(e);
  if (e->type() == QEvent::LanguageChange) {
    ui->retranslateUi(this);
    updateText();
  }
}

void SettingsDialog::initConnections() const
{
  connect(this, &SettingsDialog::shown, this, &SettingsDialog::showReadOnlyMessage, Qt::QueuedConnection);

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &SettingsDialog::loadFromConfig);
  connect(
      ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
      &SettingsDialog::resetToDefault
  );

  connect(ui->groupService, &QGroupBox::toggled, this, &SettingsDialog::updateControls);
  connect(ui->btnBrowseLog, &QPushButton::clicked, this, &SettingsDialog::browseLogPath);
  connect(ui->groupLogToFile, &QGroupBox::toggled, this, &SettingsDialog::setLogToFile);
  connect(ui->comboLogLevel, &QComboBox::currentIndexChanged, this, &SettingsDialog::logLevelChanged);

  // Connect modifiable controls
  connect(ui->sbPort, &QSpinBox::valueChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboLogLevel, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->comboInterface, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->rbAutoHide, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbPreventSleep, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->rbCloseToTray, &QRadioButton::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbElevateDaemon, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->cbAutoUpdate, &QCheckBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupLogToFile, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->groupService, &QGroupBox::toggled, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineLogFilename, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);

  // 蓝牙传输相关控件
  connect(ui->comboTransport, &QComboBox::currentIndexChanged, this, &SettingsDialog::transportChanged);
  connect(ui->comboTransport, &QComboBox::currentIndexChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->lineBtAddress, &QLineEdit::textChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->sbBtChannel, &QSpinBox::valueChanged, this, &SettingsDialog::setButtonBoxEnabledButtons);
  connect(ui->btnBtScan, &QPushButton::clicked, this, &SettingsDialog::browseBtDevice);
}

void SettingsDialog::browseLogPath()
{
  QString fileName =
      QFileDialog::getSaveFileName(this, tr("保存日志文件到..."), ui->lineLogFilename->text(), "Logs (*.log *.txt)");

  if (!fileName.isEmpty()) {
    ui->lineLogFilename->setText(fileName);
  }
}

void SettingsDialog::setLogToFile(bool logToFile)
{
  ui->widgetLogFilename->setEnabled(logToFile);
}

void SettingsDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  Q_EMIT shown();
}

void SettingsDialog::showReadOnlyMessage()
{
  if (Settings::isWritable())
    return;
  messages::showReadOnlySettings(this, Settings::settingsFile());
}

void SettingsDialog::updateText()
{
  // Set Tooltip for the logLevel Items
  ui->comboLogLevel->setItemData(0, tr("必要消息"), Qt::ToolTipRole);
  ui->comboLogLevel->setItemData(1, tr("非致命错误"), Qt::ToolTipRole);
  ui->comboLogLevel->setItemData(2, tr("一般警告"), Qt::ToolTipRole);
  ui->comboLogLevel->setItemData(3, tr("重要事件"), Qt::ToolTipRole);
  ui->comboLogLevel->setItemData(4, tr("一般事件 [默认]"), Qt::ToolTipRole);
  ui->comboLogLevel->setItemData(5, tr("调试条目"), Qt::ToolTipRole);
  ui->comboLogLevel->setItemData(6, tr("更多调试输出"), Qt::ToolTipRole);
  ui->comboLogLevel->setItemData(7, tr("详细调试输出"), Qt::ToolTipRole);
  ui->buttonBox->button(QDialogButtonBox::Save)->setToolTip(tr("关闭并保存更改"));
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setToolTip(tr("关闭并放弃更改"));
  ui->buttonBox->button(QDialogButtonBox::Reset)->setToolTip(tr("重置为已保存的值"));
  ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setToolTip(tr("重置为默认值"));
}

void SettingsDialog::accept()
{
  Settings::setValue(Settings::Core::Port, ui->sbPort->value());
  Settings::setValue(Settings::Core::Interface, ui->comboInterface->currentData());

  // 传输方式和蓝牙设置
  const auto isBluetooth = ui->comboTransport->currentIndex() == 1;
  Settings::setValue(Settings::Core::Transport, isBluetooth ? "bluetooth" : "tcp");
  Settings::setValue(Settings::Client::BtTargetAddress, ui->lineBtAddress->text());
  Settings::setValue(Settings::Server::BtChannel, ui->sbBtChannel->value());
  Settings::setValue(Settings::Log::Level, ui->comboLogLevel->currentIndex());
  Settings::setValue(Settings::Log::ToFile, ui->groupLogToFile->isChecked());
  Settings::setValue(Settings::Log::File, ui->lineLogFilename->text());
  Settings::setValue(Settings::Daemon::Elevate, ui->cbElevateDaemon->isChecked());
  Settings::setValue(Settings::Gui::Autohide, ui->rbAutoHide->isChecked());
  Settings::setValue(Settings::Gui::AutoUpdateCheck, ui->cbAutoUpdate->isChecked());
  Settings::setValue(Settings::Core::PreventSleep, ui->cbPreventSleep->isChecked());
  Settings::setValue(Settings::Gui::CloseToTray, ui->rbCloseToTray->isChecked());

  Settings::ProcessMode mode;
  if (ui->groupService->isChecked())
    mode = Settings::ProcessMode::Service;
  else
    mode = Settings::ProcessMode::Desktop;
  Settings::setValue(Settings::Core::ProcessMode, mode);

  QDialog::accept();
}

void SettingsDialog::loadFromConfig()
{
  ui->sbPort->setValue(Settings::value(Settings::Core::Port).toInt());
  ui->comboLogLevel->setCurrentIndex(Settings::value(Settings::Log::Level).toInt());
  ui->groupLogToFile->setChecked(Settings::value(Settings::Log::ToFile).toBool());
  ui->lineLogFilename->setText(Settings::value(Settings::Log::File).toString());
  ui->cbPreventSleep->setChecked(Settings::value(Settings::Core::PreventSleep).toBool());
  ui->cbElevateDaemon->setChecked(Settings::value(Settings::Daemon::Elevate).toBool());
  ui->cbAutoUpdate->setChecked(Settings::value(Settings::Gui::AutoUpdateCheck).toBool());

  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  ui->groupService->setChecked(processMode == Settings::ProcessMode::Service);

  if (!deskflow::platform::isWindows())
    ui->groupService->setVisible(false);

  const auto autoHide = Settings::value(Settings::Gui::Autohide).toBool();
  ui->rbAutoHide->setChecked(autoHide);
  ui->rbShowOnStart->setChecked(!autoHide);

  const auto closeToTray = Settings::value(Settings::Gui::CloseToTray).toBool();
  ui->rbCloseToTray->setChecked(closeToTray);
  ui->rbExitOnClose->setChecked(!closeToTray);

  ui->comboInterface->setCurrentText(Settings::value(Settings::Core::Interface).toString());
  if (ui->comboInterface->currentIndex() <= 0) {
    ui->comboInterface->setCurrentIndex(0);
    m_interfaceSetOnLoad = false;
  } else {
    m_interfaceSetOnLoad = true;
  }

  // 传输方式和蓝牙设置
  const auto transport = Settings::value(Settings::Core::Transport).toString();
  ui->comboTransport->setCurrentIndex(transport == "bluetooth" ? 1 : 0);
  ui->lineBtAddress->setText(Settings::value(Settings::Client::BtTargetAddress).toString());
  ui->sbBtChannel->setValue(Settings::value(Settings::Server::BtChannel).toInt());
  updateTransportVisibility();

  qDebug() << "load from config done";

  updateControls();
}

bool SettingsDialog::isClientMode() const
{
  return Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Client;
}

void SettingsDialog::updateControls()
{
  const bool writable = Settings::isWritable();
  const bool serviceChecked = ui->groupService->isChecked();
  const bool logToFile = ui->groupLogToFile->isChecked();

  ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(writable);

  ui->sbPort->setEnabled(writable);
  ui->comboInterface->setEnabled(writable);
  ui->comboLogLevel->setEnabled(writable);
  ui->groupLogToFile->setEnabled(writable);
  ui->rbAutoHide->setEnabled(writable);
  ui->rbShowOnStart->setEnabled(writable);
  ui->cbAutoUpdate->setEnabled(writable);
  ui->cbPreventSleep->setEnabled(writable);
  ui->rbCloseToTray->setEnabled(writable);
  ui->rbExitOnClose->setEnabled(writable);

  // Portable mode only ever applies to Windows.
  // Daemon options should only be available on Windows when *not* in portable mode.
  if (!Settings::isPortableMode()) {
    ui->groupService->setEnabled(writable);
    ui->cbElevateDaemon->setEnabled(writable && serviceChecked);
  } else {
    ui->groupService->setVisible(false);
  }

  ui->widgetLogFilename->setEnabled(writable && logToFile);

  // 蓝牙传输控件
  ui->comboTransport->setEnabled(writable);
  const bool isBluetooth = ui->comboTransport->currentIndex() == 1;
  ui->groupBluetooth->setEnabled(writable && isBluetooth);
}

void SettingsDialog::logLevelChanged()
{
}

bool SettingsDialog::isModified() const
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  const bool ignoreInterface = !m_interfaceSetOnLoad && (ui->comboInterface->currentIndex() == 0);

  bool modified =
      (ui->sbPort->value() != Settings::value(Settings::Core::Port).toInt()) ||
      (ui->comboLogLevel->currentIndex() != Settings::value(Settings::Log::Level).toInt()) ||
      (ui->groupLogToFile->isChecked() != Settings::value(Settings::Log::ToFile).toBool()) ||
      (ui->lineLogFilename->text() != Settings::value(Settings::Log::File).toString()) ||
      (ui->rbAutoHide->isChecked() != Settings::value(Settings::Gui::Autohide).toBool()) ||
      (ui->cbPreventSleep->isChecked() != Settings::value(Settings::Core::PreventSleep).toBool()) ||
      (ui->rbCloseToTray->isChecked() != Settings::value(Settings::Gui::CloseToTray).toBool()) ||
      (ui->cbElevateDaemon->isChecked() != Settings::value(Settings::Daemon::Elevate).toBool()) ||
      (ui->cbAutoUpdate->isChecked() != Settings::value(Settings::Gui::AutoUpdateCheck).toBool()) ||
      (ui->groupService->isChecked() != (processMode == Settings::ProcessMode::Service));

  if (!ignoreInterface)
    modified = modified || ui->comboInterface->currentText() != Settings::value(Settings::Core::Interface).toString();

  // 蓝牙传输相关字段
  const auto currentTransport = ui->comboTransport->currentIndex() == 1 ? QString("bluetooth") : QString("tcp");
  modified = modified || (currentTransport != Settings::value(Settings::Core::Transport).toString());
  modified = modified || (ui->lineBtAddress->text() != Settings::value(Settings::Client::BtTargetAddress).toString());
  modified = modified || (ui->sbBtChannel->value() != Settings::value(Settings::Server::BtChannel).toInt());

  return modified;
}

bool SettingsDialog::isDefault() const
{
  const auto processMode = Settings::defaultValue(Settings::Core::ProcessMode).value<Settings::ProcessMode>();

  return (
      (ui->sbPort->value() == Settings::defaultValue(Settings::Core::Port).toInt()) &&
      (ui->comboLogLevel->currentIndex() == Settings::defaultValue(Settings::Log::Level).toInt()) &&
      (ui->groupLogToFile->isChecked() == Settings::defaultValue(Settings::Log::ToFile).toBool()) &&
      (ui->lineLogFilename->text() == Settings::defaultValue(Settings::Log::File).toString()) &&
      (ui->rbAutoHide->isChecked() == Settings::defaultValue(Settings::Gui::Autohide).toBool()) &&
      (ui->cbPreventSleep->isChecked() == Settings::defaultValue(Settings::Core::PreventSleep).toBool()) &&
      (ui->rbCloseToTray->isChecked() == Settings::defaultValue(Settings::Gui::CloseToTray).toBool()) &&
      (ui->cbElevateDaemon->isChecked() == Settings::defaultValue(Settings::Daemon::Elevate).toBool()) &&
      (ui->cbAutoUpdate->isChecked() == Settings::defaultValue(Settings::Gui::AutoUpdateCheck).toBool()) &&
      (ui->groupService->isChecked() == (processMode == Settings::ProcessMode::Service)) &&
      (ui->comboInterface->currentIndex() == 0) &&
      (ui->comboTransport->currentIndex() == 0) && // 默认 TCP
      (ui->lineBtAddress->text().isEmpty()) &&
      (ui->sbBtChannel->value() == Settings::defaultValue(Settings::Server::BtChannel).toInt())
  );
}

void SettingsDialog::resetToDefault()
{
  ui->sbPort->setValue(Settings::defaultValue(Settings::Core::Port).toInt());
  ui->comboLogLevel->setCurrentIndex(Settings::defaultValue(Settings::Log::Level).toInt());
  ui->groupLogToFile->setChecked(Settings::defaultValue(Settings::Log::ToFile).toBool());
  ui->lineLogFilename->setText(Settings::defaultValue(Settings::Log::File).toString());
  ui->cbPreventSleep->setChecked(Settings::defaultValue(Settings::Core::PreventSleep).toBool());
  ui->cbElevateDaemon->setChecked(Settings::defaultValue(Settings::Daemon::Elevate).toBool());
  ui->cbAutoUpdate->setChecked(Settings::defaultValue(Settings::Gui::AutoUpdateCheck).toBool());

  const auto autoHide = Settings::defaultValue(Settings::Gui::Autohide).toBool();
  ui->rbCloseToTray->setChecked(autoHide);
  ui->rbExitOnClose->setChecked(!autoHide);

  const auto closeToTray = Settings::defaultValue(Settings::Gui::CloseToTray).toBool();
  ui->rbCloseToTray->setChecked(closeToTray);
  ui->rbExitOnClose->setChecked(!closeToTray);

  const auto processMode = Settings::defaultValue(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  ui->groupService->setChecked(processMode == Settings::ProcessMode::Service);

  if (!deskflow::platform::isWindows())
    ui->groupService->setVisible(false);

  ui->comboInterface->setCurrentIndex(0);

  // 蓝牙传输设置重置
  ui->comboTransport->setCurrentIndex(0); // 默认 TCP
  ui->lineBtAddress->clear();
  ui->sbBtChannel->setValue(Settings::defaultValue(Settings::Server::BtChannel).toInt());

  qDebug() << "reset to default values";
  updateControls();
  setButtonBoxEnabledButtons();
}

void SettingsDialog::setButtonBoxEnabledButtons() const
{
  const bool modified = isModified();
  ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(modified);
  ui->buttonBox->button(QDialogButtonBox::Reset)->setEnabled(modified);
  ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(!isDefault());
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::transportChanged()
{
  updateTransportVisibility();
  updateControls();
}

void SettingsDialog::updateTransportVisibility()
{
  const bool isBluetooth = ui->comboTransport->currentIndex() == 1;

  // 蓝牙模式下隐藏 TCP 相关控件，显示蓝牙控件
  ui->groupBluetooth->setVisible(isBluetooth);
  ui->lblNetworkIp->setVisible(!isBluetooth);
  ui->comboInterface->setVisible(!isBluetooth);
  ui->lblPort->setVisible(!isBluetooth);
  ui->sbPort->setVisible(!isBluetooth);
}

void SettingsDialog::browseBtDevice()
{
  BtDeviceDiscoveryDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    const auto address = dlg.selectedAddress();
    if (!address.isEmpty()) {
      ui->lineBtAddress->setText(address);
    }
  }
}
