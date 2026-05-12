# External Integrations

**Analysis Date:** 2026-05-12

## APIs & External Services

**Version Check API:**
- Deskflow version check endpoint - Checks for application updates
  - URL: `https://api.deskflow.org/version` (defined in `src/lib/common/UrlConstants.h`)
  - Client: `QNetworkAccessManager` (Qt Network)
  - Implementation: `src/lib/gui/VersionChecker.cpp`
  - Headers sent: `User-Agent`, `X-Deskflow-Version`, `X-Deskflow-Language`
  - Configurable via `Settings::Gui::UpdateCheckUrl`

**Deskflow Website:**
- Help page: `https://deskflow.org/help?source=gui`
- Download page: `https://deskflow.org/download?source=gui`
- Defined in: `src/lib/common/UrlConstants.h`

**Linux-specific URLs:**
- GNOME tray fix: `https://extensions.gnome.org/extension/615/appindicator-support/` (shown in GUI on Linux)

## Data Storage

**Databases:**
- Not applicable - No database server or ORM used

**Configuration Files:**
- QSettings (INI format `.conf` files) - Application settings
  - Proxy: `src/lib/common/QSettingsProxy.cpp`
  - Settings manager: `src/lib/common/Settings.cpp`
  - User and system config paths vary by platform (see Settings.h)

**TLS Certificate Storage:**
- Local filesystem only
- TLS directory derived from settings path
- Trusted servers DB: `<settings_path>/tls/trusted-servers`
- Trusted clients DB: `<settings_path>/tls/trusted-clients`
- Certificate file path: `Settings::Security::Certificate`
- Fingerprint management: `src/lib/net/FingerprintDatabase.cpp`
- Certificate generation: `src/lib/gui/TlsUtility.cpp`

**File Storage:**
- Local filesystem only
- Log files: Configurable via `Settings::Log::File`
- Server config file: Configurable via `Settings::Daemon::ConfigFile`
- Daemon log file: Configurable via `Settings::Daemon::LogFile`

**Caching:**
- None

## Authentication & Identity

**Peer Authentication:**
- TLS certificate fingerprint verification
  - On connect, peers exchange TLS fingerprints
  - Fingerprints stored in local trust databases
  - Implementation: `src/lib/net/SecureSocket.cpp`, `src/lib/net/Fingerprint.cpp`
  - Fingerprint check enabled by default: `Settings::Security::CheckPeers` defaults to true
  - GUI dialog: `src/lib/gui/dialogs/FingerprintDialog.cpp`

**TLS Certificate Management:**
- Self-signed certificate generation (RSA key pair via OpenSSL)
  - Implementation: `src/lib/gui/TlsUtility.cpp`
  - Key size configurable: `Settings::Security::KeySize`
  - TLS enabled by default: `Settings::Security::TlsEnabled` defaults to true

**No External Auth Provider:**
- All authentication is peer-to-peer TLS fingerprint based
- No OAuth, JWT, or third-party identity provider

## Platform Input/Display Integrations

**Linux (X11):**
- X11 extensions via Xlib: Xtst (test input injection), Xrandr (display), Xinerama (multi-monitor), Xi (XInput2), DPMS (power), XKB (keyboard)
  - Implementation: `src/lib/platform/XWindows*.cpp`
  - Requires: Xtst, X11, xkbfile, Xext, Xinerama, Xi, Xrandr, SM, ICE

**Linux (Wayland/libei):**
- libei 1.3+ - Emulated Input for capturing and injecting keyboard/mouse events on Wayland
  - Implementation: `src/lib/platform/EiScreen.cpp`, `src/lib/platform/EiKeyState.cpp`
- libportal 0.9.1+ - XDG Portal for input capture session management
  - `PortalInputCapture` - Modern XDG InputCapture portal (libportal >= 0.9.1)
    - Implementation: `src/lib/platform/PortalInputCapture.cpp`
    - Uses `xdp_portal_new()` from libportal C API
    - Runs GLib main loop on separate thread (`g_main_loop_new`)
  - `PortalRemoteDesktop` - XDG RemoteDesktop portal (fallback)
    - Implementation: `src/lib/platform/PortalRemoteDesktop.cpp`
- Wayland clipboard via `WlClipboard` and `WlClipboardCollection`
  - Implementation: `src/lib/platform/WlClipboard.cpp`, `src/lib/platform/WlClipboardCollection.cpp`
- Restore token support for input capture sessions (compile-time check: `HAVE_LIBPORTAL_INPUTCAPTURE_RESTORE`)
  - Token persisted in settings: `Settings::Client::XdpRestoreToken`, `Settings::Server::XdpRestoreToken`

**macOS:**
- Carbon, Cocoa, IOKit, ApplicationServices, ScreenSaver, UserNotifications, Foundation
  - Screen: `src/lib/platform/OSXScreen.mm`
  - Clipboard: `src/lib/platform/OSXClipboard.cpp`
  - Key state: `src/lib/platform/OSXKeyState.cpp`
  - Power manager: `src/lib/platform/OSXPowerManager.cpp`
  - Screen saver: `src/lib/platform/OSXScreenSaver.cpp`
  - Helpers: `src/lib/gui/OSXHelpers.mm`

