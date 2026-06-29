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
  BtAddress.cpp
  BtAddress.h
  BtBackend.h
  BtDataSocket.cpp
  BtDataSocket.h
  BtListenSocket.cpp
  BtListenSocket.h
  BtSocketFactory.cpp
  BtSocketFactory.h
  InputEventBuffer.cpp
  InputEventBuffer.h
  KeyStateTable.cpp
  KeyStateTable.h
  IDataSocket.cpp
  IDataSocket.h
  IListenSocket.h
  ISocket.h
  ISocketFactory.h
  NetworkAddress.cpp
  NetworkAddress.h
  SocketException.cpp
  SocketException.h
)

# 蓝牙平台特定后端
if(WIN32)
  target_sources(net PRIVATE
    BtBackendWindows.cpp
    BtBackendWindows.h
  )
elseif(APPLE)
  target_sources(net PRIVATE
    BtBackendMacOS.mm
    BtBackendMacOS.h
  )
endif()

target_link_libraries(
  net
  PUBLIC Qt6::Network common asio
  PRIVATE mt io)
