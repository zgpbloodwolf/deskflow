<!-- refreshed: 2026-05-12 -->
# Architecture

**Analysis Date:** 2026-05-12

## System Overview

Deskflow is a cross-platform keyboard and mouse sharing utility (KVM-like software) that allows a single keyboard and mouse to control multiple computers over a network. The architecture consists of three executable targets (GUI, core, daemon) backed by a set of static libraries implementing platform abstraction, networking, protocol handling, and input management.

```text
┌──────────────────────────────────────────────────────────────────────┐
│                        Executable Layer                              │
├────────────────────┬─────────────────────┬───────────────────────────┤
│   deskflow (GUI)   │  deskflow-core      │  deskflow-daemon          │
│  `src/apps/        │  `src/apps/          │  `src/apps/               │
│   deskflow-gui/`   │  deskflow-core/`     │  deskflow-daemon/`        │
│  Qt Widgets app    │  Client/Server app   │  Windows-only service     │
└────────┬───────────┴──────────┬──────────┴─────────────┬─────────────┘
         │                      │                        │
         ▼                      ▼                        ▼
┌──────────────────────────────────────────────────────────────────────┐
│                     Application Layer (app library)                  │
│         `src/lib/deskflow/`                                          │
│  App, ClientApp, ServerApp, Screen, ProtocolUtil, Clipboard, IPC    │
└──────────────────────────┬───────────────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                  ▼
┌──────────────┐  ┌────────────────┐  ┌────────────────┐
│   client     │  │    server      │  │    platform     │
│ `src/lib/    │  │ `src/lib/      │  │ `src/lib/       │
│  client/`    │  │  server/`      │  │  platform/`     │
│ Client,      │  │ Server,        │  │ Platform screens│
│ ServerProxy  │  │ ClientProxy,   │  │ (Win/Mac/X11/   │
│              │  │ Config         │  │  Wayland/EI)    │
└──────┬───────┘  └───────┬────────┘  └────────┬────────┘
       │                  │                     │
       ▼                  ▼                     ▼
┌──────────────────────────────────────────────────────────────────────┐
│                       Infrastructure Layer                           │
├────────────┬──────────┬──────────┬──────────┬────────────────────────┤
│    net     │   io     │   mt     │   base   │      arch              │
│`src/lib/   │`src/lib/ │`src/lib/ │`src/lib/ │ `src/lib/              │
│ net/`      │ io/`     │ mt/`     │ base/`   │  arch/`                │
│ Sockets,   │ Streams, │ Threads, │ Events,  │ Platform arch:         │
│ TLS, SSL   │ Filters  │ Mutexes  │ Logging  │ network, threads,      │
│            │          │          │          │ logging, daemon        │
└────────────┴──────────┴──────────┴──────────┴────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────────┐
│                        common                                        │
│         `src/lib/common/`                                            │
│   Settings, Constants, Enums, I18N, ExitCodes                        │
└──────────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| `deskflow` (GUI executable) | Qt Widgets GUI for configuration, status display, tray icon, process management | `src/apps/deskflow-gui/deskflow-gui.cpp` |
| `deskflow-core` | Headless client/server binary; runs the actual input sharing logic | `src/apps/deskflow-core/deskflow-core.cpp` |
| `deskflow-daemon` | Windows service for UAC elevation and secure desktop handling | `src/apps/deskflow-daemon/deskflow-daemon.cpp` |
| `app` (deskflow lib) | Core application framework: App, ClientApp, ServerApp, Screen, Clipboard, Protocol, IPC servers | `src/lib/deskflow/` |
| `gui` lib | GUI components: MainWindow, dialogs, widgets, validators, CoreProcess, config models | `src/lib/gui/` |
| `client` lib | Client-side protocol: connection to server, ServerProxy message handling | `src/lib/client/` |
| `server` lib | Server-side logic: ClientListener, ClientProxy versions, ServerConfig, InputFilter | `src/lib/server/` |
| `platform` lib | Platform-specific screen, clipboard, keyboard, and input implementations | `src/lib/platform/` |
| `net` lib | TCP sockets, TLS/SSL, socket multiplexer, fingerprint management | `src/lib/net/` |
| `mt` lib | Threading primitives: Thread, Mutex, CondVar, Lock | `src/lib/mt/` |
| `io` lib | I/O streams, stream buffers, stream filters | `src/lib/io/` |
| `base` lib | Event system, logging, string utilities, Unicode | `src/lib/base/` |
| `arch` lib | Architecture abstraction: daemon, network, multithreading, logging per platform | `src/lib/arch/` |
| `common` lib | Shared constants, enums, settings, I18N, exit codes | `src/lib/common/` |

## Pattern Overview

**Overall:** Layered architecture with platform abstraction and IPC-based process separation

**Key Characteristics:**
- **Three-process design:** GUI process manages the user interface and controls the core process via IPC (QLocalSocket). The daemon process (Windows only) handles service elevation.
- **Static library layering:** Each concern is a CMake static library target with explicit `target_link_libraries` dependencies. Libraries form a DAG with no circular dependencies.
- **Platform abstraction via interfaces:** `IPlatformScreen` is the key abstraction. Platform-specific implementations live in `src/lib/platform/` (MSWindows*, OSX*, XWindows*, EiScreen*, XDG*).
- **Event-driven core:** The `base` library provides an event queue (`EventQueue`) used throughout the core for asynchronous communication between components.
- **Custom binary protocol:** Deskflow uses its own network protocol (`ProtocolTypes.h`, `ProtocolUtil`) over TCP with optional TLS, not REST or HTTP.

## Layers

### Executable Layer
- Purpose: Entry points for each runnable target
- Location: `src/apps/`
- Contains: `main()` functions, argument parsing, Qt application setup
- Depends on: All library layers
- Used by: End user invocation

### Application Layer (deskflow library)
- Purpose: Core domain logic -- client/server apps, screen management, clipboard, protocol, IPC servers
- Location: `src/lib/deskflow/`
- Contains: App base classes (ClientApp, ServerApp), Screen, Clipboard, ProtocolUtil, KeyMap, KeyState, IPC server implementations
- Depends on: common, client/server (on Unix), net, platform, base, arch, mt, io, Qt6
- Used by: deskflow-core, deskflow-daemon, gui

### GUI Layer
- Purpose: Qt Widgets user interface
- Location: `src/lib/gui/`
- Contains: MainWindow, dialogs, widgets, validators, config models, CoreProcess (process management), IPC clients, TLS utility, version checker
- Depends on: common, platform, Qt6 (Core, Widgets, Network)
- Used by: deskflow-gui executable

### Client/Server Layer
- Purpose: Client and server protocol implementations
- Location: `src/lib/client/`, `src/lib/server/`
- Contains: Client, ServerProxy (client); Server, ClientProxy (versions 1.0 through 1.8), ClientListener, Config (server)
- Depends on: common; client also depends on app and io on Unix
- Used by: platform (circular broken by PUBLIC linkage), app

### Platform Layer
- Purpose: OS-specific screen, clipboard, keyboard, event queue, power management
- Location: `src/lib/platform/`
- Contains: MSWindows* (Windows), OSX* (macOS), XWindows* (X11), EiScreen/EiKeyState (Wayland/libei), PortalInputCapture, XDG* (Linux portal)
- Depends on: client, io, net, app, platform-specific system libs (Cocoa, libei, libportal, X11, DBus)
- Used by: app (via Screen/IPlatformScreen abstraction)

### Infrastructure Layer
- Purpose: Low-level cross-platform abstractions
- Location: `src/lib/base/`, `src/lib/arch/`, `src/lib/mt/`, `src/lib/io/`, `src/lib/net/`, `src/lib/common/`
- Contains: Event system, logging, threading, sockets, TLS, I/O streams, settings, constants
- Depends on: Each other in a DAG: common <- arch <- base <- mt; common <- io; common <- net (also mt, io)
- Used by: All upper layers

## Data Flow

### Primary Request Path: Mouse/Keyboard Sharing

1. User moves mouse to screen edge on primary (server) computer
2. `MSWindowsScreen::onMouseMove` / `OSXScreen` / `XWindowsScreen` / `EiScreen` detects edge crossing (`src/lib/platform/`)
3. Platform screen posts event to `EventQueue` (`src/lib/base/EventQueue.h`)
4. `Server::onMouseMovePrimary` processes the event (`src/lib/server/Server.cpp`)
5. `Server::mapToNeighbor` determines target screen and coordinates
6. `Server::switchScreen` sends enter command to target `BaseClientProxy` (`src/lib/server/Server.cpp`)
7. `ClientProxy1_8` (or appropriate version) serializes message via `ProtocolUtil::writef` (`src/lib/server/ClientProxy1_8.cpp`)
8. Data written to TLS/TCP socket via `net` library (`src/lib/net/SecureSocket.cpp` or `TCPSocket.cpp`)
9. On client machine, `Client` receives data, `ServerProxy` parses message (`src/lib/client/ServerProxy.cpp`)
10. Client's `deskflow::Screen` calls platform screen to synthesize input (`src/lib/deskflow/Screen.cpp`)
11. Platform screen fakes mouse movement via OS API

### GUI-to-Core IPC Path

1. User clicks "Start" in MainWindow (`src/lib/gui/MainWindow.cpp`)
2. `CoreProcess::start()` decides process mode (foreground or daemon) (`src/lib/gui/core/CoreProcess.cpp`)
3. **Foreground mode:** Launches `deskflow-core` as `QProcess`, communicates via `CoreIpcClient` (QLocalSocket)
4. **Service/Daemon mode:** Sends commands via `DaemonIpcClient` to `deskflow-daemon` which spawns `deskflow-core`
5. IPC messages are newline-delimited text commands over `QLocalSocket` (`src/lib/deskflow/ipc/IpcServer.h`)

### Clipboard Sync Path

1. User copies text on server machine
2. Platform screen detects clipboard grab, posts event
3. `Server::handleClipboardGrabbed` serializes clipboard via `StreamChunker` (`src/lib/deskflow/StreamChunker.cpp`)
4. Chunks sent over protocol to client
5. `ServerProxy` on client reassembles and sets local clipboard via platform API

**State Management:**
- Global settings via `Settings` singleton (`src/lib/common/Settings.h`) backed by QSettings
- Per-process state: `App::s_instance` static pointer
- Runtime state: `EventQueue` singleton per core process
- GUI state: `MainWindow` holds `CoreProcess`, `ServerConfig`, `DaemonIpcClient`

## Key Abstractions

**IPlatformScreen:**
- Purpose: Abstract interface for all platform-specific screen operations (input capture, clipboard, keyboard, screen geometry)
- Examples: `src/lib/platform/MSWindowsScreen.h`, `src/lib/platform/OSXScreen.h`, `src/lib/platform/EiScreen.h`, `src/lib/platform/XWindowsScreen.h`
- Pattern: Interface with platform-specific implementations selected at compile time via CMake conditionals

**IpcServer / IpcClient:**
- Purpose: Inter-process communication between GUI, core, and daemon processes
- Examples: `src/lib/deskflow/ipc/IpcServer.h`, `src/lib/gui/ipc/IpcClient.h`
- Pattern: QLocalSocket-based text protocol; base classes with virtual `processCommand()` for command routing

**App (IApp):**
- Purpose: Base class for core application lifecycle (init, args parsing, main loop)
- Examples: `src/lib/deskflow/App.h`, `src/lib/deskflow/ClientApp.h`, `src/lib/deskflow/ServerApp.h`
- Pattern: Template method pattern -- base class defines lifecycle, subclasses override `parseArgs()`, `start()`, `mainLoop()`, etc.

**ISocketFactory:**
- Purpose: Abstract factory for creating TCP/TLS sockets
- Examples: `src/lib/net/TCPSocketFactory.h`
- Pattern: Factory pattern allowing injection of socket types

**EventQueue:**
- Purpose: Central event dispatch for asynchronous component communication in the core
- Examples: `src/lib/base/EventQueue.h`
- Pattern: Observer/dispatcher pattern; components register handlers for event types

## Entry Points

**deskflow (GUI):**
- Location: `src/apps/deskflow-gui/deskflow-gui.cpp`
- Triggers: User launches the GUI application
- Responsibilities: Parse CLI args, enforce single instance via QSharedMemory, initialize Qt application, create MainWindow, run Qt event loop

**deskflow-core:**
- Location: `src/apps/deskflow-core/deskflow-core.cpp`
- Triggers: GUI process launches it (foreground mode) or daemon launches it (service mode); also runnable standalone from CLI
- Responsibilities: Parse core args (--server/--client), create ClientApp or ServerApp, start IPC server, run core event loop on separate thread

**deskflow-daemon:**
- Location: `src/apps/deskflow-daemon/deskflow-daemon.cpp`
- Triggers: Windows service control manager, or CLI with --foreground flag
- Responsibilities: Run as Windows service, handle UAC elevation for secure desktops, manage core process lifecycle via IPC, logging to file

## Architectural Constraints

- **Threading:** Core logic runs on a dedicated `QThread` separate from the Qt event loop. The GUI runs on the main thread. Platform-specific code uses native threading (Windows threads, pthreads) wrapped by the `mt` library. Socket multiplexer uses its own thread.
- **Global state:** `App::s_instance` (static pointer in `src/lib/deskflow/App.h`), `Log` singleton via `Log::getInstance()` (`src/lib/base/Log.h`), `Settings::instance()` singleton (`src/lib/common/Settings.h`), `CoreIpcServer::instance()` static.
- **Circular imports:** The `platform` library depends on `client` (for `IClient` interface), while `client` depends on `app` on Unix. The dependency is managed through PUBLIC/PRIVATE linkage in CMake to break cycles.
- **Platform conditionals:** Heavy use of `#if defined(Q_OS_WIN)`, `#elif defined(Q_OS_MACOS)`, `else` (Unix) for platform selection. CMake conditionals (`if(WIN32)`, `elseif(APPLE)`, `elseif(UNIX)`) select which source files compile.
- **C++ standard:** C++20 required (set in root `CMakeLists.txt`).
- **Qt dependency:** All executables and most libraries depend on Qt6 (Core, Widgets, Network minimum). The `common` library is the lowest-level Qt-dependent lib.
- **No REST/HTTP:** The protocol between machines is a custom binary protocol over TCP (port 24800 default). IPC between processes on the same machine uses QLocalSocket (text protocol).