**Windows:**
- Win32 API: Wtsapi32, Userenv, Wininet, Shlwapi
  - Screen: `src/lib/platform/MSWindowsScreen.cpp`
  - Clipboard: `src/lib/platform/MSWindowsClipboard.cpp`
  - Key state: `src/lib/platform/MSWindowsKeyState.cpp`
  - Session management: `src/lib/platform/MSWindowsSession.cpp`
  - UAC/elevation daemon: `src/apps/deskflow-daemon/`
  - Process watchdog: `src/lib/platform/MSWindowsWatchdog.cpp`

## Networking

**Deskflow Protocol:**
- Custom binary protocol over TCP
  - Default port: 24800 (configurable via `Settings::Core::Port`)
  - Protocol version: 1.8 (defined in `src/lib/deskflow/ProtocolTypes.h`)
  - Supports two compatibility modes: Synergy protocol and Barrier protocol
  - Protocol utility: `src/lib/deskflow/ProtocolUtil.cpp`
  - Network address resolution: `src/lib/net/NetworkAddress.cpp`
  - Connection types: Plain TCP (`TCPSocket`) and TLS (`SecureSocket`)

**IPC (Inter-Process Communication):**
- Qt `QLocalServer` / `QLocalSocket` based IPC
  - Core IPC server: `src/lib/deskflow/ipc/CoreIpcServer.h` (core process communicates with GUI)
  - Daemon IPC server: `src/lib/deskflow/ipc/DaemonIpcServer.h` (daemon communicates with GUI)
  - GUI IPC clients: `src/lib/gui/ipc/CoreIpcClient.cpp`, `src/lib/gui/ipc/DaemonIpcClient.cpp`
  - Base class: `src/lib/deskflow/ipc/IpcServer.h`

## Monitoring & Observability

**Error Tracking:**
- None (no external error tracking service)

**Logs:**
- Custom logging framework in `src/lib/base/Log.cpp`
- Log levels: FATAL, ERROR, WARNING, NOTE, INFO, DEBUG, DEBUG1, DEBUG2
- Multiple outputters: `src/lib/base/LogOutputters.cpp`
- File-based logging: Configurable via `Settings::Log::ToFile`, `Settings::Log::File`
- GUI log widget: `src/lib/gui/widgets/LogWidget.cpp`
- File tail for daemon logs: `src/lib/gui/FileTail.cpp`
- SSL logging: `src/lib/net/SslLogger.cpp`

**Static Analysis:**
- SonarCloud integration: `sonar-project.properties`
  - Organization: `deskflow`
  - Project key: `deskflow_deskflow`
  - Host: `https://sonarcloud.io`

## CI/CD & Deployment

**Hosting:**
- GitHub: `https://github.com/deskflow/deskflow`
- Website: `https://deskflow.org`

**CI Pipeline:**
- Not present in this repository (`.github` directory absent)
- CI configuration likely managed externally or in a separate branch

**Packaging:**
- CPack-based packaging (see `deploy/CMakeLists.txt`)
- Windows: WiX 4 MSI installer + 7Z archive (`deploy/windows/deploy.cmake`)
- macOS: DMG (DragNDrop) with custom background and volume icon (`deploy/mac/deploy.cmake`)
- Linux: DEB (Debian/Ubuntu), RPM (Fedora/SUSE), with auto-detection (`deploy/linux/deploy.cmake`)
- Flatpak: `deploy/linux/flatpak/org.deskflow.deskflow.yml` (KDE 6.10 runtime)
- Arch Linux: PKGBUILD template at `deploy/linux/arch/PKGBUILD.in`

## Environment Configuration

**Required env vars:**
- No required environment variables for the application itself
- `VSCMD_ARG_TGT_ARCH` - Set by CI on Windows for build architecture detection

**Build-time env vars:**
- `APPLE_CODESIGN_DEV` - Apple developer certificate ID for local codesigning
- `CMAKE_BUILD_TYPE` - Controls debug/release build

**Secrets location:**
- No secrets management system in the repository
- TLS certificates generated and stored locally in the settings directory
- No `.env` files present

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- Version check HTTP GET to `https://api.deskflow.org/version` (initiated by GUI)
  - No webhook registration; simple poll-based check
  - Triggered by `VersionChecker::checkLatest()` in `src/lib/gui/VersionChecker.cpp`

## Code Quality Tools

**Spell Checking:**
- cspell configuration: `cspell.json`

**Code Coverage:**
- gcovr-based coverage reporting (`cmake/CodeCoverage.cmake`)
- Enabled with `ENABLE_COVERAGE` CMake option

**SPDX License Compliance:**
- REUSE.toml for automated license checking
- All source files carry SPDX headers

---

*Integration audit: 2026-05-12*
