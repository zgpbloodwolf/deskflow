---
phase: 03-platform-feature-cleanup
plan: 01
subsystem: build, platform
tags: [cmake, linux-removal, x11, wayland, ei, platform-cleanup]

# Dependency graph
requires:
  - phase: 01-network-pipeline-rewrite
    provides: Asio network layer (not directly used, but prior phase)
provides:
  - "65 Linux platform files deleted (XWindows/EI/Wl/Portal/XDG)"
  - "arch/unix and deskflow/unix directories removed"
  - "All CMakeLists.txt cleaned of Linux compilation blocks"
  - "configure_xorg_libs macro removed from cmake/Libraries.cmake"
  - "generate_app_man function removed from src/apps/CMakeLists.txt"
affects: [03-platform-feature-cleanup, 05-build-validation-regression]

# Tech tracking
tech-stack:
  added: []
  patterns: ["Two-platform CMake: if(WIN32)/elseif(APPLE)/endif()"]

key-files:
  created: []
  modified:
    - CMakeLists.txt
    - cmake/Libraries.cmake
    - src/lib/platform/CMakeLists.txt
    - src/lib/deskflow/CMakeLists.txt
    - src/lib/arch/CMakeLists.txt
    - src/lib/client/CMakeLists.txt
    - src/lib/server/CMakeLists.txt
    - src/apps/CMakeLists.txt
    - src/apps/deskflow-gui/CMakeLists.txt
    - src/apps/deskflow-core/CMakeLists.txt
    - src/unittests/CMakeLists.txt
    - src/unittests/platform/CMakeLists.txt
    - src/unittests/deskflow/CMakeLists.txt
    - src/unittests/legacytests/CMakeLists.txt

key-decisions:
  - "Kept ScreenSaver framework link in Libraries.cmake APPLE block - OSXScreenSaver deletion is Plan 02 scope"
  - "Kept IScreenSaver.h in deskflow file list - screensaver removal is Plan 02 scope"

patterns-established:
  - "Two-platform CMake pattern: if(WIN32)/elseif(APPLE)/endif() replaces three-platform blocks"
  - "if(APPLE) replaces if(UNIX) for macOS-only links (client, server, deskflow libs)"

requirements-completed: [CLEAN-02, CLEAN-05]

# Metrics
duration: 6min
completed: 2026-05-13
---

# Phase 3 Plan 01: Remove Linux Platform Files & CMake Cleanup Summary

**Deleted 65 Linux platform files (X11/Wayland/EI/Portal/XDG) and cleaned 14 CMakeLists.txt to two-platform (macOS+Windows) only build configuration**

## Performance

- **Duration:** 6 min
- **Started:** 2026-05-13T05:34:14Z
- **Completed:** 2026-05-13T05:40:18Z
- **Tasks:** 2
- **Files modified:** 79 (65 deleted + 14 CMakeLists.txt modified)

## Accomplishments
- Removed all 65 Linux-only source files: 44 platform files (XWindows/EI/Wl/Portal/XDG), 15 unix common files (arch/unix + deskflow/unix), 6 Linux test files
- Cleaned all 14 CMakeLists.txt files of Linux-specific blocks: elseif(UNIX), UNIX AND NOT APPLE, BUILD_X11_SUPPORT, LIBEI/LIBPORTAL version variables
- Removed entire configure_xorg_libs macro (~90 lines) and generate_app_man function (~18 lines)
- Established two-platform CMake pattern: if(WIN32)/elseif(APPLE)/endif()

## Task Commits

Each task was committed atomically:

1. **Task 1: Delete all Linux platform files** - `03ffe52` (chore)
2. **Task 2: Update all CMakeLists.txt to remove Linux blocks** - `9ca5bbd` (chore)

## Files Created/Modified

