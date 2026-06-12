/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// 仅在 Windows 平台编译
#ifdef _WIN32

#include "net/BtBackendWindows.h"
#include "base/Log.h"

// Windows 蓝牙头文件
#include <winsock2.h>
#include <ws2bth.h>
#include <bluetoothapis.h>

#include <cstring>

// Windows 平台工厂函数实现
std::unique_ptr<BtBackend> createBtBackend()
{
  return std::make_unique<BtBackendWindows>();
}

BtBackendWindows::BtBackendWindows()
{
}

BtBackendWindows::BtBackendWindows(void *acceptedSocket) : m_socket(acceptedSocket), m_connected(true)
{
  LOG_DEBUG("蓝牙后端：从已接受连接构造");
}

BtBackendWindows::~BtBackendWindows()
{
  close();
}

unsigned long long BtBackendWindows::parseMacAddress(const std::string &btAddress)
{
  unsigned long long addr = 0;
  unsigned int a, b, c, d, e, f;
  if (sscanf(btAddress.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X", &a, &b, &c, &d, &e, &f) != 6) {
    LOG_ERR("蓝牙后端：无法解析 MAC 地址: %s", btAddress.c_str());
    return 0;
  }
  // Windows BTH_ADDR 格式：高字节在前
  addr = ((unsigned long long)a << 40) | ((unsigned long long)b << 32) | ((unsigned long long)c << 24) |
         ((unsigned long long)d << 16) | ((unsigned long long)e << 8) | ((unsigned long long)f);
  return addr;
}

void BtBackendWindows::connect(const std::string &btAddress, int channel)
{
  LOG_INFO("蓝牙后端：正在连接到 %s 通道 %d", btAddress.c_str(), channel);
  LOG_INFO("蓝牙后端：m_connected=%d, m_socket=%p", m_connected, m_socket);

  SOCKET sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  if (sock == INVALID_SOCKET) {
    LOG_ERR("蓝牙后端：创建 socket 失败，错误码: %d", WSAGetLastError());
    return;
  }
  m_socket = reinterpret_cast<void *>(sock);
  LOG_INFO("蓝牙后端：创建 socket 成功，sock=%d", sock);

  SOCKADDR_BTH remoteAddr = {};
  remoteAddr.addressFamily = AF_BTH;
  remoteAddr.btAddr = parseMacAddress(btAddress);
  remoteAddr.port = channel;
  remoteAddr.serviceClassId = GUID_NULL;

  LOG_INFO("蓝牙后端：调用 connect()");
  if (::connect(sock, (SOCKADDR *)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR) {
    int err = WSAGetLastError();
    LOG_ERR("蓝牙后端：连接失败，错误码: %d", err);
    closesocket(sock);
    m_socket = nullptr;
    m_connected = false;
    LOG_INFO("蓝牙后端：连接失败后 m_connected=%d", m_connected);
    return;
  }

  m_connected = true;
  LOG_INFO("蓝牙后端：已连接到 %s", btAddress.c_str());
  LOG_INFO("蓝牙后端：连接成功后 m_connected=%d", m_connected);
}

void BtBackendWindows::listen(int channel)
{
  LOG_INFO("蓝牙后端：正在通道 %d 上监听", channel);

  SOCKET sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  if (sock == INVALID_SOCKET) {
    LOG_ERR("蓝牙后端：创建监听 socket 失败，错误码: %d", WSAGetLastError());
    return;
  }
  m_socket = reinterpret_cast<void *>(sock);

  SOCKADDR_BTH localAddr = {};
  localAddr.addressFamily = AF_BTH;
  localAddr.btAddr = 0; // 绑定到所有蓝牙适配器
  localAddr.port = channel;

  if (bind(reinterpret_cast<SOCKET>(m_socket), (SOCKADDR *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR) {
    LOG_ERR("蓝牙后端：绑定失败，错误码: %d", WSAGetLastError());
    closesocket(reinterpret_cast<SOCKET>(m_socket));
    m_socket = nullptr;
    return;
  }

  if (::listen(reinterpret_cast<SOCKET>(m_socket), 1) == SOCKET_ERROR) {
    LOG_ERR("蓝牙后端：监听失败，错误码: %d", WSAGetLastError());
    closesocket(reinterpret_cast<SOCKET>(m_socket));
    m_socket = nullptr;
    return;
  }

  m_listening = true;
  LOG_INFO("蓝牙后端：已在通道 %d 上监听", channel);
}

std::unique_ptr<BtBackend> BtBackendWindows::accept()
{
  if (!m_listening || m_socket == nullptr) {
    return nullptr;
  }

  SOCKADDR_BTH clientAddr = {};
  int clientAddrLen = sizeof(clientAddr);

  SOCKET clientSock = ::accept(reinterpret_cast<SOCKET>(m_socket), (SOCKADDR *)&clientAddr, &clientAddrLen);
  if (clientSock == INVALID_SOCKET) {
    LOG_ERR("蓝牙后端：接受连接失败，错误码: %d", WSAGetLastError());
    return nullptr;
  }

  LOG_INFO("蓝牙后端：已接受蓝牙连接");
  return std::make_unique<BtBackendWindows>(reinterpret_cast<void *>(clientSock));
}

int BtBackendWindows::read(void *buf, size_t len)
{
  if (!m_connected || m_socket == nullptr) {
    LOG_INFO("蓝牙后端：read 失败 - m_connected=%d, m_socket=%p", m_connected, m_socket);
    return -1;
  }

  LOG_INFO("蓝牙后端：read 开始读取...");
  int result = recv(reinterpret_cast<SOCKET>(m_socket), static_cast<char *>(buf), static_cast<int>(len), 0);
  if (result == SOCKET_ERROR) {
    int err = WSAGetLastError();
    // WSAEWOULDBLOCK: 非阻塞模式下无数据可读（正常）
    // WSAENOTSOCK: socket 已关闭（连接断开）
    if (err == WSAEWOULDBLOCK) {
      return 0; // 无数据可读
    }
    if (err == WSAENOTSOCK) {
      LOG_INFO("蓝牙后端：read 返回 WSAENOTSOCK，连接已关闭");
      m_connected = false;
      return -1;
    }
    LOG_ERR("蓝牙后端：读取失败，错误码: %d", err);
    m_connected = false;
    return -1;
  } else if (result == 0) {
    LOG_INFO("蓝牙后端：read 返回 0，连接已关闭");
    m_connected = false;
    return -1;
  }
  LOG_INFO("蓝牙后端：read 成功读取 %d 字节", result);
  return result;
}

int BtBackendWindows::write(const void *buf, size_t len)
{
  if (!m_connected || m_socket == nullptr) {
    return -1;
  }

  int result = send(reinterpret_cast<SOCKET>(m_socket), static_cast<const char *>(buf), static_cast<int>(len), 0);
  if (result == SOCKET_ERROR) {
    LOG_ERR("蓝牙后端：写入失败，错误码: %d", WSAGetLastError());
    m_connected = false;
    return -1;
  }
  return result;
}

void BtBackendWindows::close()
{
  if (m_socket != nullptr) {
    closesocket(reinterpret_cast<SOCKET>(m_socket));
    m_socket = nullptr;
  }
  m_connected = false;
  m_listening = false;
}

bool BtBackendWindows::isConnected() const
{
  return m_connected;
}

bool BtBackendWindows::pollRead(int timeoutMs)
{
  if (m_socket == nullptr) {
    return false;
  }

  fd_set readFds;
  FD_ZERO(&readFds);
  FD_SET(reinterpret_cast<SOCKET>(m_socket), &readFds);

  timeval tv = {};
  tv.tv_sec = timeoutMs / 1000;
  tv.tv_usec = (timeoutMs % 1000) * 1000;

  int result = select(0, &readFds, nullptr, nullptr, &tv);
  return result > 0;
}

#endif // _WIN32
