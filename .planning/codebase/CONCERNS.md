# Codebase Concerns

**Analysis Date:** 2026-05-12

## Tech Debt

**SocketMultiplexer manual memory management:**
- Issue: Raw `new`/`delete` throughout, custom cursor-mark sentinel pattern using `reinterpret_cast` for list traversal. The author themselves left a `// TODO: Remove this evilness` comment at line 35.
- Files: `src/lib/net/SocketMultiplexer.cpp`
- Impact: Use-after-free or double-delete if exception is thrown between `new` and assignment. The `lockJobListLock` method allocates a `Thread` object via `new` at line 272 that is later deleted in `unlockJobList` at line 305 -- if the lock path is not perfectly symmetric, it leaks or double-deletes.
- Fix approach: Replace with `std::unique_ptr` / `std::shared_ptr` for socket jobs. Replace the `reinterpret_cast<ISocketMultiplexerJob*>(this)` cursor mark with a proper sentinel type or index-based iteration.

**Massive monolithic files with mixed responsibilities:**
- Issue: Several core files exceed 1500 lines, mixing platform logic, event handling, and business rules.
- Files:
  - `src/lib/server/Config.cpp` (2074 lines)
  - `src/lib/server/Server.cpp` (2071 lines)
  - `src/lib/platform/XWindowsScreen.cpp` (1952 lines)
  - `src/lib/platform/OSXScreen.mm` (1936 lines)
  - `src/lib/platform/MSWindowsScreen.cpp` (1736 lines)
  - `src/lib/platform/XDGKeyUtil.cpp` (1593 lines)
  - `src/lib/gui/MainWindow.cpp` (1274 lines)
- Impact: Hard to understand, test, and modify. Changes in one concern risk regressions in another.
- Fix approach: Extract focused helper classes. For example, split `Config.cpp` into serialization, validation, and neighbor-management modules. Split `MainWindow.cpp` into individual dialog/screen controllers.

**92 TODO/FIXME/HACK/XXX comments in source:**
- Issue: Accumulated technical debt markers across the codebase, concentrated in platform layer.
- Files: Concentrated in `src/lib/platform/` (60+ comments) and `src/lib/deskflow/KeyMap.cpp` (6 comments)
- Impact: Known unimplemented features, race conditions acknowledged but not fixed.
- Fix approach: Triage by severity. The `FIXME` items in `XWindowsClipboard.cpp` (race conditions, incorrect logic) should be prioritized over cosmetic `XXX` comments.

**Legacy test framework:**
- Issue: A `src/unittests/legacytests/` directory exists with its own `main.cpp`, mock objects, and shared utilities. The entry point hack (`// HACK: shouldn't be needed, but logging fails without this`) indicates infrastructure coupling.
- Files: `src/unittests/legacytests/legacytests/main.cpp`, `src/unittests/legacytests/mock/`, `src/unittests/legacytests/shared/`
- Impact: Duplicate mock infrastructure, harder to maintain consistent testing patterns.
- Fix approach: Migrate legacy tests to the same framework used by `src/unittests/deskflow/`, `src/unittests/net/`, etc. Consolidate mock objects.

**Self-signed TLS certificate generation writes private key to disk:**
- Issue: `generatePemSelfSignedCert` in `SecureUtils.cpp` writes both the private key and certificate to the same file using C-style `fopen`/`fclose`.
- Files: `src/lib/net/SecureUtils.cpp` lines 65-108
- Impact: File permissions may not restrict access to the private key. Using `fopen` with `"w"` mode does not set restrictive permissions (0600). On shared systems, private key could be readable by other users.
- Fix approach: Use `QSaveFile` with explicit permission setting, or ensure `umask` is applied. Separate private key and certificate into different files with appropriate permissions.

## Known Bugs

**`exit(0)` called in RestartServer action:**
- Symptoms: The server restart action in the input filter calls `exit(0)` directly, bypassing all cleanup (RAII destructors, socket teardown, TLS shutdown).
- Files: `src/lib/server/InputFilter.cpp` lines 262-266
- Trigger: Configured hotkey or input filter rule that triggers a restart.
- Workaround: The comment `// HACK Super hack we should gracefully exit` acknowledges the issue.
- Fix: Post a quit event to the event queue instead of calling `exit(0)`.

