# SPDX-FileCopyrightText: (C) 2024 - 2025 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-FileCopyrightText: (C) 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: (C) 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Network)

add_library(net STATIC
  AsioTCPSocket.cpp
  AsioTCPSocket.h
  AsioTCPListenSocket.cpp
  AsioTCPListenSocket.h
  AsioTCPSocketFactory.cpp
  AsioTCPSocketFactory.h
  InputEventBuffer.cpp
  InputEventBuffer.h
  KeyStateTable.cpp
  KeyStateTable.h
  IDataSocket.cpp
  IDataSocket.h
  IListenSocket.h
  ISocket.h
  ISocketFactory.h
  ISocketMultiplexerJob.h
  NetworkAddress.cpp
  NetworkAddress.h
  SocketException.cpp
  SocketException.h
  SocketMultiplexer.cpp
  SocketMultiplexer.h
  TCPListenSocket.cpp
  TCPListenSocket.h
  TCPSocket.cpp
  TCPSocket.h
  TCPSocketFactory.cpp
  TCPSocketFactory.h
  TSocketMultiplexerMethodJob.h
)

target_link_libraries(
  net
  PUBLIC Qt6::Network common asio
  PRIVATE mt io)
