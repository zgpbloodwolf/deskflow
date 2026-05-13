---
phase: 05-build-validation-regression
plan: 01
subsystem: build, testing
tags: cmake, macos, clang, qt6, gtest, vcpkg

requires:
  - phase: 04-cpp-modernization
    provides: C++20 modernization (smart pointers, std::jthread, event quit)
provides:
  - macOS clean build with zero errors and zero project-code warnings
  - All 20 Qt Test modern tests passing (100%)
  - All 27 Google Test legacy tests passing (100%)
  - vcpkg.json.in cleaned (no openssl/gtest references)
  - Restored arch/unix and deskflow/unix files needed by macOS
  - Local googletest vendor source (no network FetchContent)
affects: [05-02, 05-03, windows-build]

tech-stack:
  added: [googletest 1.15.2 vendor local source]
  patterns: [vendor-local FetchContent pattern for Chinese network environment]

key-files:
  created:
    - src/lib/arch/unix/ArchLogUnix.cpp
    - src/lib/arch/unix/ArchMultithreadPosix.cpp
    - src/lib/arch/unix/ArchNetworkBSD.cpp
    - src/lib/arch/unix/XArchUnix.cpp
    - src/lib/deskflow/unix/AppUtilUnix.cpp
    - vendor/googletest-1.15.2/
  modified:
    - cmake/vcpkg.json.in
    - src/lib/arch/CMakeLists.txt
    - src/lib/deskflow/CMakeLists.txt
    - src/lib/server/Server.cpp
    - src/unittests/legacytests/CMakeLists.txt

key-decisions:
  - "Restored arch/unix files: Phase 3 wrongly deleted them as Linux-only, but macOS needs POSIX/BSD implementations"
  - "Restored deskflow/unix/AppUtilUnix: Required by App.h for macOS keyboard layout queries"
  - "Vendor-local googletest: FetchContent git clone fails in Chinese network, use vendor/ directory like Asio"
  - "ArchNetworkBSDTests re-enabled: Added elseif(APPLE) block in legacytests CMakeLists"

patterns-established:
  - "Vendor-local FetchContent: SOURCE_DIR instead of GIT_REPOSITORY for dependencies unreliable to fetch"

requirements-completed: [BQ-01, BQ-02]

duration: 22min
completed: 2026-05-13
---

# Phase 5 Plan 01: Fix macOS Build & Run Tests Summary

macOS clean build restored with zero errors, all 47 automated tests passing (20 Qt Test + 27 GTest), vcpkg template cleaned of stale OpenSSL/GTest references

## Performance

- **Duration:** 22 min
- **Started:** 2026-05-13T07:06:24Z
- **Completed:** 2026-05-13T07:29:23Z
- **Tasks:** 2
- **Files modified:** 5 source files + 8 restored arch/unix files + vendor/googletest

## Accomplishments

### Task 1: Fix macOS build issues and clean up stale references

- Removed "openssl" and "gtest" from cmake/vcpkg.json.in dependencies array
- Restored 8 arch/unix files (ArchLogUnix, ArchMultithreadPosix, ArchNetworkBSD, XArchUnix) wrongly deleted by Phase 3
- Restored deskflow/unix/AppUtilUnix needed by App.h on macOS
- Added elseif(APPLE) blocks in arch/CMakeLists.txt and deskflow/CMakeLists.txt
- Fixed switchScreen() 4-argument call site after Phase 4 changed signature to 3 arguments
- Changed FetchContent googletest from GIT_REPOSITORY to local SOURCE_DIR (Chinese network)
- Verified net/CMakeLists.txt contains only SPDX header (no stale TLS test registrations)
- macOS clean build exits 0 with zero project-code errors and zero project-code warnings
- Both binaries exist: build/bin/Deskflow.app/Contents/MacOS/Deskflow and deskflow-core

### Task 2: Run and fix all automated tests on macOS

- All 20 modern Qt Test tests pass (100%)
- All 27 legacy Google Test tests pass (15 KeyStateTests + 12 ArchNetworkBSDTests)
- OSXKeyStateTests passes on this system (no keyboard layout dependency issue)
- Added elseif(APPLE) in replace_arch_sources to include ArchNetworkBSDTests.cpp

## Test Results

### Modern Qt Test (20 tests)