**ServerConfigTests linking disabled:**
- Symptoms: Network address setup in `equalityCheck_compatible()` is commented out with `/* TODO Fix linking to the proper libs */`.
- Files: `src/unittests/server/ServerConfigTests.cpp` lines 60-67, 109
- Trigger: The test passes but without verifying the network address equality part.
- Workaround: Test runs but with reduced coverage.

**PrimaryClient silently discards key events:**
- Symptoms: `keyDown` and `keyUp` in `PrimaryClient` have their actual implementation commented out, with `// XXX -- don't forward keystrokes to primary screen for now`.
- Files: `src/lib/server/PrimaryClient.cpp` lines 142-166
- Trigger: Keystroke forwarding to the primary screen never happens.
- Workaround: None -- this is intentionally disabled behavior, but the commented-out code is dead code.

## Security Considerations

**TLS certificate verification bypassed with callback that always returns 1:**
- Risk: `verifyIgnoreCertCallback` at `src/lib/net/SecureSocket.cpp` line 42-45 returns 1 unconditionally. This is used for `PeerAuth` security level -- the server asks for the peer certificate but ignores its validity. The only actual verification is fingerprint-based (checking against a local database), but X.509 chain validation is entirely skipped.
- Files: `src/lib/net/SecureSocket.cpp` lines 42-45, 366-371
- Current mitigation: Fingerprint verification (`verifyCertFingerprint`) provides TOFU (trust-on-first-use) style authentication. If the fingerprint database is compromised, an attacker could MITM.
- Recommendations: Add optional full X.509 chain validation mode. Document the security model clearly so users understand the trust model.

**`_CRT_SECURE_NO_WARNINGS` defined globally on Windows:**
- Risk: Suppresses all MSVC security warnings for deprecated CRT functions (sprintf, strcpy, etc.), which can hide buffer overflow vulnerabilities.
- Files: `src/lib/cmake/Libraries.cmake` line 17
- Current mitigation: None.
- Recommendations: Remove global suppression and fix individual warnings. Use `sprintf_s`, `strcpy_s`, or better yet, `std::string` / `std::format`.

**Deprecated macOS API usage silenced with pragmas:**
- Risk: Multiple files suppress `-Wdeprecated-declarations` warnings, meaning deprecated macOS APIs are being used. These can be removed by Apple in future macOS versions, causing runtime crashes.
- Files:
  - `src/lib/platform/OSXKeyState.cpp` line 17-18
  - `src/lib/platform/OSXScreenSaver.cpp` line 136
  - `src/lib/platform/OSXScreen.mm` line 44-45
  - `src/lib/gui/OSXHelpers.mm` line 19-20
  - `src/lib/deskflow/App.cpp` line 74-75
- Current mitigation: Pragmas suppress the compiler errors, but do not fix the underlying issue.
- Recommendations: Audit each deprecated usage and migrate to modern replacements (e.g., CGEvent API replacements for older Carbon calls).

**No file permission enforcement on TLS fingerprint database:**
- Risk: `FingerprintDatabase::write` uses `QFile` with `QIODevice::WriteOnly` without setting file permissions. If the fingerprint database is tampered with, TOFU trust model is compromised.
- Files: `src/lib/net/FingerprintDatabase.cpp` lines 47-54
- Current mitigation: None visible.
- Recommendations: Set file permissions to owner-only (0600) after writing the fingerprint database.

## Performance Bottlenecks

**Busy-polling in MSWindowsWatchdog:**
- Problem: The watchdog uses `Arch::sleep(0.1)` in its main loop, polling every 100ms.
- Files: `src/lib/platform/MSWindowsWatchdog.cpp` lines 222-223, 349
- Cause: No event-driven notification mechanism for process state changes.
- Improvement path: Use Windows `WaitForSingleObject` on the process handle instead of sleep-polling.

**Sleep-based polling in XWindowsScreen:**
- Problem: Uses `Arch::sleep(0.05)` (50ms) in polling loops.
- Files: `src/lib/platform/XWindowsScreen.cpp` lines 1861, 1878
- Cause: Used for waiting on state changes that lack event-driven notification.
- Improvement path: Use X11 event-driven mechanisms or condition variables.

