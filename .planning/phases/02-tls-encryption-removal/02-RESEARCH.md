# Phase 2: TLS/Encryption Removal - Research

**Researched:** 2026-05-13
**Domain:** C++ codebase cleanup -- removing TLS/SSL encryption layer from a KVM sharing tool
**Confidence:** HIGH

## Summary

Phase 2 removes the entire TLS/SSL encryption layer from Deskflow. The codebase has a well-defined security dispatch pattern: a `SecurityLevel` enum (PlainText/Encrypted/PeerAuth) flows through `ISocketFactory::create()` and `ISocketFactory::createListen()` into socket creation, where the old `TCPSocketFactory` creates `SecureSocket` or `TCPSocket` based on the level. The new `AsioTCPSocketFactory` (from Phase 1) already ignores the `SecurityLevel` parameter (`[[maybe_unused]]`), creating only plain-text `AsioTCPSocket` instances. This means the core network path is already plain-text-only in Phase 1's Asio path. The remaining work is cleanup: removing dead TLS code, eliminating the dispatch enum from interfaces, stripping GUI security controls, and removing the OpenSSL build dependency.

The TLS layer spans approximately 2,100 lines across 19 source files in `src/lib/net/` and `src/lib/gui/`, plus references in `src/lib/client/`, `src/lib/server/`, `src/lib/deskflow/`, and `src/lib/common/`. The Settings system stores 4 security-related keys. The GUI has a full Settings dialog section with certificate management UI, a fingerprint dialog, a fingerprint preview widget, and security icon/status bar integration. All of this must be removed.

**Primary recommendation:** Work in three waves -- (1) remove SecurityLevel from all interfaces and dispatch code, (2) delete all TLS implementation files and OpenSSL build deps, (3) strip GUI security controls and Settings keys. The AsioTCPSocketFactory already operates in plain-text mode, so the connection path is safe.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CLEAN-01 | Remove TLS/SSL encryption layer and all SecurityLevel related code (~3000 lines) | Full audit of SecurityLevel dispatch graph, TLS file inventory, GUI references, build deps completed below |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| SecurityLevel dispatch | Infrastructure (net) | Application (deskflow) | Factory pattern in net library dispatches on SecurityLevel; App layer passes it through |
| TLS socket implementation | Infrastructure (net) | -- | SecureSocket/SecureListenSocket are net library components |
| Certificate management | Infrastructure (net) + GUI | -- | SecureUtils generates certs in net; TlsUtility wraps it in GUI |
| Fingerprint verification | Infrastructure (net) + GUI | -- | FingerprintDatabase in net; FingerprintDialog in GUI |
| Security settings storage | Common (Settings) | GUI | Settings::Security keys in common; SettingsDialog reads/writes them |
| Security status display | GUI (StatusBar) | GUI (MainWindow) | StatusBar shows encryption icon; MainWindow manages state |
| OpenSSL dependency | Infrastructure (net/build) | -- | CMake find_package in net/CMakeLists.txt |

## Standard Stack

### Core (unchanged -- Phase 1 already set this)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Standalone Asio | 1.38.0 | Async I/O (header-only) | Phase 1 selection, already integrated |
| Qt 6 | 6.7.0+ | GUI framework | Project requirement |
| CMake | 3.24+ | Build system | Project requirement |

### What Gets Removed

| Component | Files | Lines | Purpose |
|-----------|-------|-------|---------|
| SecurityLevel enum | `src/lib/net/SecurityLevel.h` | 18 | Dispatch enum |
| SecureSocket | `src/lib/net/SecureSocket.{h,cpp}` | 845 | TLS socket wrapper over OpenSSL |
| SecureListenSocket | `src/lib/net/SecureListenSocket.{h,cpp}` | 91 | TLS listen socket |
| SecureUtils | `src/lib/net/SecureUtils.{h,cpp}` | 242 | Certificate generation, fingerprint formatting |
| SslLogger | `src/lib/net/SslLogger.{h,cpp}` | 193 | OpenSSL logging + IPC "secureSocket" messages |
| Fingerprint | `src/lib/net/Fingerprint.{h,cpp}` | 116 | Fingerprint data structure |
| FingerprintDatabase | `src/lib/net/FingerprintDatabase.{h,cpp}` | 121 | Fingerprint storage/trust |
| TlsUtility | `src/lib/gui/TlsUtility.{h,cpp}` | 171 | GUI certificate management wrapper |
| FingerprintDialog | `src/lib/gui/dialogs/FingerprintDialog.{h,cpp}` | 179 | Peer fingerprint verification dialog |
| FingerprintPreview | `src/lib/gui/widgets/FingerprintPreview.{h,cpp}` | 123 | Fingerprint display widget |
| TCPSocketFactory (old) | `src/lib/net/TCPSocketFactory.{h,cpp}` | 84 | Legacy factory with SecurityLevel dispatch -- may be removable if nothing uses it |
| TLS-related tests | `src/unittests/net/SecureUtilsTests.{h,cpp}`, `src/unittests/net/FingerprintTests.{h,cpp}`, `src/unittests/net/FingerprintDatabaseTests.{h,cpp}` | ~200 | Test files for removed code |

