# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT

# Standalone Asio 集成
# 优先使用 vendor 目录中的本地源码（离线构建支持）
# 如果本地源码不存在且网络可用，则通过 FetchContent 下载

set(ASIO_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/asio-asio-1-38-0")

if(EXISTS "${ASIO_VENDOR_DIR}/include/asio.hpp")
  # 使用本地 vendor 目录中的 Asio 源码
  message(STATUS "使用本地 Asio 源码: ${ASIO_VENDOR_DIR}")
  add_library(asio INTERFACE)
  target_include_directories(asio SYSTEM INTERFACE "${ASIO_VENDOR_DIR}/include")
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
  if(WIN32)
    target_link_libraries(asio INTERFACE ws2_32)
  endif()
else()
  # 网络下载作为备选方案
  include(FetchContent)

  FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-38-0
  )

  FetchContent_MakeAvailable(asio)

  # Standalone Asio 没有CMakeLists.txt，需要手动创建 interface library
  if(NOT TARGET asio)
    add_library(asio INTERFACE)
    target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/include)
    target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
    if(WIN32)
      target_link_libraries(asio INTERFACE ws2_32)
    endif()
  endif()
endif()