### Deleted (65 files)
- `src/lib/platform/XWindows*.cpp/.h` (25 files) - X11 platform implementation
- `src/lib/platform/Ei*.cpp/.h` (6 files) - EI/Portal input implementation
- `src/lib/platform/Wl*.cpp/.h` (4 files) - Wayland clipboard
- `src/lib/platform/Portal*.cpp/.h` (4 files) - Portal remote desktop
- `src/lib/platform/XDG*.cpp/.h` (5 files) - XDG desktop utilities
- `src/lib/arch/unix/*` (8 files) - POSIX arch layer
- `src/lib/deskflow/unix/*` (7 files) - Unix app utilities and keyboard layout
- `src/unittests/platform/XWindowsClipboardTests.*` (2 files) - X11 clipboard tests
- `src/unittests/platform/WlClipboardTests.*` (2 files) - Wayland clipboard tests
- `src/unittests/deskflow/X11LayoutParserTests.*` (2 files) - X11 layout parser tests

### Modified (14 files)
- `CMakeLists.txt` - Removed LIBEI/LIBPORTAL version vars, simplified option block
- `cmake/Libraries.cmake` - Removed Linux else() block and configure_xorg_libs macro
- `src/lib/platform/CMakeLists.txt` - Removed top LIBEI block, elseif(UNIX) block, bottom Linux link block
- `src/lib/deskflow/CMakeLists.txt` - Removed elseif(UNIX) block, changed if(UNIX) to if(APPLE)
- `src/lib/arch/CMakeLists.txt` - Removed elseif(UNIX) block
- `src/lib/client/CMakeLists.txt` - Changed if(UNIX) to if(APPLE)
- `src/lib/server/CMakeLists.txt` - Changed if(UNIX) to if(APPLE)
- `src/apps/CMakeLists.txt` - Removed help2man and generate_app_man function
- `src/apps/deskflow-gui/CMakeLists.txt` - Removed else() generate_app_man call
- `src/apps/deskflow-core/CMakeLists.txt` - Removed else() generate_app_man call
- `src/unittests/CMakeLists.txt` - Removed UNIX AND NOT APPLE qPA_Platform block
- `src/unittests/platform/CMakeLists.txt` - Removed elseif(UNIX) test block
- `src/unittests/deskflow/CMakeLists.txt` - Removed BUILD_X11_SUPPORT test block
- `src/unittests/legacytests/CMakeLists.txt` - Removed elseif(UNIX) in replace_platform/arch macros

## Decisions Made
- Kept ScreenSaver framework link in Libraries.cmake APPLE block - OSXScreenSaver files still exist and will be handled in Plan 02 (screensaver removal)
- Kept IScreenSaver.h in deskflow CMakeLists.txt file list - screensaver interface removal is Plan 02 scope
- Changed if(UNIX) to if(APPLE) for client/server links since macOS is the only remaining UNIX platform

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Removed generate_app_man call from deskflow-core/CMakeLists.txt**
- **Found during:** Task 2 (CMake cleanup verification)
- **Issue:** src/apps/deskflow-core/CMakeLists.txt contained a `generate_app_man()` call in an else() block, but the function was deleted from src/apps/CMakeLists.txt. This would cause CMake configure failure.
- **Fix:** Removed the else() block with the generate_app_man call from deskflow-core/CMakeLists.txt
- **Files modified:** src/apps/deskflow-core/CMakeLists.txt
- **Verification:** grep for generate_app_man returns zero matches
- **Committed in:** 9ca5bbd (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking issue)
**Impact on plan:** Essential fix - without it, CMake configure would fail since generate_app_man function no longer exists. No scope creep.

## Issues Encountered
None - all file deletions and CMake edits were straightforward.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Linux platform files and CMake blocks fully removed, ready for Plan 02 (screensaver removal)
- Build system now configured for two-platform (macOS + Windows) only
- Note: cmake/Libraries.cmake still references ScreenSaver framework in APPLE block - will be cleaned in Plan 02

---
*Phase: 03-platform-feature-cleanup*
*Completed: 2026-05-13*
