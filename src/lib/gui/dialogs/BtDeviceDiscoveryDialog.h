/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

//! 蓝牙设备扫描对话框
/*!
扫描附近已配对的蓝牙设备，让用户选择目标设备。
返回选中设备的 MAC 地址。
*/
class BtDeviceDiscoveryDialog : public QDialog
{
  Q_OBJECT

public:
  explicit BtDeviceDiscoveryDialog(QWidget *parent = nullptr);
  ~BtDeviceDiscoveryDialog() override = default;

  //! 获取选中的蓝牙设备地址
  QString selectedAddress() const;

private:
  //! 开始扫描蓝牙设备
  void startDiscovery();

  //! 停止扫描
  void stopDiscovery();

  //! 扫描完成
  void onDiscoveryFinished();

  QListWidget *m_deviceList = nullptr;
  QPushButton *m_btnScan = nullptr;
  QLabel *m_lblStatus = nullptr;
  QDialogButtonBox *m_buttonBox = nullptr;

  //! 设备地址列表（与 m_deviceList 行号一一对应）
  QStringList m_addresses;
};
