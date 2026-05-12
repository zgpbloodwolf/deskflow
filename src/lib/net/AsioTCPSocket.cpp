/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

// 占位实现文件 -- Task 2 将填充完整实现
#include "net/AsioTCPSocket.h"

#include "base/IEventQueue.h"

AsioTCPSocket::AsioTCPSocket(IEventQueue *events)
    : IDataSocket(events), m_socket(m_ioContext), m_strand(asio::make_strand(m_ioContext)), m_workGuard(m_ioContext.get_executor()), m_events(events)
{
}

AsioTCPSocket::AsioTCPSocket(IEventQueue *events, asio::ip::tcp::socket socket)
    : IDataSocket(events), m_socket(std::move(socket)), m_strand(asio::make_strand(m_ioContext)), m_workGuard(m_ioContext.get_executor()), m_events(events)
{
}

AsioTCPSocket::~AsioTCPSocket()
{
  close();
  m_ioContext.stop();
  if (m_ioThread.joinable()) {
    m_ioThread.join();
  }
}

void AsioTCPSocket::bind(const NetworkAddress &) {}

void AsioTCPSocket::close()
{
  std::error_code ec;
  m_socket.close(ec);
  if (m_connected.exchange(false)) {
    sendEvent(EventTypes::SocketDisconnected);
  }
}

void *AsioTCPSocket::getEventTarget() const { return const_cast<void *>(static_cast<const void *>(this)); }

uint32_t AsioTCPSocket::read(void *buffer, uint32_t n) { return 0; }

void AsioTCPSocket::write(const void *buffer, uint32_t n) {}

void AsioTCPSocket::flush() {}

void AsioTCPSocket::shutdownInput()
{
  std::error_code ec;
  m_socket.shutdown(asio::ip::tcp::socket::shutdown_receive, ec);
}

void AsioTCPSocket::shutdownOutput()
{
  std::error_code ec;
  m_socket.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
}

bool AsioTCPSocket::isReady() const { return m_inputBuffer.getSize() > 0; }

bool AsioTCPSocket::isFatal() const { return false; }

uint32_t AsioTCPSocket::getSize() const { return m_inputBuffer.getSize(); }

void AsioTCPSocket::connect(const NetworkAddress &) {}

void AsioTCPSocket::startAsyncRead() {}

void AsioTCPSocket::onReadComplete(const std::error_code &, size_t) {}

void AsioTCPSocket::onWriteComplete(const std::error_code &, size_t) {}

void AsioTCPSocket::startAsyncWrite() {}

void AsioTCPSocket::sendEvent(EventTypes type) { m_events->addEvent(Event(type, getEventTarget())); }
