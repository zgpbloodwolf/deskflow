# Technology Stack

**Analysis Date:** 2026-05-12

## Languages

**Primary:**
- C++ (C++20 standard) - Core application logic, platform abstraction, networking, server/client protocol, GUI
- C - Platform-specific code (macOS Objective-C interop via `.m`/`.mm` files)

**Secondary:**
- CMake - Build system configuration and packaging
- Qt QML/UI (`.ui` files) - GUI layout definitions
- QRC (Qt Resource files) - Icon and asset bundling
- AppleScript - macOS DMG generation script (`deploy/mac/generate_ds_store.applescript`)

## Runtime

**Environment:**
- Native desktop application (no virtual machine or interpreter runtime)
- Compiles to native binaries for Windows (MSVC), macOS (Clang), and Linux (GCC/Clang)
- C++ standard: C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, extensions OFF)

**Build System:**
- CMake 3.24+ (minimum required)
- Ninja generator used for Flatpak and CI builds
- CPack for packaging (DEB, RPM, WIX/MSI, DragNDrop/DMG, 7Z)

## Frameworks

**Core:**
- Qt 6.7.0+ - GUI framework, networking, IPC, settings, internationalization
  - Components: `Core`, `Widgets`, `Network`
  - Linux-only components: `DBus`, `Xml`
  - Test component: `Test` (Qt Test framework)
  - LinguistTools - translation file generation

**Platform Abstraction (Linux/BSD):**
- libei 1.3+ - Emulated Input client library for Wayland input injection/capture
- libportal 0.9.1+ - XDG Portal library for Wayland input capture and remote desktop
- X11/Xorg libs - Legacy X11 input support (optional, `BUILD_X11_SUPPORT` ON by default)
  - Xtst, X11, xkbfile, Xext, Xinerama, Xi, Xrandr, SM, ICE

**Security:**
- OpenSSL 3.0+ - TLS encryption for network connections (`SSL` and `Crypto` components)
  - Apple uses static linking for OpenSSL

**Testing:**
- Qt Test (`Qt::Test`) - Unit test framework
- Google Test (googletest) - Fetched if not found on host system; used for legacy tests

## Key Dependencies

**Critical:**
- Qt6::Core - Event loop, signals/slots, QSettings, QString, QProcess
- Qt6::Widgets - Main window, dialogs, widgets, layout system
- Qt6::Network - TCP sockets, TLS, QNetworkAccessManager (update checks)
- OpenSSL::SSL / OpenSSL::Crypto - Secure socket implementation (`src/lib/net/SecureSocket.cpp`)

**Platform-Specific (Windows):**
- Wtsapi32, Userenv, Wininet, comsuppw, Shlwapi, version - Windows API integration
- Crypt32, ws2_32 - Windows crypto and networking
- MSI - Windows Installer custom actions (WiX)
- OpenSSL::applink - OpenSSL/MSVC ABI compatibility

**Platform-Specific (macOS):**
- ScreenSaver, IOKit, ApplicationServices, Foundation, Carbon, UserNotifications, Cocoa - macOS system APIs
- macdeployqt - Qt deployment tool for macOS app bundles

**Platform-Specific (Linux/BSD):**
- pthread - POSIX threading
- xkbcommon - Keyboard layout handling
- glib-2.0 - GLib main loop (required by libportal/libei integration)
- libm - Math library

## Configuration

**Build Configuration:**
- Main build file: `CMakeLists.txt` (project root)
- Library configuration: `cmake/Libraries.cmake`
- Platform code signing: `cmake/MacCodesign.cmake`
- Code coverage: `cmake/CodeCoverage.cmake` (uses gcovr)
- vcpkg template: `cmake/vcpkg.json.in` (Windows optional Qt via vcpkg)

**CMake Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `BUILD_X11_SUPPORT` | Build X11 backend (Linux/BSD) | ON |
| `BUILD_OSX_BUNDLE` | Build macOS app bundle | ON |
| `BUILD_INSTALLER` | Build installers/packages | ON |
| `ENABLE_COVERAGE` | Enable test coverage (gcovr) | OFF |
| `SKIP_BUILD_TESTS` | Skip running tests at build time | OFF |
| `VCPKG_QT` | Build Qt with vcpkg (Windows) | OFF |
| `CLEAN_TRS` | Remove obsolete translation strings | OFF |
| `APPLE_CODESIGN_DEV` | Apple codesign cert ID for dev | Not set |

**Runtime Configuration:**
- QSettings-based configuration (INI-style `.conf` files)
- Settings paths differ by OS (see `src/lib/common/Settings.h`):
  - Windows: `%APPDATA%/Deskflow/Deskflow.conf` (user), `C:\ProgramData\Deskflow\Deskflow.conf` (system)
  - macOS: `~/Library/Deskflow/Deskflow.conf` (user), `/Library/Deskflow/Deskflow.conf` (system)
  - Linux: `~/.config/Deskflow/Deskflow.conf` (user), `/etc/Deskflow/Deskflow.conf` (system)

**Internationalization:**
- Qt Linguist translations: `translations/` directory
- Supported languages: en (plural-only), es, it, ja, zh_CN, ru, ko
- Translation files: `.ts` format, compiled at build time

## Platform Requirements

**Development:**
- CMake 3.24+
- Qt 6.7.0+ SDK
- OpenSSL 3.0+ development headers
- C++20 compatible compiler (MSVC 14.x on Windows, Clang on macOS, GCC/Clang on Linux)
- On Linux: pkg-config, libei-dev 1.3+, libportal-dev 0.9.1+, X11 development libraries
- Optional: Doxygen (for documentation), gcovr (for coverage)

**Production:**
- Cross-platform desktop deployment
- Windows: WiX 4+ for MSI installer, 7Z archive fallback
- macOS: DMG (DragNDrop CPack generator), app bundle with macdeployqt
- Linux: DEB (Debian/Ubuntu), RPM (Fedora/SUSE), Flatpak (via `deploy/linux/flatpak/`), Arch PKGBUILD
- Default network port: 24800 (TCP, configurable)

## Version Management

- Version derived from git tags (`git describe --long --match v*`)
- Fallback version: 1.26.0.0 (in `CMakeLists.txt`)
- Version constants generated: `src/lib/common/Constants.h.in`, `src/lib/common/VersionInfo.h.in`
- Update check endpoint: `https://api.deskflow.org/version` (configured in `src/lib/common/UrlConstants.h`)

## License

- GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
- SPDX compliance managed via `REUSE.toml`
- Icon assets: LGPL-2.1-only (KDE Breeze Icons)

---

*Stack analysis: 2026-05-12*