## Anti-Patterns

### Macro-based logging mixed with Qt logging

**What happens:** The codebase uses both the custom `LOG_*` / `CLOG` macros from `src/lib/base/Log.h` and `qDebug()`/`qInfo()` Qt logging. In `deskflow-core.cpp`, a custom `qtMessageHandler` bridges Qt messages to the CLOG system.
**Why it's wrong:** Two logging systems create confusion about which to use. The bridge handler in core must be installed manually.
**Do this instead:** When adding logging in core/library code, use `LOG_*` macros (`src/lib/base/Log.h`). In GUI code, use Qt logging (`qInfo`, `qWarning`, etc.) which gets routed through the GUI's message handler (`src/lib/gui/Messages.h`).

### Static singleton access patterns

**What happens:** `App::instance()`, `Settings::instance()`, and `Log::getInstance()` use static pointers with `assert()` guards.
**Why it's wrong:** Order-of-initialization and teardown issues; calling these before initialization crashes with assert failure.
**Do this instead:** Ensure singletons are initialized early in `main()` before any use. In `deskflow-core.cpp`, note the order: `Arch arch; arch.init();` then `Log log;` then `EventQueue events;`.

## Error Handling

**Strategy:** Multi-layered: C++ exceptions for recoverable errors, exit codes for process termination, Qt signals for IPC errors.