**WlClipboard polling monitor:**
- Problem: Uses `std::this_thread::sleep_for` for clipboard monitoring intervals.
- Files: `src/lib/platform/WlClipboard.cpp` line 346
- Cause: No Wayland clipboard change notification available.
- Improvement path: Use file descriptor polling with `poll()` on the Wayland display fd.

**Inefficient cursor position tracking in OSXScreen:**
- Problem: Comment `// XXX -- this is inefficient` at line 383.
- Files: `src/lib/platform/OSXScreen.mm` line 383
- Cause: Likely polling-based cursor tracking rather than event-driven.
- Improvement path: Use CGEvent tap for cursor movement notifications.

## Fragile Areas

**SocketMultiplexer threading model:**
- Files: `src/lib/net/SocketMultiplexer.cpp`
- Why fragile: Custom two-phase locking using `CondVar<bool>` flags (`m_jobListLock`, `m_jobListLockLocked`, `m_jobListLocker`, `m_jobListLockLocker`). The lock ownership is tracked via raw `Thread` pointers that are dynamically allocated/deleted. The `assert` at line 280 checks thread identity but assertions are disabled in release builds.
- Safe modification: Any change to the locking protocol requires understanding the full state machine. Consider replacing with `std::mutex` + `std::condition_variable` + `std::lock_guard`.
- Test coverage: No dedicated unit tests for `SocketMultiplexer`.

**XWindowsClipboard race conditions:**
- Files: `src/lib/platform/XWindowsClipboard.cpp`
- Why fragile: Multiple `FIXME` comments acknowledge race conditions at lines 557, 589, 745, 966. X11 selection ownership transfers are inherently asynchronous, and the code has timeout-based heuristics (line 1209: `static const double s_timeout = 0.25; // FIXME -- is this too short?`).
- Safe modification: Changes to clipboard handling require testing against multiple X11 clipboard managers and compositors.
- Test coverage: `src/unittests/platform/XWindowsClipboardTests.cpp` (77 lines) provides minimal coverage.

**Platform-specific screen implementations with unimplemented methods:**
- Files:
  - `src/lib/platform/EiScreen.cpp` (6 FIXME items, screensaver not implemented)
  - `src/lib/platform/OSXScreen.mm` (multiple FIXME for not-implemented features)
  - `src/lib/platform/XWindowsScreen.cpp` (FIXME not implemented at lines 725, 730)
- Why fragile: Each platform screen has different sets of unimplemented features. Adding cross-platform features requires checking which platform actually supports them.
- Safe modification: Any new screen feature must be implemented for all platforms, or the interface must be updated to make features optional.
- Test coverage: Platform screen implementations have minimal test coverage; most testing requires actual hardware/display.

**OSXScreen mutable button state:**
- Files: `src/lib/platform/OSXScreen.h` lines 239-245
- Why fragile: `m_buttonState` is declared `mutable` with a FIXME comment acknowledging this is "Evil." The mutation happens in `const` methods via `fakeMouseButton`, which violates logical constness.
- Safe modification: Refactoring the button state out of the screen class would require changing the `IPlatformScreen` interface.
- Test coverage: `src/unittests/platform/OSXKeyStateTests.cpp` tests key state but not button state tracking.

## Scaling Limits

**Single-threaded socket service loop:**
- Current capacity: One `SocketMultiplexer` thread services all connections.
- Limit: All socket I/O (accept, read, write, TLS handshake) goes through a single service thread. During TLS handshakes, all other connections are blocked.
- Files: `src/lib/net/SocketMultiplexer.cpp` line 122 (`serviceThread`)
- Scaling path: Add per-connection worker threads or use an io_uring/proactor pattern. The code already acknowledges the DoS risk from blocking handshakes (line 427: `Historically a 1s sleep let any failed handshake DoS all clients`).

**Protocol message parsing buffer sizes:**
- Current capacity: `kMaxHelloLength = 1024` bytes for hello messages.
- Limit: Fixed buffer sizes may be insufficient for future protocol extensions.
- Files: `src/lib/deskflow/ProtocolTypes.h` line 68
- Scaling path: Make buffer sizes configurable or use variable-length framing.

