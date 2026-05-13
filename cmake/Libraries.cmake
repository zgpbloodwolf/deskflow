# SPDX-FileCopyrightText: (C) 2024 - 2025 Deskflow Developers
# SPDX-FileCopyrightText: (C) 2024 Symless Ltd
# SPDX-License-Identifier: MIT

macro(configure_libs)

  set(libs)
  if(UNIX)
    configure_unix_libs()
  elseif(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /O2 /Ob2")
    list(APPEND libs Wtsapi32 Userenv Wininet comsuppw Shlwapi version)
    add_definitions(
      /DWIN32
      /D_WINDOWS
      /D_CRT_SECURE_NO_WARNINGS
      /D_XKEYCHECK_H
    )
  endif()

  find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core Widgets Network)

  # Define the location of Qt deployment tool
  if(WIN32)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND VCPKG_QT)
      set(DEPLOY_TOOL windeployqt.debug.bat)
    else()
      set(DEPLOY_TOOL windeployqt)
    endif()
  elseif(APPLE)
      set(DEPLOY_TOOL macdeployqt)
  endif()

  if (WIN32 OR APPLE)
    find_program(DEPLOYQT ${DEPLOY_TOOL})
    if(DEPLOYQT STREQUAL "DEPLOYQT-NOTFOUND")
      message(FATAL_ERROR "Unable to locate the Qt Deploy Tool: \"${DEPLOY_TOOL}\"")
    endif()
    unset(DEPLOY_TOOL)
  endif()

  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTORCC ON)

  message(STATUS "Qt version: ${Qt6_VERSION}")

  # Check if <format> header is available
  check_cxx_source_compiles("
    #include <format>
    int main() {
        char buffer[100];
        std::format_to_n(buffer, 100, \"test {}\", 42);
        return 0;
    }
    " HAVE_FORMAT)

  if(HAVE_FORMAT)
    add_definitions(-DHAVE_FORMAT)
  endif()

  option(ENABLE_COVERAGE "Enable test coverage" OFF)
  if(ENABLE_COVERAGE)
    message(STATUS "Enabling code coverage")
    include(cmake/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    set(test_exclude subprojects/* build/* src/unittests/*)
    set(test_src ${PROJECT_SOURCE_DIR}/src)

    # Apparently solves the bug in gcov where it returns negative counts and confuses gcovr.
    # > Got negative hit value in gcov line 'branch  2 taken -1' caused by a bug in gcov tool
    # Bug report: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68080
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-update=atomic")

    setup_target_for_coverage_gcovr_xml(
      NAME coverage-legacytests
      EXECUTABLE legacytests
      BASE_DIRECTORY ${test_src}
      EXCLUDE ${test_exclude}
    )
  endif()

endmacro()

#
# Unix (Mac, Linux, BSD, etc)
#
macro(configure_unix_libs)

  # For config.h, detect the libraries, functions, etc.
  include(CheckIncludeFiles)
  include(CheckLibraryExists)
  include(CheckFunctionExists)
  include(CheckTypeSize)
  include(CheckIncludeFileCXX)
  include(CheckSymbolExists)
  include(CheckCSourceCompiles)
  include(CheckCXXSourceCompiles)

  check_include_files(sys/socket.h HAVE_SYS_SOCKET_H)
  if (NOT HAVE_SYS_SOCKET_H)
    message(FATAL_ERROR "Missing header: sys/socket.h")
  endif()


  check_include_files(unistd.h HAVE_UNISTD_H)
  if (NOT HAVE_UNISTD_H)
    message(FATAL_ERROR "Missing unistd.h")
  endif()

  check_function_exists(sigwait HAVE_POSIX_SIGWAIT)
  if (NOT HAVE_POSIX_SIGWAIT)
    message(FATAL_ERROR "Missing posix sigwait")
  endif()

  # pthread is used on both Linux and Mac
  check_library_exists("pthread" pthread_create "" HAVE_PTHREAD)
  if(HAVE_PTHREAD)
    list(APPEND libs pthread)
  else()
    message(FATAL_ERROR "Missing library: pthread")
  endif()

  if(APPLE)
    find_library(lib_ScreenSaver ScreenSaver)
    find_library(lib_IOKit IOKit)
    find_library(lib_ApplicationServices ApplicationServices)
    find_library(lib_Foundation Foundation)
    find_library(lib_Carbon Carbon)
    find_library(lib_UserNotifications UserNotifications)
    list(APPEND libs
      ${lib_ScreenSaver} ${lib_IOKit} ${lib_ApplicationServices}
      ${lib_Foundation} ${lib_Carbon} ${lib_UserNotifications}
    )
  endif()
endmacro()
