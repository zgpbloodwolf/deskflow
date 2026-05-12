# Codebase Structure

**Analysis Date:** 2026-05-12

## Directory Layout

```
deskflow/
├── cmake/                        # CMake helper modules
│   ├── Libraries.cmake           # Dependency resolution (Qt, OpenSSL, libei, libportal, etc.)
│   ├── MacCodesign.cmake         # macOS code signing
│   ├── CodeCoverage.cmake        # gcovr coverage setup
│   └── vcpkg.json.in             # Windows vcpkg manifest template
├── deploy/                       # Platform-specific packaging/deployment
│   ├── CMakeLists.txt            # Top-level deploy orchestration
│   ├── linux/                    # Linux packaging (Arch, Flatpak)
│   ├── mac/                      # macOS packaging (DMG, app bundle)
│   └── windows/                  # Windows packaging (MSI, NSIS)
├── docs/                         # Documentation
│   ├── dev/                      # Developer docs
│   └── user/                     # User-facing docs
├── src/                          # All source code
│   ├── CMakeLists.txt            # Adds lib/, apps/, unittests/
│   ├── apps/                     # Executable targets
│   │   ├── deskflow-gui/         # GUI application entry point
│   │   ├── deskflow-core/        # Core client/server executable
│   │   ├── deskflow-daemon/      # Windows daemon executable
│   │   └── res/                  # Shared app resources (icons, plist templates, etc.)
│   ├── lib/                      # Static libraries
│   │   ├── arch/                 # Platform architecture abstraction
│   │   ├── base/                 # Event system, logging, string utils
│   │   ├── client/               # Client protocol (Client, ServerProxy)
│   │   ├── common/               # Shared constants, settings, enums, I18N
│   │   ├── deskflow/             # Core application framework (App, IPC servers)
│   │   ├── gui/                  # GUI library (MainWindow, dialogs, widgets)
│   │   ├── io/                   # I/O streams and filters
│   │   ├── mt/                   # Threading primitives
│   │   ├── net/                  # Networking (TCP, TLS, sockets)
│   │   ├── platform/             # Platform-specific implementations
│   │   └── server/               # Server protocol (Server, ClientProxy, Config)
│   └── unittests/                # Test source files
│       ├── base/                 # Tests for base lib
│       ├── common/               # Tests for common lib
│       ├── deskflow/             # Tests for deskflow lib
│       ├── gui/                  # Tests for gui lib
│       ├── legacytests/          # Legacy test suite
│       ├── net/                  # Tests for net lib
│       ├── platform/             # Tests for platform lib
│       └── server/               # Tests for server lib
├── translations/                 # Qt translation files (.ts)
├── LICENSES/                     # License files
├── CMakeLists.txt                # Root CMake configuration
├── CLAUDE.md                     # AI agent instructions
├── REUSE.toml                    # REUSE compliance config
├── cspell.json                  # Spell check config
└── sonar-project.properties      # SonarQube config
```

## Directory Purposes

**`src/apps/`:**
- Purpose: Executable entry points (GUI, core, daemon)
- Contains: `main()` functions, argument parsing, resource files
- Key files: `deskflow-gui/deskflow-gui.cpp`, `deskflow-core/deskflow-core.cpp`, `deskflow-daemon/deskflow-daemon.cpp`

**`src/apps/res/`:**
- Purpose: Shared application resources
- Contains: Icons (dark/light themes at multiple resolutions), Windows .rc templates, macOS plist templates, app icons (.ico, .icns), man page template
- Key files: `deskflow.qrc` (Qt resource file), `deskflow.plist.in` (macOS bundle config)

**`src/lib/common/`:**
- Purpose: Lowest-level shared code used by all other libraries
- Contains: Constants, version info, settings singleton, exit codes, enums, I18N, platform info, network protocol constants, URL constants
- Key files: `Settings.h/.cpp`, `Constants.h.in`, `VersionInfo.h.in`, `Enums.h`, `ExitCodes.h`
- Note: The `common` library sets the PUBLIC include path `src/lib/` so all libraries can use `#include "common/Settings.h"` style paths

**`src/lib/arch/`:**
- Purpose: Platform-specific low-level abstractions
- Contains: Platform-specific daemon control, network operations, multithreading primitives, logging
- Subdirectories: `win32/` (Windows implementations), `unix/` (POSIX implementations)
- Key files: `Arch.h/.cpp`, `IArchDaemon.h`, `IArchNetwork.h`, `IArchMultithread.h`

**`src/lib/base/`:**
- Purpose: Core event system, logging, and utility functions
- Contains: EventQueue, Log, Event types, string utilities, Unicode, exceptions, stopwatch
- Key files: `EventQueue.h/.cpp`, `Event.h`, `Log.h/.cpp`, `BaseException.h`, `String.h/.cpp`

**`src/lib/mt/`:**
- Purpose: Threading primitives
- Contains: Thread, Mutex, CondVar, Lock wrappers
- Key files: `Thread.h/.cpp`, `Mutex.h/.cpp`, `CondVar.h/.cpp`