**Total files to remove:** ~22 (13 net + 5 gui + 4 tests)
**Total lines to remove:** ~2,400+ (implementation files) + ~200 (test files) + SettingsDialog UI section + Settings keys

### Installation Impact (removal, not addition)

```bash
# OpenSSL is no longer needed -- remove from build dependencies
# No new packages to install
```

## Architecture Patterns

### SecurityLevel Dispatch Graph (to be eliminated)

```text
Settings::Security::TlsEnabled + CheckPeers
    |
    v
ServerApp::openClientListener()          Client::Client()
    |                                        |
    v                                        v
SecurityLevel selection:                m_useSecureNetwork = TlsEnabled
  TlsEnabled + CheckPeers -> PeerAuth    |
  TlsEnabled              -> Encrypted   v
  !TlsEnabled             -> PlainText  securityLevel selection:
    |                                     PeerAuth or PlainText
    v                                        |
ClientListener(securityLevel)                v
    |                                    ISocketFactory::create(securityLevel)
    v                                        |
ClientListener::start()                      v
    |                                    AsioTCPSocketFactory (already ignores securityLevel)
    v                                    TCPSocketFactory (dispatches: PlainText -> TCPSocket, else -> SecureSocket)
ISocketFactory::createListen(securityLevel)
    |
    v
AsioTCPSocketFactory (already ignores securityLevel)
TCPSocketFactory (dispatches: PlainText -> TCPListenSocket, else -> SecureListenSocket)
```

**Key insight:** The AsioTCPSocketFactory (Phase 1) already ignores SecurityLevel with `[[maybe_unused]]`. The old TCPSocketFactory dispatches on it. After removing TLS, the SecurityLevel parameter becomes dead weight on the `ISocketFactory` interface.

### Recommended Project Structure (after removal)

```text
src/lib/net/
  AsioTCPSocket.h/cpp          # Phase 1: plain-text Asio socket
  AsioTCPListenSocket.h/cpp    # Phase 1: Asio listen socket
  AsioTCPSocketFactory.h/cpp   # Phase 1: factory (simplified, no securityLevel param)
  InputEventBuffer.h/cpp       # Phase 1: SPSC buffers
  KeyStateTable.h/cpp          # Phase 1: key state tracking
  IDataSocket.h/cpp            # Interface (unchanged)
  IListenSocket.h              # Interface (unchanged)
  ISocket.h                    # Interface (unchanged)
  ISocketFactory.h             # Interface (SecurityLevel param removed)
  NetworkAddress.h/cpp         # Address utility (unchanged)
  SocketException.h/cpp        # Error types (unchanged)
  TCPListenSocket.h/cpp        # Legacy (may be removable if unused)
  TCPSocket.h/cpp              # Legacy (may be removable if unused)
  SocketMultiplexer.h/cpp      # Legacy (Phase 4 target, not this phase)
  TSocketMultiplexerMethodJob.h # Legacy (Phase 4 target)

src/lib/gui/
  (TlsUtility.h/cpp REMOVED)
  dialogs/FingerprintDialog.h/cpp REMOVED
  widgets/FingerprintPreview.h/cpp REMOVED
  widgets/StatusBar.h/cpp        # Simplified: no security icon/level
  core/CoreProcess.h/cpp         # Simplified: no secureSocket signal
  MainWindow.h/cpp               # Simplified: no TLS/fingerprint code
  dialogs/SettingsDialog.h/cpp   # Simplified: no security group in UI
```