**Patterns:**
- Custom exception hierarchy rooted at `BaseException` (`src/lib/base/BaseException.h`) and `IOException` (`src/lib/io/IOException.h`)
- Domain exceptions: `DeskflowException` (`src/lib/deskflow/DeskflowException.h`), `SocketException` (`src/lib/net/SocketException.h`), `MTException` (`src/lib/mt/MTException.h`)
- Exit codes defined as constants in `src/lib/common/ExitCodes.h`: `s_exitSuccess` (0), `s_exitFailed` (1), `s_exitTerminated` (2), `s_exitArgs` (3), `s_exitConfig` (4), `s_exitDuplicate` (5)
- Qt signals for async error propagation: `CoreProcess::error()` signal, `IpcClient::connectionFailed()` signal
- Try/catch in daemon main (`src/apps/deskflow-daemon/deskflow-daemon.cpp`) catches all exceptions

## Cross-Cutting Concerns

**Logging:** Custom `Log` class (`src/lib/base/Log.h`) with pluggable outputters. Level hierarchy: FATAL, ERROR, WARNING, NOTE, INFO, DEBUG, DEBUG1, DEBUG2. GUI has its own message handler (`src/lib/gui/Messages.h`). Log output goes to console and optionally to file.

**Validation:** GUI input validation via validator classes in `src/lib/gui/validators/` (e.g., `AliasValidator`, `ScreenNameValidator`, `IpAddressValidator`). All inherit from `IStringValidator`.

**Configuration:** `Settings` singleton (`src/lib/common/Settings.h`) backed by QSettings. Settings keys organized in nested structs (Client, Core, Daemon, Gui, Log, Security, Server). Platform-specific file paths for config files.

**Internationalization:** Qt Linguist-based translations in `translations/` directory. `I18N` class in `src/lib/common/I18N.h` manages language loading. Currently supports: en, es, it, ja, ko, ru, zh_CN.

**Security:** TLS via OpenSSL for network connections (`src/lib/net/SecureSocket.cpp`). Fingerprint-based peer verification (`src/lib/net/FingerprintDatabase.cpp`). Certificate management via `TlsUtility` (`src/lib/gui/TlsUtility.cpp`). Peer fingerprint checking controlled by `Settings::Security::CheckPeers`.

---

*Architecture analysis: 2026-05-12*