**`src/lib/io/`:**
- Purpose: Stream abstractions
- Contains: IStream interface, StreamBuffer, StreamFilter, IOException
- Key files: `IStream.h`, `StreamBuffer.h/.cpp`, `StreamFilter.h/.cpp`

**`src/lib/net/`:**
- Purpose: Network communication
- Contains: TCP sockets, TLS/SSL sockets, socket multiplexer, socket factories, fingerprint database, network address resolution
- Key files: `TCPSocket.h/.cpp`, `SecureSocket.h/.cpp`, `SocketMultiplexer.h/.cpp`, `FingerprintDatabase.h/.cpp`, `NetworkAddress.h/.cpp`

**`src/lib/deskflow/`:**
- Purpose: Core application framework and domain logic
- Contains: App base classes (ClientApp, ServerApp), Screen, Clipboard, ProtocolUtil, ProtocolTypes, KeyMap, KeyState, IPC servers, platform utilities
- Subdirectories: `ipc/` (IpcServer, CoreIpcServer, DaemonIpcServer), `unix/` (Unix app utils, X11 layout parsing), `win32/` (Windows app utils)
- Key files: `App.h/.cpp`, `ClientApp.h/.cpp`, `ServerApp.h/.cpp`, `ProtocolTypes.h`, `ProtocolUtil.h/.cpp`, `Screen.h/.cpp`, `Clipboard.h/.cpp`

**`src/lib/client/`:**
- Purpose: Client-side protocol implementation
- Contains: Client class, ServerProxy (server message handler on client side)
- Key files: `Client.h/.cpp`, `ServerProxy.h/.cpp`

**`src/lib/server/`:**
- Purpose: Server-side protocol implementation
- Contains: Server class, ClientProxy (versioned 1.0 through 1.8), ClientListener, Config (screen layout), InputFilter, PrimaryClient
- Key files: `Server.h/.cpp`, `Config.h/.cpp`, `ClientProxy1_8.h/.cpp`, `ClientListener.h/.cpp`

**`src/lib/platform/`:**
- Purpose: OS-specific screen, input, and clipboard implementations
- Contains: One platform implementation per OS. Windows: MSWindows* files. macOS: OSX* files. Linux X11: XWindows* files. Linux Wayland: EiScreen*, PortalInputCapture*, WlClipboard*, XDG* files.
- Key files: Platform screens (`MSWindowsScreen.h`, `OSXScreen.h`, `EiScreen.h`, `XWindowsScreen.h`), clipboard implementations, key state implementations

**`src/lib/gui/`:**
- Purpose: Qt Widgets GUI library
- Subdirectories:
  - `config/` - Screen, ScreenConfig, ScreenList, ServerConfig models
  - `core/` - CoreProcess (manages core subprocess), NetworkMonitor
  - `dialogs/` - AboutDialog, ActionDialog, SettingsDialog, ServerConfigDialog, etc.
  - `ipc/` - IpcClient, DaemonIpcClient, CoreIpcClient
  - `validators/` - Input validators (alias, hostname, IP, screen name)
  - `widgets/` - Custom widgets (LogDock, StatusBar, ScreenSetupView, etc.)
- Key files: `MainWindow.h/.cpp`, `core/CoreProcess.h/.cpp`, `TlsUtility.h/.cpp`

**`src/unittests/`:**
- Purpose: Unit tests mirroring lib directory structure
- Contains: One test .cpp file per component under test, plus a `legacytests/` subdirectory with older test infrastructure
- Key pattern: Tests are organized in subdirectories matching `src/lib/` structure (base/, common/, deskflow/, gui/, net/, platform/, server/)

**`translations/`:**
- Purpose: Qt Linguist translation files
- Contains: `.ts` files for each language (en, es, it, ja, ko, ru, zh_CN)
- Generated: `.qm` files are generated during build

**`deploy/`:**
- Purpose: Platform-specific packaging and installer creation
- Subdirectories: `linux/` (Arch PKGBUILD, Flatpak manifest), `mac/` (DMG creation), `windows/` (MSI/NSIS installer)

**`cmake/`:**
- Purpose: CMake helper modules
- Key files: `Libraries.cmake` (finds and configures all external dependencies), `MacCodesign.cmake` (macOS code signing)

## Key File Locations

**Entry Points:**
- `src/apps/deskflow-gui/deskflow-gui.cpp`: GUI application entry point
- `src/apps/deskflow-core/deskflow-core.cpp`: Core (client/server) entry point
- `src/apps/deskflow-daemon/deskflow-daemon.cpp`: Windows daemon entry point

**Configuration:**
- `CMakeLists.txt`: Root build configuration
- `cmake/Libraries.cmake`: External dependency management
- `src/lib/common/Settings.h`: All settings keys and defaults
- `src/lib/common/Constants.h.in`: Build-time constants (app name, version)
- `src/lib/common/VersionInfo.h.in`: Version information template

**Core Logic:**
- `src/lib/deskflow/ProtocolTypes.h`: Network protocol constants and message format definitions
- `src/lib/deskflow/ProtocolUtil.h/.cpp`: Binary protocol serialization/deserialization
- `src/lib/server/Server.h/.cpp`: Server main loop, screen switching, input routing
- `src/lib/client/Client.h/.cpp`: Client connection and input handling
- `src/lib/deskflow/Screen.h/.cpp`: Platform-independent screen abstraction