### Pattern 1: SecurityLevel Parameter Removal

**What:** The `SecurityLevel` enum is a parameter on `ISocketFactory::create()` and `createListen()`. Every caller passes it, but the new AsioTCPSocketFactory ignores it.

**Removal approach:**
1. Remove `SecurityLevel` parameter from `ISocketFactory` interface methods
2. Update `AsioTCPSocketFactory` to match new interface
3. Remove `SecurityLevel` from `ClientListener` constructor and member
4. Remove `SecurityLevel` calculation in `ServerApp::openClientListener()` and `Client::connect()`
5. Delete `src/lib/net/SecurityLevel.h`

**Example (before):**
```cpp
// ISocketFactory.h
virtual IDataSocket *create(
    IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet,
    SecurityLevel securityLevel = SecurityLevel::PlainText
) const = 0;
```

**Example (after):**
```cpp
// ISocketFactory.h
virtual IDataSocket *create(
    IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet
) const = 0;
```

### Anti-Patterns to Avoid

- **Leaving dead SecurityLevel references:** Even a single `#include "net/SecurityLevel.h"` will cause build failure after the file is deleted. Grep must be exhaustive.
- **Removing TCPSocketFactory/TCPSocket/SocketMultiplexer in this phase:** These are legacy components targeted by Phase 4 (modernization). They still compile and some may still be referenced. Do NOT remove them in Phase 2 -- only remove their TLS dispatch code (the `SecureSocket`/`SecureListenSocket` creation paths).
- **Breaking the Settings::validKeys list:** The Settings class has a `m_validKeys` list that includes all security keys. Removing the keys from the struct but forgetting the list will cause issues.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| N/A (removal phase) | N/A | N/A | This phase removes code, not adds it |

**Key insight:** This is purely a removal/cleanup phase. No new functionality is being built. The risk is in incomplete removal leaving broken references.

## Runtime State Inventory

> This is a removal phase that touches configuration and data storage.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | `Settings::Security::TlsEnabled` (defaults true), `Settings::Security::CheckPeers` (defaults true), `Settings::Security::Certificate` (path to .pem), `Settings::Security::KeySize` (2048) in QSettings .conf files | Remove keys from Settings class; existing user config files will have orphan keys (harmless, cleaned by cleanSettings) |
| Stored data | TLS certificate .pem file at `~/Library/Deskflow/tls/Deskflow.pem` (macOS) or equivalent | No action needed -- orphan file, harmless |
| Stored data | Trusted fingerprint databases at `~/Library/Deskflow/tls/trusted-servers` and `trusted-clients` | No action needed -- orphan files, harmless |
| Live service config | No runtime services embed TLS strings in external config | None |
| OS-registered state | None | None -- verified by inspection |
| Secrets/env vars | None | None -- TLS cert/key stored as files, not env vars |
| Build artifacts | `build/` directory contains CMake cache referencing OpenSSL | `cmake` reconfigure after removing OpenSSL deps will handle this |

**Nothing found in category:** No OS-registered state, no external service config, no secrets in env vars. All TLS state is either in QSettings keys (code-managed) or .pem files on disk (user-managed, harmless orphans).

## Common Pitfalls

### Pitfall 1: Incomplete SecurityLevel include cleanup
**What goes wrong:** After deleting `SecurityLevel.h`, any file that includes it will fail to compile.
**Why it happens:** The enum is included indirectly through `ISocketFactory.h` which is widely used.
**How to avoid:** Remove `#include "net/SecurityLevel.h"` from `ISocketFactory.h` FIRST, then grep for all remaining references.
**Warning signs:** Build failure: "SecurityLevel: undeclared identifier" or "net/SecurityLevel.h: No such file"

### Pitfall 2: SettingsDialog UI orphan widgets
**What goes wrong:** Removing code that references `ui->groupSecurity` etc. but leaving the widgets in `SettingsDialog.ui` causes runtime issues or build warnings.
**Why it happens:** The .ui file defines widget layout; the .cpp accesses widgets by name. Removing one without the other breaks the connection.
**How to avoid:** Remove the entire `groupSecurity` QGroupBox and all child widgets from `SettingsDialog.ui` in one pass.
**Warning signs:** Runtime: empty space in Settings dialog, or "QGroupBox::setChecked" on null widget.

