# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

if(WIN32)
  # classifyBtError 依赖 winsock2.h 的 WSA 符号，仅 Windows 编译
  create_test(
    NAME BtErrorTests
    DEPENDS net
    LIBS base arch
    SOURCE BtErrorTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
  )
endif()
