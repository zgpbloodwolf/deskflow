# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

if(WIN32)
  # classifyBtError 依赖 winsock2.h 的 WSA 符号，仅 Windows 编译
  # arch 静态库引用 GetFileVersionInfo* 等 version.lib 符号，需在此传递
  create_test(
    NAME BtErrorTests
    DEPENDS net
    LIBS base arch version
    SOURCE BtErrorTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
  )
endif()
