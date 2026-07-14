/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/BtError.h"

#include <cstddef>
#include <memory>
#include <string>

//! 蓝牙 RFCOMM 后端接口
/*!
平台无关的蓝牙 RFCOMM I/O 抽象。
各平台实现此类以提供实际的蓝牙通信能力。
*/
class BtBackend
{
public:
  virtual ~BtBackend() = default;

  //! 客户端模式：连接到远程蓝牙设备
  /*!
  \param btAddress 目标设备 MAC 地址（"AA:BB:CC:DD:EE:FF"）
  \param channel RFCOMM 通道号（1-30）
  */
  virtual void connect(const std::string &btAddress, int channel) = 0;

  //! 服务端模式：在指定通道上监听蓝牙连接
  virtual void listen(int channel) = 0;

  //! 服务端模式：接受一个传入连接，返回新的后端实例
  virtual std::unique_ptr<BtBackend> accept() = 0;

  //! 从 RFCOMM 通道读取数据
  /*!
  \param buf 接收缓冲区
  \param len 缓冲区大小
  \return 实际读取的字节数，0 表示连接关闭，-1 表示错误
  */
  virtual int read(void *buf, size_t len) = 0;

  //! 向 RFCOMM 通道写入数据
  /*!
  \param buf 发送缓冲区
  \param len 要发送的字节数
  \return 实际写入的字节数，-1 表示错误
  */
  virtual int write(const void *buf, size_t len) = 0;

  //! 关闭 RFCOMM 连接
  virtual void close() = 0;

  //! 连接是否已建立
  virtual bool isConnected() const = 0;

  //! 最近一次 connect 失败的错误类别
  /*!
  在 connect() 失败（isConnected()==false）后查询，用于上层决定重连策略。
  默认返回 Unknown；各平台实现按底层错误码置位（见 BtError.h）。
  */
  virtual BtErrorCategory lastErrorCategory() const
  {
    return BtErrorCategory::Unknown;
  }

  //! 是否正在监听（服务端模式）
  virtual bool isListening() const { return false; }

  //! 检查是否有数据可读（非阻塞轮询）
  /*!
  \param timeoutMs 超时时间（毫秒），0 表示立即返回
  \return true 表示有数据可读
  */
  virtual bool pollRead(int timeoutMs) = 0;
};

//! 蓝牙后端工厂函数（平台特定实现）
/*!
创建适合当前平台的 BtBackend 实例。
由各平台的 .cpp/.mm 文件实现。
*/
std::unique_ptr<BtBackend> createBtBackend();