**Static `retry` variable in SecureSocket:**
- Current capacity: Single connection state tracking.
- Limit: `static int retry` at line 421 in `SecureSocket.cpp` is shared across all instances in the same translation unit. If multiple `SecureSocket` instances call `secureAccept` concurrently, the retry counter will be corrupted.
- Files: `src/lib/net/SecureSocket.cpp` line 421
- Scaling path: Convert to instance member variable.

## Dependencies at Risk

**OpenSSL API:**
- Risk: Direct usage of low-level OpenSSL APIs (SSL_*, X509_*, EVP_*) throughout `SecureSocket.cpp` and `SecureUtils.cpp`. OpenSSL 3.x deprecates many low-level APIs in favor of provider-based APIs.
- Impact: Future OpenSSL versions may remove deprecated functions, breaking TLS support.
- Migration plan: Wrap OpenSSL calls behind a `TlsContext` abstraction. Consider using Qt's SSL classes (`QSslSocket`, `QSslCertificate`) which already abstract OpenSSL details.

**Deprecated protocol message types:**
- Risk: 13 deprecated message types and fields remain in `ProtocolTypes.h` (file drag-and-drop, legacy key events, heartbeat settings).
- Impact: Dead code increases binary size and maintenance burden. Future developers may accidentally use deprecated message types.
- Migration plan: Mark deprecated protocol messages with `[[deprecated]]` attribute and remove handlers for unused messages.

## Missing Critical Features

**No compiler warning enforcement:**
- Problem: The CMake configuration does not enable `-Wall -Wextra -Werror` (or MSVC equivalents). The Windows build defines `_CRT_SECURE_NO_WARNINGS` to suppress security warnings. No warning flags are set in `CMakeLists.txt` or `cmake/Libraries.cmake`.
- Blocks: Preventing regressions and maintaining code quality.

**No test coverage enforcement:**
- Problem: Code coverage is available via `cmake/CodeCoverage.cmake` and `ENABLE_COVERAGE` option, but not enforced. SonarCloud configuration exists (`sonar-project.properties`) but coverage exclusions skip significant portions (unittests, GUI, resources).
- Blocks: Confidence in refactoring and change safety.

**EiScreen screensaver control missing:**
- Problem: `openScreensaver`, `closeScreensaver`, and `screensaver` methods are empty stubs (just `// FIXME`).
- Files: `src/lib/platform/EiScreen.cpp` lines 428-441
- Blocks: Screensaver inhibition does not work on Wayland/EI-based setups.

## Test Coverage Gaps

**SocketMultiplexer untested:**
- What's not tested: Connection management, job scheduling, thread safety of the lock protocol, socket addition/removal.
- Files: `src/lib/net/SocketMultiplexer.cpp` (317 lines)
- Risk: High -- this is the core networking component with complex threading. Any regression could cause deadlocks or data races.
- Priority: High

**Server core logic sparsely tested:**
- What's not tested: Most of `Server.cpp` (2071 lines) and `Config.cpp` (2074 lines). Only `ServerConfigTests.cpp` (164 lines) exists.
- Files: `src/lib/server/Server.cpp`, `src/lib/server/Config.cpp`
- Risk: High -- server coordinate calculation, screen switching, and clipboard synchronization are untested.
- Priority: High

**Platform screens have minimal coverage:**
- What's not tested: Screen shape detection, input injection, cursor warping on all platforms.
- Files: `src/lib/platform/XWindowsScreen.cpp`, `src/lib/platform/OSXScreen.mm`, `src/lib/platform/MSWindowsScreen.cpp`
- Risk: Medium -- requires mocking or hardware; platform-specific bugs escape automated testing.
- Priority: Medium

**EiScreen and PortalInputCapture untested:**
- What's not tested: The Wayland/EI input capture path has zero unit tests.
- Files: `src/lib/platform/EiScreen.cpp` (957 lines), `src/lib/platform/PortalInputCapture.cpp`, `src/lib/platform/EiKeyState.cpp`
- Risk: High -- this is the primary path for modern Linux desktop support, with many FIXME items.
- Priority: High

**Client-side protocol handling:**
- What's not tested: `ServerProxy.cpp` (852 lines) message parsing and response handling.
- Files: `src/lib/client/ServerProxy.cpp`
- Risk: Medium -- protocol parsing bugs can cause desync between client and server.
- Priority: Medium

---

*Concerns audit: 2026-05-12*
