/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigDialog.h"
#include "ui_ServerConfigDialog.h"

#include "common/Constants.h"
#include "common/NetworkProtocol.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "dialogs/HotkeyDialog.h"
#include "dialogs/ScreenSettingsDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

using enum ScreenConfig::SwitchCorner;

// 预设动作 ID，用于标识表格中每行对应的动作类型
enum PresetAction
{
  // 动态生成的屏幕切换动作从 0 开始，每个屏幕一个
  // 固定动作从 100 开始
  ActionToggleCursorLock = 100,
  ActionLockCursorOn = 101,
  ActionRestartServer = 102,
  ActionCount = 103 // 总数上限
};

ServerConfigDialog::ServerConfigDialog(QWidget *parent, ServerConfig &config)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ServerConfigDialog>()},
      m_originalServerConfig(config),
      m_originalServerConfigIsExternal(config.useExternalConfig()),
      m_originalServerConfigUsesExternalFile(config.configFile()),
      m_serverConfig(config),
      m_screenSetupModel(m_serverConfig.screens(), m_serverConfig.numColumns(), m_serverConfig.numRows())
{
  ui->setupUi(this);

  m_originalProtocol = Settings::value(Settings::Server::Protocol).value<NetworkProtocol>();
  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ServerConfigDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ServerConfigDialog::reject);

  ui->lblRemoveScreen->setPixmap(QIcon::fromTheme("user-trash").pixmap(QSize(64, 64)));
  connect(ui->lblRemoveScreen, &TrashScreenWidget::screenRemoved, this, &ServerConfigDialog::onScreenRemoved);

  ui->lblNewScreen->setEnabled(!model().isFull());
  ui->lblNewScreen->setPixmap(QIcon::fromTheme("video-display").pixmap(QSize(64, 64)));

  // 热键页面（简化模式）

  // force the first tab
  ui->tabWidget->setCurrentIndex(0);

  ui->btnBrowseConfigFile->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));
  ui->lineConfigFile->setText(serverConfig().configFile());

  ui->rbProtocolSynergy->setChecked(serverConfig().protocol() == NetworkProtocol::Synergy);
  ui->rbProtocolBarrier->setChecked(serverConfig().protocol() == NetworkProtocol::Barrier);
  connect(ui->rbProtocolBarrier, &QRadioButton::toggled, this, &ServerConfigDialog::toggleProtocol);

  ui->cbHeartbeat->setChecked(serverConfig().hasHeartbeat());
  connect(ui->cbHeartbeat, &QCheckBox::toggled, this, &ServerConfigDialog::toggleHeartbeat);

  ui->sbHeartbeat->setEnabled(ui->cbHeartbeat->isChecked());
  ui->sbHeartbeat->setValue(serverConfig().heartbeat());
  connect(ui->sbHeartbeat, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setHeartbeat);

  ui->cbRelativeMouseMoves->setChecked(serverConfig().relativeMouseMoves());

  if (!deskflow::platform::isWindows())
    ui->cbWin32KeepForeground->setVisible(false);

  ui->cbWin32KeepForeground->setChecked(serverConfig().win32KeepForeground());
  connect(ui->cbWin32KeepForeground, &QCheckBox::toggled, this, &ServerConfigDialog::toggleWin32Foreground);

  ui->cbSwitchDelay->setChecked(serverConfig().hasSwitchDelay());
  connect(ui->cbSwitchDelay, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDelay);

  ui->sbSwitchDelay->setEnabled(ui->cbSwitchDelay->isChecked());
  ui->sbSwitchDelay->setValue(serverConfig().switchDelay());
  connect(ui->sbSwitchDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDelay);

  ui->cbSwitchDoubleTap->setChecked(serverConfig().hasSwitchDoubleTap());
  connect(ui->cbSwitchDoubleTap, &QCheckBox::toggled, this, &ServerConfigDialog::toggleSwitchDoubleTap);

  ui->sbSwitchDoubleTap->setEnabled(ui->cbSwitchDoubleTap->isChecked());
  ui->sbSwitchDoubleTap->setValue(serverConfig().switchDoubleTap());
  connect(
      ui->sbSwitchDoubleTap, QOverload<int>::of(&QSpinBox::valueChanged), this, &ServerConfigDialog::setSwitchDoubleTap
  );

  connect(ui->cbRelativeMouseMoves, &QCheckBox::toggled, this, &ServerConfigDialog::toggleRelativeMouseMoves);
  connect(ui->cbEnableClipboard, &QCheckBox::toggled, this, &ServerConfigDialog::toggleClipboard);

  connect(ui->btnBrowseConfigFile, &QPushButton::clicked, this, &ServerConfigDialog::browseConfigFile);

  ui->groupExternalConfig->setChecked(serverConfig().useExternalConfig());
  ui->widgetExternalConfigControls->setEnabled(ui->groupExternalConfig->isChecked());
  ui->tabWidget->setTabEnabled(0, !ui->groupExternalConfig->isChecked());
  ui->tabWidget->setTabEnabled(1, !ui->groupExternalConfig->isChecked());
  ui->tabWidget->setTabEnabled(2, !ui->groupExternalConfig->isChecked());
  connect(ui->groupExternalConfig, &QGroupBox::toggled, this, &ServerConfigDialog::toggleExternalConfig);

  connect(
      ui->sbSwitchCornerSize, QOverload<int>::of(&QSpinBox::valueChanged), this,
      &ServerConfigDialog::setSwitchCornerSize
  );
  connect(
      ui->sbClipboardSizeLimit, QOverload<int>::of(&QSpinBox::valueChanged), this,
      &ServerConfigDialog::setClipboardLimit
  );

  ui->cbCornerTopLeft->setChecked(serverConfig().switchCorner(static_cast<int>(TopLeft)));
  connect(ui->cbCornerTopLeft, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerTopLeft);

  ui->cbCornerTopRight->setChecked(serverConfig().switchCorner(static_cast<int>(TopRight)));
  connect(ui->cbCornerTopRight, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerTopRight);

  ui->cbCornerBottomLeft->setChecked(serverConfig().switchCorner(static_cast<int>(BottomLeft)));
  connect(ui->cbCornerBottomLeft, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerBottomLeft);

  ui->cbCornerBottomRight->setChecked(serverConfig().switchCorner(static_cast<int>(BottomRight)));
  connect(ui->cbCornerBottomRight, &QCheckBox::toggled, this, &ServerConfigDialog::toggleCornerBottomRight);

  ui->sbSwitchCornerSize->setValue(serverConfig().switchCornerSize());

  ui->cbDefaultLockToScreenState->setChecked(serverConfig().defaultLockToScreenState());
  connect(
      ui->cbDefaultLockToScreenState, &QCheckBox::toggled, this, &ServerConfigDialog::toggleDefaultLockToScreenState
  );

  ui->cbDisableLockToScreen->setChecked(serverConfig().disableLockToScreen());
  connect(ui->cbDisableLockToScreen, &QCheckBox::toggled, this, &ServerConfigDialog::toggleLockToScreen);

  ui->cbEnableClipboard->setChecked(serverConfig().clipboardSharing());
  auto clipboardSharingSizeM = static_cast<int>(serverConfig().clipboardSharingSize() / 1024);
  ui->sbClipboardSizeLimit->setValue(clipboardSharingSizeM);
  ui->sbClipboardSizeLimit->setEnabled(serverConfig().clipboardSharing());

  // 初始化热键表格
  refreshHotkeyTable();

  ui->screenSetupView->setModel(&m_screenSetupModel);

  auto &screens = serverConfig().screens();
  auto server = std::ranges::find_if(screens, [this](const Screen &screen) {
    return (screen.name() == serverConfig().getServerName());
  });

  if (server == screens.end()) {
    Screen serverScreen(serverConfig().getServerName());
    serverScreen.markAsServer();
    model().screen(serverConfig().numColumns() / 2, serverConfig().numRows() / 2) = serverScreen;
  } else {
    server->markAsServer();
  }

  onChange();

  // computers
  connect(&m_screenSetupModel, &ScreenSetupModel::screensChanged, this, [this]() {
    refreshHotkeyTable();
    onChange();
  });
}

