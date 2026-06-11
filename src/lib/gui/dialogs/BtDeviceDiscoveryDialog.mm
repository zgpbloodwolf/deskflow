/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BtDeviceDiscoveryDialog.h"

#include <QProcess>
#include <QRegularExpression>

BtDeviceDiscoveryDialog::BtDeviceDiscoveryDialog(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("扫描蓝牙设备"));
  setMinimumSize(400, 300);

  auto *mainLayout = new QVBoxLayout(this);

  m_lblStatus = new QLabel(tr("点击\"扫描\"按钮搜索附近的蓝牙设备"), this);
  mainLayout->addWidget(m_lblStatus);

  m_deviceList = new QListWidget(this);
  m_deviceList->setSelectionMode(QAbstractItemView::SingleSelection);
  mainLayout->addWidget(m_deviceList);

  auto *btnLayout = new QHBoxLayout();
  m_btnScan = new QPushButton(tr("扫描"), this);
  m_btnScan->setCursor(Qt::PointingHandCursor);
  connect(m_btnScan, &QPushButton::clicked, this, &BtDeviceDiscoveryDialog::startDiscovery);
  btnLayout->addWidget(m_btnScan);

  btnLayout->addStretch();

  m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  btnLayout->addWidget(m_buttonBox);

  mainLayout->addLayout(btnLayout);

  m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

QString BtDeviceDiscoveryDialog::selectedAddress() const
{
  int row = m_deviceList->currentRow();
  if (row >= 0 && row < m_addresses.size()) {
    return m_addresses[row];
  }
  return QString();
}

void BtDeviceDiscoveryDialog::startDiscovery()
{
  m_deviceList->clear();
  m_addresses.clear();
  m_lblStatus->setText(tr("正在扫描..."));
  m_btnScan->setEnabled(false);
  m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

#ifdef __APPLE__
  // 使用 system_profiler 查询已配对的蓝牙设备（系统自带命令，不依赖 IOBluetooth API）
  QProcess proc;
  proc.start("/usr/sbin/system_profiler", {"SPBluetoothDataType"});
  if (!proc.waitForFinished(15000)) {
    m_lblStatus->setText(tr("扫描超时，请重试"));
    m_btnScan->setEnabled(true);
    return;
  }

  if (proc.exitCode() != 0) {
    m_lblStatus->setText(tr("扫描失败，退出码: %1").arg(proc.exitCode()));
    m_btnScan->setEnabled(true);
    return;
  }

  const QString output = QString::fromUtf8(proc.readAllStandardOutput());

  // 解析 system_profiler 输出格式：
  //   设备名:
  //       Address: AA:BB:CC:DD:EE:FF
  //
  // 设备名是缩进的行（后面跟冒号但不是 "Address:"），
  // Address 行在其下方，缩进更多。
  QRegularExpression addrRe("Address:\\s*([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})");

  // 按设备块分割：找到所有 "Address:" 行及其前面的设备名
  const QStringList lines = output.split('\n');
  QString lastDeviceName;
  // 跳过本机蓝牙控制器（Bluetooth Controller 块）
  bool skipController = false;

  for (const QString &line : lines) {
    QString trimmed = line.trimmed();

    // 检测是否进入 Bluetooth Controller 块（本机控制器，跳过）
    if (trimmed == "Bluetooth Controller:") {
      skipController = true;
      continue;
    }

    // 检测是否离开了 Controller 块（遇到新的非子级行）
    if (skipController && !line.startsWith("          ") && !trimmed.isEmpty()) {
      skipController = false;
    }

    if (skipController) {
      continue;
    }

    // 匹配地址行
    QRegularExpressionMatch addrMatch = addrRe.match(trimmed);
    if (addrMatch.hasMatch()) {
      QString address = addrMatch.captured(1).toUpper();
      QString displayName = lastDeviceName.isEmpty() ? address : lastDeviceName;

      if (!address.isEmpty() && m_addresses.indexOf(address) == -1) {
        m_deviceList->addItem(QString("%1 (%2)").arg(displayName, address));
        m_addresses.append(address);
      }
      lastDeviceName.clear();
      continue;
    }

    // 检测设备名行：缩进行 + 名称 + 冒号结尾（如 "BT5.0 Mouse:"）
    // 但排除 "Address:"、"Minor Type:" 等已知属性行
    if (line.startsWith("      ") && trimmed.endsWith(':') && !trimmed.contains("Address:") &&
        !trimmed.contains("Type:") && !trimmed.contains("Vendor:") && !trimmed.contains("Product:") &&
        !trimmed.contains("Firmware:") && !trimmed.contains("Services:") &&
        !trimmed.contains("Supported ") && !trimmed.contains("Transport:") &&
        !trimmed.contains("State:") && !trimmed.contains("Discoverable:") &&
        !trimmed.contains("Chipset:")) {
      lastDeviceName = trimmed.chopped(1); // 移除末尾冒号
    }
  }

  if (m_addresses.isEmpty()) {
    m_lblStatus->setText(tr("未发现已配对的蓝牙设备，请先在系统设置中配对设备。\n"
                              "也可直接在设置中手动输入设备 MAC 地址。"));
  } else {
    m_lblStatus->setText(tr("扫描完成，共发现 %1 个已配对设备").arg(m_addresses.size()));
    m_deviceList->setCurrentRow(0);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
  }
#else
  m_lblStatus->setText(tr("当前平台暂不支持扫描，请手动输入蓝牙地址"));
#endif

  m_btnScan->setEnabled(true);

  connect(m_deviceList, &QListWidget::itemSelectionChanged, this, [this]() {
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_deviceList->currentRow() >= 0);
  });
}
