/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BtDeviceDiscoveryDialog.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QMessageBox>

BtDeviceDiscoveryDialog::BtDeviceDiscoveryDialog(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("扫描蓝牙设备"));
  setMinimumSize(400, 300);

  auto *mainLayout = new QVBoxLayout(this);

  // 状态标签
  m_lblStatus = new QLabel(tr("点击\"扫描\"按钮搜索附近的蓝牙设备"), this);
  mainLayout->addWidget(m_lblStatus);

  // 设备列表
  m_deviceList = new QListWidget(this);
  m_deviceList->setSelectionMode(QAbstractItemView::SingleSelection);
  mainLayout->addWidget(m_deviceList);

  // 扫描按钮
  auto *btnLayout = new QHBoxLayout();
  m_btnScan = new QPushButton(tr("扫描"), this);
  m_btnScan->setCursor(Qt::PointingHandCursor);
  connect(m_btnScan, &QPushButton::clicked, this, &BtDeviceDiscoveryDialog::startDiscovery);
  btnLayout->addWidget(m_btnScan);

  btnLayout->addStretch();

  // 确定/取消按钮
  m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  btnLayout->addWidget(m_buttonBox);

  mainLayout->addLayout(btnLayout);

  // 初始状态：确定按钮禁用
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

  auto *discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);

  connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this,
          [this](const QBluetoothDeviceInfo &device) {
            // 只添加有名称的设备
            QString name = device.name();
            QString address = device.address().toString();
            if (name.isEmpty()) {
              name = address;
            }
            m_deviceList->addItem(QString("%1 (%2)").arg(name, address));
            m_addresses.append(address);
            m_lblStatus->setText(tr("已发现 %1 个设备...").arg(m_addresses.size()));
          });

  connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this,
          [this, discoveryAgent]() {
            m_lblStatus->setText(tr("扫描完成，共发现 %1 个设备").arg(m_addresses.size()));
            m_btnScan->setEnabled(true);
            if (!m_addresses.isEmpty()) {
              m_deviceList->setCurrentRow(0);
              m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
            }
            discoveryAgent->deleteLater();
          });

  connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
          [this, discoveryAgent](QBluetoothDeviceDiscoveryAgent::Error error) {
            m_lblStatus->setText(tr("扫描出错: %1").arg(discoveryAgent->errorString()));
            m_btnScan->setEnabled(true);
            discoveryAgent->deleteLater();
          });

  connect(m_deviceList, &QListWidget::itemSelectionChanged, this, [this]() {
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_deviceList->currentRow() >= 0);
  });

  discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
}