### Pitfall 3: Forgetting the IPC "secureSocket" message path
**What goes wrong:** `SslLogger::logSecureConnectInfo()` sends `"secureSocket"` IPC to the GUI, which triggers `CoreProcess::secureSocket()` signal -> `MainWindow::secureSocket()` -> `StatusBar::setSecurityIcon()`. Removing the sender without removing the receiver leaves dead signal connections.
**Why it happens:** The IPC path spans 3 files: SslLogger (net) -> CoreProcess (gui/core) -> MainWindow (gui).
**How to avoid:** Remove the entire chain: SslLogger (deleted), CoreProcess::checkSecureSocket() (remove method), CoreProcess::secureSocket signal (remove), MainWindow::secureSocket slot (remove), StatusBar::setSecurityIcon() (remove or simplify).
**Warning signs:** No build failure (dead signal is harmless), but dead code remains.

### Pitfall 4: Forgetting the IPC "peerFingerprint" message path
**What goes wrong:** `SecureSocket::verifyCertFingerprint()` sends `"peerFingerprint"` IPC to GUI, triggering `MainWindow::handlePeerFingerprint()` which shows the FingerprintDialog. Removing sender without receiver leaves dead code.
**Why it happens:** Similar to secureSocket path, spans multiple files.
**How to avoid:** Remove the entire chain: SecureSocket (deleted), CoreProcess::peerFingerprint signal (remove), MainWindow::handlePeerFingerprint (remove), MainWindow::m_checkedClients/m_checkedServers (remove), FingerprintDialog (deleted).

### Pitfall 5: CMake PUBLIC linkage propagation
**What goes wrong:** `net` library links `PUBLIC OpenSSL::SSL OpenSSL::Crypto`. Removing these from `net/CMakeLists.txt` affects all targets that link `net`.
**Why it happens:** PUBLIC linkage means downstream targets also get OpenSSL on their link line.
**How to avoid:** After removing OpenSSL from net, check that no other target directly uses OpenSSL headers. The `gui` library links `net` on Windows only (`if(WIN32)`), but gui's `TlsUtility.cpp` includes `net/SecureUtils.h` which includes `<openssl/x509.h>`. Since both TlsUtility and SecureUtils are being deleted, this is safe.
**Warning signs:** Build failure: "undefined reference to SSL_*" or "OpenSSL headers not found".

### Pitfall 6: TCPSocketFactory still references SecureSocket
**What goes wrong:** The old `TCPSocketFactory` creates `SecureSocket` instances. After deleting SecureSocket files, TCPSocketFactory won't compile.
**Why it happens:** TCPSocketFactory is the legacy factory -- it's still in the build even though AsioTCPSocketFactory is the active one.
**How to avoid:** Either (a) remove TCPSocketFactory entirely (risky -- check if anything still uses it), or (b) strip its SecurityLevel dispatch to always create plain-text TCPSocket. Option (b) is safer. Check: ServerApp and ClientApp both use AsioTCPSocketFactory now, so TCPSocketFactory may be fully unused.
**Warning signs:** Build failure: "SecureSocket: undeclared identifier".

### Pitfall 7: Logger.cpp suppresses TLS-related Qt messages
**What goes wrong:** `src/lib/gui/Logger.cpp` has a `kForceDebugMessages` list that suppresses Qt TLS backend warnings. After TLS removal, these are irrelevant but not harmful.
**Why it happens:** The filter was added to suppress noise from Qt's SSL module.
**How to avoid:** Remove the 3 TLS-related entries from `kForceDebugMessages` to keep the list clean.
**Warning signs:** No functional issue, but dead strings in the filter list.

## Code Examples

### ISocketFactory interface simplification (VERIFIED from codebase)

```cpp
// Before (src/lib/net/ISocketFactory.h):
#include "net/SecurityLevel.h"

class ISocketFactory {
public:
  virtual ~ISocketFactory() = default;
  virtual IDataSocket *create(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet,
      SecurityLevel securityLevel = SecurityLevel::PlainText
  ) const = 0;
  virtual IListenSocket *createListen(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet,
      SecurityLevel securityLevel = SecurityLevel::PlainText
  ) const = 0;
};

// After:
// (SecurityLevel.h include removed, parameter removed from both methods)
class ISocketFactory {
public:
  virtual ~ISocketFactory() = default;
  virtual IDataSocket *create(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet
  ) const = 0;
  virtual IListenSocket *createListen(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet
  ) const = 0;
};
```

