/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// 仅在 Windows 平台编译
#ifdef _WIN32

#include "BtDeviceDiscoveryDialog.h"

#include <QRegularExpression>
#include <QThread>

// Windows 蓝牙 API
#include <windows.h>
#include <bluetoothapis.h>
#include <ws2bth.h>

// 链接蓝牙库
#pragma comment(lib, "Bthprops.lib")

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

  // 使用 Windows Bluetooth API 扫描已配对/已记住的蓝牙设备
  BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {};
  searchParams.dwSize = sizeof(searchParams);
  searchParams.fReturnAuthenticated = TRUE; // 已配对设备
  searchParams.fReturnRemembered = TRUE;    // 已记住设备
  searchParams.fReturnUnknown = FALSE;      // 不返回未知设备
  searchParams.fReturnConnected = TRUE;     // 已连接设备
  searchParams.fIssueInquiry = FALSE;       // 不发起新查询，只返回已知设备
  searchParams.cTimeoutMultiplier = 0;      // 无超时（不进行新查询）
  searchParams.hRadio = NULL;               // 使用所有蓝牙适配器

  BLUETOOTH_DEVICE_INFO deviceInfo = {};
  deviceInfo.dwSize = sizeof(deviceInfo);

  HANDLE hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);

  if (hFind == NULL) {
    DWORD lastErr = GetLastError();
    if (lastErr == ERROR_NO_MORE_ITEMS) {
      m_lblStatus->setText(tr("未发现已配对的蓝牙设备，请先在 Windows 蓝牙设置中配对设备。\n"
                               "也可直接在设置中手动输入设备 MAC 地址。"));
    } else {
      m_lblStatus->setText(tr("扫描失败，错误码: %1\n"
                               "请确认蓝牙适配器已启用。")
                               .arg(lastErr));
    }
    m_btnScan->setEnabled(true);
    connect(m_deviceList, &QListWidget::itemSelectionChanged, this, [this]() {
      m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_deviceList->currentRow() >= 0);
    });
    return;
  }

  // 遍历所有找到的设备
  do {
    // 将 BTH_ADDR (uint64) 转换为 MAC 地址字符串 "AA:BB:CC:DD:EE:FF"
    BTH_ADDR addr = deviceInfo.Address.ullLong;
    QString macAddr = QString("%1:%2:%3:%4:%5:%6")
                          .arg((addr >> 40) & 0xFF, 2, 16, QChar('0'))
                          .arg((addr >> 32) & 0xFF, 2, 16, QChar('0'))
                          .arg((addr >> 24) & 0xFF, 2, 16, QChar('0'))
                          .arg((addr >> 16) & 0xFF, 2, 16, QChar('0'))
                          .arg((addr >> 8) & 0xFF, 2, 16, QChar('0'))
                          .arg(addr & 0xFF, 2, 16, QChar('0'))
                          .toUpper();

    // 获取设备名称
    QString deviceName = QString::fromWCharArray(deviceInfo.szName);
    if (deviceName.isEmpty()) {
      deviceName = macAddr; // 无名称则使用地址
    }

    // 检查是否重复
    if (!macAddr.isEmpty() && m_addresses.indexOf(macAddr) == -1) {
      // 显示连接状态标记
      QString statusMark;
      if (deviceInfo.fConnected) {
        statusMark = tr(" [已连接]");
      } else if (deviceInfo.fAuthenticated) {
        statusMark = tr(" [已配对]");
      } else {
        statusMark = tr(" [已记住]");
      }

      m_deviceList->addItem(QString("%1 (%2)%3").arg(deviceName, macAddr, statusMark));
      m_addresses.append(macAddr);
    }

    // 重置结构体大小以供下一次迭代
    deviceInfo.dwSize = sizeof(deviceInfo);
  } while (BluetoothFindNextDevice(hFind, &deviceInfo));

  BluetoothFindDeviceClose(hFind);

  // 更新 UI 状态
  if (m_addresses.isEmpty()) {
    m_lblStatus->setText(tr("未发现已配对的蓝牙设备，请先在 Windows 蓝牙设置中配对设备。\n"
                             "也可直接在设置中手动输入设备 MAC 地址。"));
  } else {
    m_lblStatus->setText(tr("扫描完成，共发现 %1 个蓝牙设备").arg(m_addresses.size()));
    m_deviceList->setCurrentRow(0);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
  }

  m_btnScan->setEnabled(true);

  connect(m_deviceList, &QListWidget::itemSelectionChanged, this, [this]() {
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_deviceList->currentRow() >= 0);
  });
}

#endif // _WIN32
