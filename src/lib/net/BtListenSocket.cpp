/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/BtListenSocket.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "common/Settings.h"
#include "net/BtDataSocket.h"

//
// BtListenSocket
//

BtListenSocket::BtListenSocket(IEventQueue *events) : m_events(events)
{
  LOG_DEBUG("蓝牙监听 socket 创建");
}

BtListenSocket::~BtListenSocket()
{
  close();

  m_running = false;
  if (m_acceptThread.joinable()) {
    m_acceptThread.join();
  }
}

void BtListenSocket::bind(const NetworkAddress &)
{
  // ClientListener::start() 会调用 bind() 传入 TCP 地址，
  // 蓝牙模式下忽略 TCP 地址，从设置中读取 RFCOMM 通道号
  const auto channel = Settings::value(Settings::Server::BtChannel).toInt();
  bindBt(channel);
}

void BtListenSocket::bindBt(int channel)
{
  LOG_INFO("蓝牙监听 socket：正在通道 %d 上监听", channel);

  m_backend = createBtBackend();
  m_backend->listen(channel);

  // 启动接受线程
  m_running = true;
  m_acceptThread = std::thread(&BtListenSocket::acceptThreadFunc, this);
}

void BtListenSocket::close()
{
  m_running = false;
  if (m_backend) {
    m_backend->close();
  }
}

void *BtListenSocket::getEventTarget() const
{
  return const_cast<void *>(static_cast<const void *>(this));
}

std::shared_ptr<IDataSocket> BtListenSocket::accept()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_pendingSocket) {
    auto socket = m_pendingSocket;
    m_pendingSocket = nullptr;
    return socket;
  }
  return nullptr;
}

void BtListenSocket::acceptThreadFunc()
{
  LOG_DEBUG("蓝牙接受线程启动");

  while (m_running) {
    if (!m_backend || !m_backend->isListening()) {
      // 等待后端就绪
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    auto clientBackend = m_backend->accept();
    if (clientBackend && clientBackend->isConnected()) {
      LOG_INFO("蓝牙监听 socket：已接受新连接");

      auto socket = std::make_shared<BtDataSocket>(m_events, std::move(clientBackend));

      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingSocket = socket;
      }

      // 通知上层有新连接
      sendEvent(EventTypes::ListenSocketConnecting);
    }

    // accept() 已包含超时等待，无需额外休眠
  }

  LOG_DEBUG("蓝牙接受线程退出");
}

void BtListenSocket::sendEvent(EventTypes type)
{
  m_events->addEvent(Event(type, getEventTarget()));
}