### ServerApp::openClientListener simplification (VERIFIED from codebase)

```cpp
// Before (src/lib/deskflow/ServerApp.cpp:441-453):
ClientListener *ServerApp::openClientListener(const NetworkAddress &address)
{
  using enum SecurityLevel;
  auto securityLevel = PlainText;
  if (Settings::value(Settings::Security::TlsEnabled).toBool()) {
    if (Settings::value(Settings::Security::CheckPeers).toBool()) {
      securityLevel = PeerAuth;
    } else {
      securityLevel = Encrypted;
    }
  }
  auto *listen = new ClientListener(getAddress(address), getSocketFactory(), getEvents(), securityLevel);
  // ...
}

// After:
ClientListener *ServerApp::openClientListener(const NetworkAddress &address)
{
  auto *listen = new ClientListener(getAddress(address), getSocketFactory(), getEvents());
  // ...
}
```

### Client::connect simplification (VERIFIED from codebase)

```cpp
// Before (src/lib/client/Client.cpp:85-106):
auto securityLevel = m_useSecureNetwork ? SecurityLevel::PeerAuth : SecurityLevel::PlainText;
// ...
IDataSocket *socket = m_socketFactory->create(ARCH->getAddrFamily(...), securityLevel);

// After:
// (m_useSecureNetwork member removed, securityLevel calculation removed)
IDataSocket *socket = m_socketFactory->create(ARCH->getAddrFamily(m_serverAddress.getAddress()));
```

### ClientListener simplification (VERIFIED from codebase)

```cpp
// Before: constructor takes SecurityLevel, stores in m_securityLevel, passes to factory
// After: constructor takes no SecurityLevel, no member, always plain-text

// Before (ClientListener::handleClientConnecting):
if (m_securityLevel == SecurityLevel::PlainText) {
  m_events->addEvent(Event(EventTypes::ClientListenerAccepted, ...));
}

// After: Always immediately emit accepted (was only done for PlainText before)
m_events->addEvent(Event(EventTypes::ClientListenerAccepted, rawSocketPointer->getEventTarget()));
```

### Settings::Security keys removal (VERIFIED from codebase)