| # | Test | Result |
|---|------|--------|
| 1 | StringTests | PASS |
| 2 | UnicodeTests | PASS |
| 3 | LogTests | PASS |
| 4 | BaseExceptionTests | PASS |
| 5 | SettingsTests | PASS |
| 6 | I18NTests | PASS |
| 7 | ClipboardTests | PASS |
| 8 | ClipboardChunksTests | PASS |
| 9 | IKeyStateTests | PASS |
| 10 | KeyMapTests | PASS |
| 11 | KeyStateTests | PASS |
| 12 | KeyboardLayoutManagerTests | PASS |
| 13 | LoggerTests | PASS |
| 14 | KeySequenceTests | PASS |
| 15 | ScreenTests | PASS |
| 16 | NetworkMonitorTests | PASS |
| 17 | OSXClipboardTests | PASS |
| 18 | OSXKeyStateTests | PASS |
| 19 | ServerConfigTests | PASS |
| 20 | ServerTests | PASS |

### Legacy Google Test (27 tests)

- 15 KeyStateTests: PASS
- 12 ArchNetworkBSDTests: PASS

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Phase 3 wrongly deleted arch/unix files needed by macOS**
- **Found during:** Task 1 build
- **Issue:** Phase 3 deleted arch/unix/ as "Linux-only" but macOS needs POSIX/BSD implementations (ArchLogUnix, ArchMultithreadPosix, ArchNetworkBSD, XArchUnix)
- **Fix:** Restored all 8 files from git history, added elseif(APPLE) block in CMakeLists.txt
- **Files modified:** src/lib/arch/unix/*, src/lib/arch/CMakeLists.txt
- **Commit:** fcef0cb

**2. [Rule 1 - Bug] Phase 3 wrongly deleted deskflow/unix/AppUtilUnix**
- **Found during:** Task 1 build (App.h includes deskflow/unix/AppUtilUnix.h)
- **Issue:** AppUtilUnix is needed on macOS for keyboard layout queries via Carbon/TIS
- **Fix:** Restored AppUtilUnix.cpp and AppUtilUnix.h from git history, added elseif(APPLE) in deskflow CMakeLists.txt
- **Files modified:** src/lib/deskflow/unix/AppUtilUnix.cpp, AppUtilUnix.h, CMakeLists.txt
- **Commit:** fcef0cb

**3. [Rule 1 - Bug] switchScreen() call with wrong number of arguments**
- **Found during:** Task 1 build
- **Issue:** Phase 4 changed switchScreen() from 4 to 3 parameters but missed one call site at Server.cpp:1791
- **Fix:** Removed extra `false` argument from switchScreen() call
- **Files modified:** src/lib/server/Server.cpp
- **Commit:** fcef0cb

**4. [Rule 3 - Blocking] FetchContent googletest fails in Chinese network**
- **Found during:** Task 1 cmake configure
- **Issue:** FetchContent GIT_REPOSITORY https://github.com/google/googletest.git hangs/times out in Chinese network
- **Fix:** Downloaded googletest 1.15.2 via mirror to vendor/ directory, changed FetchContent to use SOURCE_DIR
- **Files modified:** src/unittests/legacytests/CMakeLists.txt, vendor/googletest-1.15.2/
- **Commit:** fcef0cb

**5. [Rule 3 - Blocking] ArchNetworkBSDTests not compiled on macOS**
- **Found during:** Task 2 test run (only 15 of expected 27 legacy tests)
- **Issue:** replace_arch_sources() in legacytests CMakeLists only had if(WIN32) block, no elseif(APPLE) for unix tests
- **Fix:** Added elseif(APPLE) block to include arch/unix/ArchNetworkBSDTests.cpp
- **Files modified:** src/unittests/legacytests/CMakeLists.txt
- **Commit:** a2b7f3f

## Environment

- macOS 26.4.1 (Tahoe)
- Apple clang 21.0.0
- CMake 4.0.2
- Qt 6.9.0 (homebrew)
- Asio 1.38.0 (vendor local)
- Googletest 1.15.2 (vendor local)
- AGL stub framework in /tmp (macOS 26+ workaround)

## Self-Check: PASSED

- cmake/vcpkg.json.in does not contain "openssl" or "gtest"
- build/bin/Deskflow.app/Contents/MacOS/Deskflow exists
- build/bin/Deskflow.app/Contents/MacOS/deskflow-core exists
- ctest 20/20 tests passed
- legacytests 27/27 tests passed
- Commit fcef0cb exists in git log
- Commit a2b7f3f exists in git log