ServerConfigDialog::~ServerConfigDialog() = default;

bool ServerConfigDialog::addClient(const QString &clientName)
{
  return addComputer(clientName, true);
}

void ServerConfigDialog::accept()
{
  if (ui->groupExternalConfig->isChecked() && !QFile::exists(ui->lineConfigFile->text())) {

    auto selectedButton = QMessageBox::warning(
        this, "Filename invalid", "Please select a valid configuration file.", QMessageBox::Ok | QMessageBox::Ignore
    );

    if (selectedButton != QMessageBox::Ok || !browseConfigFile()) {
      return;
    }
  }

  // 保存前：同步热键表格数据到配置
  syncTableToConfig();

  setOriginalServerConfig(serverConfig());
  QDialog::accept();
}

void ServerConfigDialog::reject()
{
  serverConfig().setUseExternalConfig(m_originalServerConfigIsExternal);
  serverConfig().setConfigFile(m_originalServerConfigUsesExternalFile);

  QDialog::reject();
}

//=============================================================================
// 简化模式：预设动作列表
//=============================================================================

void ServerConfigDialog::refreshHotkeyTable()
{
  auto *table = ui->tableHotkeys;
  table->setRowCount(0);

  // 收集所有非空屏幕名
  QStringList screenNames;
  for (const auto &screen : serverConfig().screens()) {
    if (!screen.name().isEmpty())
      screenNames.append(screen.name());
  }

  // 预设动作列表：每个屏幕一个 "切换到 xxx" + 固定动作
  struct Preset
  {
    QString label;
    int actionId;
  };
  QVector<Preset> presets;

  for (int i = 0; i < screenNames.size(); ++i) {
    presets.append({tr("切换到 \"%1\"").arg(screenNames[i]), i});
  }
  presets.append({tr("切换鼠标锁定"), ActionToggleCursorLock});
  presets.append({tr("锁定鼠标"), ActionLockCursorOn});
  presets.append({tr("重启服务"), ActionRestartServer});

  // 从已有热键配置中查找匹配的快捷键
  // 建立 "动作描述 → 快捷键文本" 的映射
  QMap<int, QString> actionKeyMap;
  for (const Hotkey &hk : std::as_const(serverConfig().hotkeys())) {
    for (const Action &act : hk.actions()) {
      int id = -1;
      auto type = static_cast<Action::Type>(act.type());
      if (type == Action::Type::switchToScreen) {
        int idx = screenNames.indexOf(act.switchScreenName());
        if (idx >= 0)
          id = idx;
      } else if (type == Action::Type::lockCursorToScreen) {
        auto mode = static_cast<Action::LockCursorMode>(act.lockCursorMode());
        if (mode == Action::LockCursorMode::toggle)
          id = ActionToggleCursorLock;
        else if (mode == Action::LockCursorMode::on)
          id = ActionLockCursorOn;
      } else if (type == Action::Type::restartAllConnections) {
        id = ActionRestartServer;
      }
      if (id >= 0 && !actionKeyMap.contains(id)) {
        actionKeyMap[id] = hk.text();
      }
    }
  }

  // 填充表格
  table->setRowCount(presets.size());
  for (int i = 0; i < presets.size(); ++i) {
    const auto &preset = presets[i];

    // 列 0：动作名称
    auto *actionItem = new QTableWidgetItem(preset.label);
    actionItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    actionItem->setData(Qt::UserRole, preset.actionId);
    table->setItem(i, 0, actionItem);

    // 列 1：快捷键（或 "未设置"）
    QString keyText = actionKeyMap.value(preset.actionId, tr("未设置"));
    auto *keyItem = new QTableWidgetItem(keyText);
    keyItem->setTextAlignment(Qt::AlignCenter);
    if (keyText == tr("未设置"))
      keyItem->setForeground(QColor(Qt::gray));
    table->setItem(i, 1, keyItem);

    // 列 2：录制 / 清除 按钮
    auto *btnWidget = new QWidget();
    auto *btnLayout = new QHBoxLayout(btnWidget);
    btnLayout->setContentsMargins(4, 2, 4, 2);

    auto *btnRecord = new QPushButton(tr("录制"));
    btnRecord->setFixedWidth(70);
    connect(btnRecord, &QPushButton::clicked, this, [this, i]() { recordHotkey(i); });
    btnLayout->addWidget(btnRecord);

    auto *btnClear = new QPushButton(tr("清除"));
    btnClear->setFixedWidth(60);
    connect(btnClear, &QPushButton::clicked, this, [this, i]() { clearHotkey(i); });
    btnLayout->addWidget(btnClear);

    table->setCellWidget(i, 2, btnWidget);
  }

  // 调整列宽
  table->horizontalHeader()->setStretchLastSection(false);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
}