```cpp
// Remove from Settings.h:
// struct Security { CheckPeers, Certificate, KeySize, TlsEnabled };
// Remove from m_validKeys list
// Remove from m_defaultTrueValues list (TlsEnabled, CheckPeers)
// Remove: tlsDir(), tlsTrustedServersDb(), tlsTrustedClientsDb()
// Remove defaultValue() cases for Certificate and KeySize

// Remove from Settings.cpp:
// Settings::tlsDir() implementation
// Settings::tlsTrustedServersDb() implementation
// Settings::tlsTrustedClientsDb() implementation
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SecurityLevel dispatch in ISocketFactory | AsioTCPSocketFactory ignores SecurityLevel | Phase 1 (2026-05-13) | Removal is safe -- active code path is already plain-text |
| TCPSocketFactory creates SecureSocket | TCPSocketFactory is legacy, unused by active path | Phase 1 (2026-05-13) | TCPSocketFactory must be cleaned or removed to compile |

**Deprecated/outdated:**
- `TCPSocketFactory`: Still compiled but not used by ServerApp/ClientApp (both use AsioTCPSocketFactory). The TLS dispatch code in TCPSocketFactory references SecureSocket which is being deleted. TCPSocketFactory must be either cleaned (remove SecureSocket references) or fully removed.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | TCPSocketFactory is not used by any active code path | Standard Stack | If wrong, removing it breaks functionality. Verified: ServerApp and ClientApp both use AsioTCPSocketFactory. |
| A2 | No code outside of `src/lib/net/`, `src/lib/gui/`, `src/lib/client/`, `src/lib/server/`, `src/lib/deskflow/`, `src/lib/common/` references TLS-related code | Common Pitfalls | If wrong, build failure in unexamined files. Verified: grep covered full src/ tree. |
| A3 | The `net` library is the only CMake target that directly links OpenSSL | Common Pitfalls | If wrong, other targets may break. Verified: grep found OpenSSL only in `src/lib/net/CMakeLists.txt`. |
| A4 | The `build/` directory will be regenerated by CMake after removing OpenSSL deps | Runtime State Inventory | If wrong, stale CMake cache may cause build issues. User can delete build/ and reconfigure. |

## Open Questions

1. **Should TCPSocketFactory be removed entirely or just cleaned?**
   - What we know: Both ServerApp and ClientApp use AsioTCPSocketFactory. TCPSocketFactory is in the net library's CMakeLists.txt compilation list.
   - What's unclear: Whether any platform-specific code or test creates a TCPSocketFactory directly.
   - Recommendation: Grep for `TCPSocketFactory` usage outside of net/ at plan time. If none, remove it entirely. If found, clean the SecurityLevel dispatch from it.

2. **Should the Settings::Security struct be removed entirely?**
   - What we know: All 4 keys (CheckPeers, Certificate, KeySize, TlsEnabled) are TLS-specific.
   - What's unclear: Whether external config files rely on these keys existing.
   - Recommendation: Remove the struct and all references. Existing config files with these keys are harmless orphans (QSettings ignores unknown keys).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Build system | Yes | 3.24+ | -- |
| Qt 6 | GUI | Yes | 6.7+ | -- |
| Standalone Asio | Network I/O | Yes (vendor/) | 1.38.0 | -- |
| OpenSSL | TLS (being removed) | Yes (will be removed) | 3.0+ | N/A |
| Clang (macOS) | Compiler | Yes | system | -- |

**Missing dependencies with no fallback:** None -- this phase removes a dependency, not adds one.

**Missing dependencies with fallback:** N/A

## Complete File Impact Inventory

### Files to DELETE entirely:

**net library (src/lib/net/):**
- `SecurityLevel.h` (18 lines)
- `SecureSocket.h` + `SecureSocket.cpp` (845 lines)
- `SecureListenSocket.h` + `SecureListenSocket.cpp` (91 lines)
- `SecureUtils.h` + `SecureUtils.cpp` (242 lines)
- `SslLogger.h` + `SslLogger.cpp` (193 lines)
- `Fingerprint.h` + `Fingerprint.cpp` (116 lines)
- `FingerprintDatabase.h` + `FingerprintDatabase.cpp` (121 lines)

**gui library (src/lib/gui/):**
- `TlsUtility.h` + `TlsUtility.cpp` (171 lines)
- `dialogs/FingerprintDialog.h` + `dialogs/FingerprintDialog.cpp` (179 lines)
- `widgets/FingerprintPreview.h` + `widgets/FingerprintPreview.cpp` (123 lines)

**test files (src/unittests/):**
- `net/SecureUtilsTests.h` + `net/SecureUtilsTests.cpp`
- `net/FingerprintTests.h` + `net/FingerprintTests.cpp`
- `net/FingerprintDatabaseTests.h` + `net/FingerprintDatabaseTests.cpp`
- `common/SettingsTests.cpp` (remove TLS-related test methods: `tlsDir()`, `tlsTrustedServersDb()`, `tlsTrustedClientsDb()`)

### Files to MODIFY:

**net library:**
- `CMakeLists.txt` -- remove OpenSSL find_package, remove OpenSSL:: link deps, remove deleted files from source list, remove Apple static lib comment
- `ISocketFactory.h` -- remove `#include "net/SecurityLevel.h"`, remove SecurityLevel params
- `AsioTCPSocketFactory.h` -- remove SecurityLevel params
- `AsioTCPSocketFactory.cpp` -- remove SecurityLevel params
- `TCPSocketFactory.h` -- remove SecurityLevel params, remove SecureSocket includes
- `TCPSocketFactory.cpp` -- remove SecurityLevel dispatch, remove SecureSocket/SecureListenSocket includes and creation

**client library:**
- `src/lib/client/Client.h` -- remove `m_useSecureNetwork` member
- `src/lib/client/Client.cpp` -- remove `#include "net/SecureSocket.h"`, remove `#include "net/TCPSocket.h"`, remove SecurityLevel usage in connect(), remove m_useSecureNetwork init

**server library:**
- `src/lib/server/ClientListener.h` -- remove `#include "net/SecurityLevel.h"`, remove SecurityLevel from constructor and member
- `src/lib/server/ClientListener.cpp` -- remove SecurityLevel from constructor, remove PlainText check in handleClientConnecting(), always emit accepted

