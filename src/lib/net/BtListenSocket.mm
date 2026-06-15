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

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#endif

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
  LOG_INFO("蓝牙监听 socket：将在通道 %d 上监听", channel);

  // 保存通道号，由 accept 线程内部调用 listen()
  // 这样 IOBluetooth 通知注册和回调派发都在 accept 线程的 RunLoop 上
  m_btChannel = channel;

  // 启动接受线程（线程内部创建 backend 并监听）
  m_running = true;
  m_acceptThread = std::thread(&BtListenSocket::acceptThreadFuncBt, this);
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

void BtListenSocket::acceptThreadFuncBt()
{
  LOG_DEBUG("蓝牙接受线程启动（含 NSRunLoop）");

#ifdef __APPLE__
  // macOS：在 accept 线程内创建 backend 并注册通知
  // IOBluetooth 通知回调依赖注册线程的 RunLoop 派发
  // 将 listen() 放在此线程确保回调能正确触发
  m_backend = createBtBackend();
  m_backend->listen(m_btChannel);

  if (!m_backend->isListening()) {
    LOG_ERR("蓝牙监听 socket：后端监听失败");
    return;
  }

  LOG_INFO("蓝牙监听 socket：已在通道 %d 上监听", m_btChannel);

  while (m_running) {
    // 使用 NSRunLoop 派发 IOBluetooth 回调（必须在注册通知的同一线程）
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

    auto clientBackend = m_backend->accept();
    if (clientBackend && clientBackend->isConnected()) {
      LOG_INFO("蓝牙监听 socket：已接受新连接");

      auto socket = std::make_shared<BtDataSocket>(m_events, std::move(clientBackend));

      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingSocket = socket;
      }

      sendEvent(EventTypes::ListenSocketConnecting);
    }
  }
#else
  // 非 macOS 平台：使用原有的 acceptThreadFunc 逻辑
  // Windows 等平台不需要 NSRunLoop，直接在主线程创建 backend
  m_backend = createBtBackend();
  m_backend->listen(m_btChannel);

  if (!m_backend->isListening()) {
    LOG_ERR("蓝牙监听 socket：后端监听失败");
    return;
  }

  acceptThreadFunc();
#endif

  LOG_DEBUG("蓝牙接受线程退出");
}

void BtListenSocket::sendEvent(EventTypes type)
{
  m_events->addEvent(Event(type, getEventTarget()));
}