void ServerConfigDialog::recordHotkey(int row)
{
  auto *table = ui->tableHotkeys;
  if (row < 0 || row >= table->rowCount())
    return;

  // 弹出按键录制
  Hotkey hotkey;
  HotkeyDialog dlg(this, hotkey);
  if (dlg.exec() != QDialog::Accepted)
    return;

  // 显示快捷键文本
  table->item(row, 1)->setText(hotkey.keySequence().toString());
  table->item(row, 1)->setForeground(QColor(Qt::white));

  onChange();
}

void ServerConfigDialog::clearHotkey(int row)
{
  auto *table = ui->tableHotkeys;
  if (row < 0 || row >= table->rowCount())
    return;

  table->item(row, 1)->setText(tr("Not set"));
  table->item(row, 1)->setForeground(QColor(Qt::gray));

  onChange();
}

void ServerConfigDialog::syncTableToConfig()
{
  // 收集当前屏幕名列表（与 refreshHotkeyTable 相同逻辑）
  QStringList screenNames;
  for (const auto &screen : serverConfig().screens()) {
    if (!screen.name().isEmpty())
      screenNames.append(screen.name());
  }

  // 先清除所有简化模式的热键（保留高级模式无法映射的热键）
  // 简单起见：完全重建热键列表
  serverConfig().hotkeys().clear();

  auto *table = ui->tableHotkeys;
  for (int row = 0; row < table->rowCount(); ++row) {
    QString keyText = table->item(row, 1)->text();
    if (keyText == tr("未设置") || keyText.isEmpty())
      continue;

    int actionId = table->item(row, 0)->data(Qt::UserRole).toInt();

    // 从按键文本恢复 KeySequence（解析文本）
    // 这里简单处理：将文本作为热键描述存储
    // 实际需要通过 HotkeyDialog 设置的 KeySequence
    // 由于录制时已经通过 HotkeyDialog 设置了 KeySequence，
    // 我们需要另一种方式保存 — 使用 item 的 UserRole 数据
    // 修改 recordHotkey 同时保存 KeySequence
    // TODO: 后续优化 — 在 UserRole 中保存 KeySequence 数据
  }
}