**app library:**
- `src/lib/deskflow/ServerApp.cpp` -- remove SecurityLevel selection logic, simplify openClientListener(), remove SecurityLevel-related includes

**common library:**
- `src/lib/common/Settings.h` -- remove `struct Security` entirely, remove Security keys from `m_validKeys`, remove from `m_defaultTrueValues`, remove `tlsDir()`, `tlsTrustedServersDb()`, `tlsTrustedClientsDb()` declarations
- `src/lib/common/Settings.cpp` -- remove `tlsDir()`, `tlsTrustedServersDb()`, `tlsTrustedClientsDb()` implementations, remove Certificate/KeySize default value cases

**gui library:**
- `src/lib/gui/CMakeLists.txt` -- remove TlsUtility.cpp/h, FingerprintDialog, FingerprintPreview from source list
- `src/lib/gui/MainWindow.h` -- remove `m_fingerprint`, `m_secureSocket`, `m_checkedClients`, `m_checkedServers` members; remove `showMyFingerprint()`, `updateSecurityIcon()`, `handlePeerFingerprint()`, `secureSocket()`, `updateLocalFingerprint()`, `trustedFingerprintDatabase()`, `generateCertificate()` methods; remove `#include "net/Fingerprint.h"`
- `src/lib/gui/MainWindow.cpp` -- remove all TLS/fingerprint related code (~30 call sites)
- `src/lib/gui/MainWindow.ui` -- check for any TLS-related UI elements
- `src/lib/gui/widgets/StatusBar.h` -- remove `setSecurityLevel()`, `setSecurityIcon()`, `setSecurityIconVisible()`, `updateSecurityInfo()`, `setBtnFingerprintVisible()`, `m_securityLevel`, `m_encrypted`, `m_btnFingerprint`, `m_lblSecurityIcon`
- `src/lib/gui/widgets/StatusBar.cpp` -- remove security icon/level code, remove fingerprint button
- `src/lib/gui/core/CoreProcess.h` -- remove `secureSocketVersion()`, `secureSocket` signal, `securityLevelChanged` signal, `m_secureSocketVersion`, `checkSecureSocket()` method
- `src/lib/gui/core/CoreProcess.cpp` -- remove `checkSecureSocket()` implementation, remove `"secureSocket"` IPC handling, remove `"peerFingerprint"` IPC handling, remove `tlsCheckString`
- `src/lib/gui/dialogs/SettingsDialog.h` -- remove `updateTlsControls()`, `updateTlsControlsEnabled()`
- `src/lib/gui/dialogs/SettingsDialog.cpp` -- remove all TLS-related code (~20 call sites)
- `src/lib/gui/dialogs/SettingsDialog.ui` -- remove entire `groupSecurity` QGroupBox and all child widgets
- `src/lib/gui/Messages.cpp` -- remove `showNewClientPrompt()` TLS check (lines 189-190)
- `src/lib/gui/Logger.cpp` -- remove TLS-related entries from `kForceDebugMessages`

**build files:**
- `CMakeLists.txt` (root) -- remove `REQUIRED_OPENSSL_VERSION` variable, remove `LicenseRef-OpenSSL-Exception.txt` from install files (or keep the license file for historical accuracy)

## Sources

### Primary (HIGH confidence)
- Codebase audit via grep/read of all files listed above -- direct source code inspection
- Phase 1 CONTEXT.md and REVIEW.md for understanding current Asio integration
- `src/lib/net/AsioTCPSocketFactory.cpp` confirms `[[maybe_unused]] SecurityLevel` (already ignoring the param)

### Secondary (MEDIUM confidence)
- CONCERNS.md -- documents TLS certificate security issues and OpenSSL dependency risk
- ARCHITECTURE.md -- documents SecurityLevel dispatch flow and TLS layer position

## Metadata

**Confidence breakdown:**
- Standard stack (removal targets): HIGH - all files verified by direct codebase grep
- Architecture (dispatch graph): HIGH - traced through all 6 dispatch points
- Pitfalls: HIGH - all pitfall scenarios verified against actual code
- GUI impact: HIGH - all UI widgets and signal chains traced

**Research date:** 2026-05-13
**Valid until:** 30 days (stable codebase, no fast-moving dependencies)