**IPC Communication:**
- `src/lib/deskflow/ipc/IpcServer.h`: Base IPC server (QLocalSocket)
- `src/lib/deskflow/ipc/CoreIpcServer.h`: Core process IPC server
- `src/lib/deskflow/ipc/DaemonIpcServer.h`: Daemon IPC server
- `src/lib/gui/ipc/IpcClient.h`: Base IPC client
- `src/lib/gui/ipc/DaemonIpcClient.h`: GUI-to-daemon IPC client
- `src/lib/gui/ipc/CoreIpcClient.h`: GUI-to-core IPC client

**Testing:**
- `src/unittests/CMakeLists.txt`: Test infrastructure and `create_test()` CMake function
- Individual test files follow pattern: `src/unittests/{lib_name}/{ClassName}Tests.cpp`

## Naming Conventions

**Files:**
- PascalCase for class files: `MainWindow.cpp`, `CoreProcess.cpp`, `ServerProxy.cpp`
- Interface prefix `I` for abstract classes: `IPlatformScreen.h`, `IApp.h`, `IStream.h`
- Platform prefix for platform-specific classes: `MSWindowsScreen.h`, `OSXScreen.h`, `XWindowsScreen.h`, `EiScreen.h`
- Test files use suffix `Tests.cpp`: `SettingsTests.cpp`, `KeyMapTests.cpp`
- `.ui` files for Qt Designer forms: `MainWindow.ui`, `SettingsDialog.ui`
- `.in` suffix for CMake configure templates: `Constants.h.in`, `vcpkg.json.in`

**Directories:**
- Lowercase for library directories: `base/`, `net/`, `mt/`, `io/`
- kebab-case for app directories: `deskflow-gui/`, `deskflow-core/`, `deskflow-daemon/`
- Lowercase for subdirectories within libraries: `ipc/`, `config/`, `dialogs/`, `widgets/`, `validators/`, `core/`

**CMake Targets:**
- Library targets use directory name: `base`, `net`, `mt`, `io`, `arch`, `client`, `server`, `platform`, `gui`, `common`
- The deskflow library target is named `app` (see `src/lib/deskflow/CMakeLists.txt` line `set(lib_name app)`)
- Executable targets: `deskflow` (GUI, or `Deskflow` on macOS), `deskflow-core`, `deskflow-daemon` (Windows only)

## Where to Add New Code

**New Feature (core protocol):**
- Primary code: `src/lib/deskflow/` for app-level logic, `src/lib/server/` or `src/lib/client/` for protocol changes
- Protocol changes: Update `src/lib/deskflow/ProtocolTypes.h` for message types
- Tests: `src/unittests/deskflow/` or `src/unittests/server/`

**New Feature (GUI):**
- Dialog: `src/lib/gui/dialogs/` (add .cpp, .h, .ui files; update `src/lib/gui/CMakeLists.txt`)
- Widget: `src/lib/gui/widgets/`
- Config model: `src/lib/gui/config/`
- Tests: `src/unittests/gui/`

**New Platform Support:**
- Screen implementation: `src/lib/platform/` (add new platform screen class implementing `IPlatformScreen`)
- Update `src/lib/platform/CMakeLists.txt` with new platform conditional block
- Arch layer: `src/lib/arch/` if new OS-level primitives needed

**New IPC Command:**
- Server side: Add command handling in `src/lib/deskflow/ipc/CoreIpcServer.cpp` or `DaemonIpcServer.cpp`
- Client side: Add send method in `src/lib/gui/ipc/CoreIpcClient.cpp` or `DaemonIpcClient.cpp`

**New Setting:**
- Add key constant in `src/lib/common/Settings.h` (appropriate struct: Client, Core, Daemon, Gui, etc.)
- Add key to `m_validKeys` list
- Add default value to `m_defaultFalseValues` or `m_defaultTrueValues` if boolean

**New Test:**
- Create `src/unittests/{lib_name}/{ClassName}Tests.cpp`
- Use `create_test()` CMake function from `src/unittests/CMakeLists.txt`
- Link against the library being tested plus `Qt::Test`

**New Translation:**
- Add `.ts` file to `translations/`
- Update `translations/CMakeLists.txt`

## Special Directories

**`src/lib/common/` (generated files):**
- Purpose: CMake-generated headers (Constants.h, VersionInfo.h)
- Generated: Yes (from `.h.in` templates)
- Committed: No (build artifacts)

**`translations/` (.ts files):**
- Purpose: Qt Linguist translation source files
- Generated: Partially (lupdate updates them)
- Committed: Yes

**`src/apps/res/`:**
- Purpose: Application icons (dark/light themes), Windows resources, macOS bundle configs
- Generated: No
- Committed: Yes

**`deploy/`:**
- Purpose: Platform-specific packaging scripts and configs
- Generated: No
- Committed: Yes

**`.planning/`:**
- Purpose: GSD planning documents
- Generated: By GSD tools
- Committed: Yes (part of the repo)

---

*Structure analysis: 2026-05-12*