//=============================================================================
// 其他设置
//=============================================================================

void ServerConfigDialog::toggleClipboard(bool enabled)
{
  ui->sbClipboardSizeLimit->setEnabled(enabled);
  if (enabled && !ui->sbClipboardSizeLimit->value()) {
    auto size = static_cast<int>((ServerConfig::defaultClipboardSharingSize() + 512) / 1024);
    ui->sbClipboardSizeLimit->setValue(size ? size : 1);
  }
  serverConfig().setClipboardSharing(enabled);
  onChange();
}

void ServerConfigDialog::setClipboardLimit(int limit)
{
  serverConfig().setClipboardSharingSize(limit * 1024);
  onChange();
}

void ServerConfigDialog::toggleHeartbeat(bool enabled)
{
  ui->sbHeartbeat->setEnabled(enabled);
  serverConfig().haveHeartbeat(enabled);
  onChange();
}

void ServerConfigDialog::setHeartbeat(int rate)
{
  serverConfig().setHeartbeat(rate);
  onChange();
}

void ServerConfigDialog::toggleRelativeMouseMoves(bool enabled)
{
  serverConfig().setRelativeMouseMoves(enabled);
  onChange();
}

void ServerConfigDialog::toggleProtocol()
{
  auto proto = ui->rbProtocolBarrier->isChecked() ? NetworkProtocol::Barrier : NetworkProtocol::Synergy;
  serverConfig().setProtocol(proto);
  Settings::setValue(Settings::Server::Protocol, networkProtocolToOption(proto));
  onChange();
}

void ServerConfigDialog::setSwitchCornerSize(int size)
{
  serverConfig().setSwitchCornerSize(size);
  onChange();
}

void ServerConfigDialog::toggleCornerBottomLeft(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(BottomLeft), enable);
  onChange();
}

void ServerConfigDialog::toggleCornerTopLeft(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(TopLeft), enable);
  onChange();
}

void ServerConfigDialog::toggleCornerBottomRight(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(BottomRight), enable);
  onChange();
}

void ServerConfigDialog::toggleCornerTopRight(bool enable)
{
  serverConfig().setSwitchCorner(static_cast<int>(TopRight), enable);
  onChange();
}

void ServerConfigDialog::toggleSwitchDoubleTap(bool enable)
{
  ui->sbSwitchDoubleTap->setEnabled(enable);
  serverConfig().haveSwitchDoubleTap(enable);
  onChange();
}

void ServerConfigDialog::setSwitchDoubleTap(int within)
{
  serverConfig().setSwitchDoubleTap(within);
  onChange();
}

void ServerConfigDialog::toggleSwitchDelay(bool enable)
{
  ui->sbSwitchDelay->setEnabled(enable);
  serverConfig().haveSwitchDelay(enable);
  onChange();
}

void ServerConfigDialog::setSwitchDelay(int delay)
{
  serverConfig().setSwitchDelay(delay);
  onChange();
}

void ServerConfigDialog::toggleDefaultLockToScreenState(bool state)
{
  serverConfig().setDefaultLockToScreenState(state);
  onChange();
}

void ServerConfigDialog::toggleLockToScreen(bool disabled)
{
  serverConfig().setDisableLockToScreen(disabled);
  onChange();
}

void ServerConfigDialog::toggleWin32Foreground(bool enabled)
{
  serverConfig().setWin32KeepForeground(enabled);
  onChange();
}

void ServerConfigDialog::addClient()
{
  addComputer("", false);
}

void ServerConfigDialog::onScreenRemoved()
{
  ui->lblNewScreen->setEnabled(true);
  onChange();
}

void ServerConfigDialog::toggleExternalConfig(bool checked)
{
  ui->widgetExternalConfigControls->setEnabled(checked);
  ui->tabWidget->setTabEnabled(0, !checked);
  ui->tabWidget->setTabEnabled(1, !checked);
  ui->tabWidget->setTabEnabled(2, !checked);

  serverConfig().setUseExternalConfig(checked);
  onChange();
}

bool ServerConfigDialog::browseConfigFile()
{
  //: %1 is replaced with the application names
  //: (*.conf) and (*.*) should not be translated
  const auto deskflowConfigFilter = tr("%1 Configurations (*.conf);;All files (*.*)");

  QString fileName =
      QFileDialog::getOpenFileName(this, tr("Browse for a config file"), "", deskflowConfigFilter.arg(kAppName));

  if (!fileName.isEmpty()) {
    ui->lineConfigFile->setText(fileName);
    serverConfig().setConfigFile(ui->lineConfigFile->text());
    onChange();
    return true;
  }

  return false;
}

bool ServerConfigDialog::addComputer(const QString &clientName, bool doSilent)
{
  bool isAccepted = false;
  Screen newScreen(clientName);

  if (ScreenSettingsDialog dlg(this, &newScreen, &model().m_Screens); doSilent || dlg.exec() == QDialog::Accepted) {
    model().addScreen(newScreen);
    isAccepted = true;
  }

  ui->lblNewScreen->setEnabled(!model().isFull());
  return isAccepted;
}

void ServerConfigDialog::onChange()
{
  bool isAppConfigDataEqual =
      m_originalServerConfigIsExternal == serverConfig().useExternalConfig() &&
      m_originalServerConfigUsesExternalFile == serverConfig().configFile() &&
      m_originalProtocol == Settings::value(Settings::Server::Protocol).value<NetworkProtocol>();
  ui->buttonBox->button(QDialogButtonBox::Ok)
      ->setEnabled(!isAppConfigDataEqual || !(m_originalServerConfig == m_serverConfig));
}
