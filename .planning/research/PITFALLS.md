# Pitfalls Research

**Domain:** KVM/keyboard-mouse sharing software (C++ cross-platform, mature codebase)
**Researched:** 2026-05-12
**Confidence:** HIGH (codebase-verified where noted, MEDIUM for web-research-only findings)

## Critical Pitfalls

### Pitfall 1: Single-Threaded SocketMultiplexer Blocks All Connections During Any Slow Operation

**What goes wrong:**
The `SocketMultiplexer::serviceThread` is the single event loop for ALL socket I/O. It calls `ARCH->pollSocket()` with infinite timeout (`-1`), then processes each ready socket sequentially by calling `job->run()`. If any single `run()` call blocks (e.g., TLS handshake in `SecureSocket`, a slow write to a lagging client, or a read that enters a retry loop), every other connected client's input events are delayed. This is the root cause of the reported network stutter: one slow operation stalls mouse movement for all clients.

In the codebase, `SecureSocket::doRead()` at line 120 uses `static uint8_t buffer[4096]` -- a shared static buffer across all instances. More critically, `SecureSocket::doWrite()` uses `static` retry/buffer variables (lines 177-180) that are shared across ALL `SecureSocket` instances. If two connections call `doWrite()` concurrently through the multiplexer, these statics corrupt each other.

**Why it happens:**
The architecture was designed for a small number of connections (1-2 clients) where blocking during a TLS handshake was tolerable. The code even acknowledges this: line 427 of `SecureSocket.cpp` says "Historically a 1s sleep let any failed handshake DoS all clients." As the codebase aged, more operations were added to the service loop without considering the head-of-line blocking problem.

**How to avoid:**
When refactoring the network layer, the poll loop must NEVER call any potentially blocking operation. Specifically:
1. Replace the infinite timeout `pollSocket(... -1)` with a bounded timeout (e.g., 10ms) so the loop can detect stale connections.
2. After removing TLS (the biggest blocking culprit), ensure the remaining `ARCH->readSocket` and `ARCH->writeSocket` calls use non-blocking sockets with proper `EAGAIN`/`EWOULDBLOCK` handling.
3. Eliminate ALL `static` local variables in `SecureSocket` (and its removal path) -- these are latent data races even in single-threaded contexts because the multiplexer services multiple socket objects in the same loop iteration.

**Warning signs:**
- Mouse cursor stutters or freezes intermittently, then catches up in a burst.
- Stutter correlates with network jitter or connection changes.
- Adding `LOG_DEBUG` around socket operations makes the problem change (timing-dependent).
- Any `static` variable inside a method that processes socket data.

**Phase to address:**
Phase 1 (Network Fix). This is the highest priority pitfall because it directly impacts the core value proposition: "fluent LAN keyboard/mouse sharing." The TLS removal and network layer simplification are tightly coupled here.

---

### Pitfall 2: Removing TLS Leaves Stale Security-Level Dispatch Code That Creates Dead Paths

**What goes wrong:**
The TLS/encryption layer is deeply woven through the architecture, not just a wrapper. Analysis of the codebase shows `SecurityLevel` (an enum with `PlainText`, `PeerAuth`, `Encrypted`) flows through:
- `Client.cpp:85` -- client constructor decides security level
- `ServerApp.cpp:443-453` -- server app creates listeners with security level
- `ClientListener.h:92` -- stores `m_securityLevel` as member
- `TCPSocketFactory.cpp:29` -- creates `SecureSocket` based on level
- `MainWindow.cpp:841-862` -- GUI manages fingerprint databases
- `CoreProcess.cpp:605` -- monitors secure socket status

If TLS files are deleted but the `SecurityLevel` enum and dispatch code remain, the code will have dead branches everywhere: `if (m_securityLevel == SecurityLevel::PeerAuth)` checks that can never be true, and factory methods that create `SecureSocket` objects that no longer exist. These dead paths are not just messy -- they can hide bugs. For example, `ClientListener.cpp:151` has `if (m_securityLevel == SecurityLevel::PlainText)` which takes the plain path, but the else branch would create a `SecureListenSocket` that no longer compiles.

**Why it happens:**
TLS was added as a cross-cutting concern, not behind a clean interface. The `SecurityLevel` enum leaks into client, server, GUI, and factory code. Developers naturally think "delete the TLS files" without tracing every code path that references security concepts.

**How to avoid:**
1. Start by finding every file that includes or references `SecureSocket`, `SecureUtils`, `SslLogger`, `FingerprintDatabase`, `SecurityLevel`, `SecureListenSocket`. The list above is a starting point but must be verified at refactor time.
2. Replace `SecurityLevel` with a simpler boolean or remove it entirely. Since this project is LAN-only, hardcode plain-text mode.
3. In `TCPSocketFactory`, remove the security level parameter entirely and always create `TCPSocket`.
4. In the GUI (`MainWindow`), remove fingerprint management UI and the `trustedFingerprintDatabase()` method.
5. Remove OpenSSL from `CMakeLists.txt` dependency list.
6. After deletion, search the codebase for any remaining `#include` of deleted headers.

**Warning signs:**
- Compile errors referencing `SecureSocket` or `SecurityLevel` after TLS file deletion.
- Linker errors for unresolved `SecureSocket` symbols.
- GUI shows TLS-related options that crash when clicked.
- Settings keys related to TLS that have no handler.

**Phase to address:**
Phase 2 (TLS Removal). Should be a dedicated phase, not mixed with the network fix. Removing TLS first simplifies the network fix by eliminating the handshake-blocking path, but doing them simultaneously risks conflating two different changesets.

---

### Pitfall 3: Removing Linux Platform Code Breaks the Abstract Interface Contract

**What goes wrong:**
The platform layer uses an abstract interface pattern: `IPlatformScreen` (in `src/lib/deskflow/IPlatformScreen.h`) is implemented by `OSXScreen`, `MSWindowsScreen`, and several Linux implementations (`XWindowsScreen`, `EiScreen`). The CMake build in `src/lib/platform/CMakeLists.txt` uses `if(UNIX AND NOT APPLE)` to conditionally include Linux files.

There are 44 Linux-specific files (X11, Wayland/EI, Portal, XDG) to remove. The pitfall is not just deleting files -- it is that the abstract interface (`IPlatformScreen`, `IPrimaryScreen`, `ISecondaryScreen`, `IKeyState`) was designed with ALL platforms in mind. Some interface methods exist only because Linux needed them, and some methods have `// FIXME not implemented` in macOS/Windows because they were added for Linux first.

If Linux code is removed without simplifying the interface, you get dead virtual methods that no implementation will ever use differently. More dangerously, shared code in `src/lib/deskflow/` may contain `#ifdef WINAPI_LIBEI` or similar guards (as seen in the CMake at line 189: `target_compile_definitions(platform PUBLIC WINAPI_LIBEI WINAPI_LIBPORTAL)`) that create different code paths per platform. Removing the Linux side of these `#ifdef` blocks incorrectly can silently change behavior on macOS/Windows.

**Why it happens:**
Cross-platform C++ codebases accumulate platform-specific code at three levels:
1. Implementation files (easy to delete -- just remove the file).
2. CMake build rules (medium -- need to simplify conditionals).
3. Shared interface code with platform branches (hard -- scattered across business logic).

Developers typically handle level 1 and 2, miss level 3, and end up with a codebase that compiles but has mysterious behavioral differences.

**How to avoid:**
1. Delete all files matching patterns: `X*`, `Ei*`, `Portal*`, `Wl*`, `XDG*` in `src/lib/platform/`.
2. Simplify `CMakeLists.txt` to only have `if(WIN32)` and `elseif(APPLE)` branches (remove the `elseif(UNIX)` block entirely).
3. Search all non-platform code for `WINAPI_LIBEI`, `WINAPI_LIBPORTAL`, `HAVE_X11`, `Q_OS_LINUX`, `BUILD_X11_SUPPORT`, and remove or simplify those conditional blocks.
4. Remove Linux-specific dependencies from CMake: `libei`, `libportal`, `xkbcommon`, `X11`, `DBus`.
5. Review `IPlatformScreen` and remove any methods that only Linux implementations override differently. But DO NOT remove methods that macOS/Windows implement -- keep those.
6. Search for `#include` of any deleted header file across the entire codebase.

**Warning signs:**
- CMake configure fails because it still tries to find `libei` or `libportal`.
- Compile errors about undefined `WINAPI_LIBEI` or similar macros.
- Linker errors about missing X11 or Wayland symbols.
- Settings or configuration options that reference Linux-specific features.

**Phase to address:**
Phase 3 (Platform Removal). After TLS removal and network fix, so the codebase is already cleaner. The platform removal is a large deletion that should be done in one coherent pass, not spread across phases.

---

### Pitfall 4: Stuck Keys When Network Connection Drops During Key-Down State

**What goes wrong:**
In a KVM software keyboard sharing system, when a key is pressed on the server and forwarded to a client, the sequence is: keyDown sent over network -> client fakes keyDown. When the key is released: keyUp sent over network -> client fakes keyUp. If the network connection drops (or stalls) BETWEEN the keyDown and keyUp messages, the client's OS believes the key is still held down. The key will repeat infinitely until either:
- The connection recovers and a late keyUp arrives.
- The user presses and releases the same key again on the client machine directly.
- The user restarts the software.

This is a well-known failure mode in all software KVM tools. The codebase shows evidence of awareness but no robust solution: `IKeyState.h:97-99` documents `fakeKeyRelease` for "every key that is synthetically pressed" but there is no evidence of a "release all pressed keys on disconnect" mechanism in the server or client code.

**Why it happens:**
The key state tracking is split across the server (tracks which keys are physically pressed) and the client (tracks which keys are virtually pressed via fake input). When the connection breaks, the server-side key state resets (no physical keys are stuck), but the client-side fake key state is never explicitly released. The disconnect handler focuses on closing sockets and cleaning up buffers, not on releasing held keys.

**How to avoid:**
1. Add a `releaseAllKeys()` call to the client's disconnect handler. When the connection drops, iterate all currently fake-pressed keys and send a `fakeKeyUp` for each.
2. Add a `keyState` tracking set on the client that records every `fakeKeyDown` and removes on `fakeKeyUp`. On disconnect, iterate this set and release all.
3. On the server side, when a client disconnects, send a "reset" message before closing (if possible) telling the client to release all keys.
4. Add a watchdog timer: if no keyUp arrives within N seconds of a keyDown for a non-modifier key, auto-release it.
5. Test this explicitly: hold a key, unplug the network cable, verify the key stops repeating within 1 second.

**Warning signs:**
- Keys get "stuck" repeating after switching screens or after network hiccups.
- Users report needing to press the stuck key on the client machine directly to stop it.
- Modifier keys (Shift, Ctrl, Alt) get stuck, causing unexpected behavior on the client.

**Phase to address:**
Phase 1 (Network Fix). The stuck-key issue is directly related to network reliability. As the network layer is refactored, the disconnect-handling path must be updated simultaneously. If separated, the network fix phase will not have a clean disconnect handler to hook into.

---

### Pitfall 5: Smart Pointer Migration Creates Ownership Ambiguity in SocketMultiplexer's Custom Memory Management

**What goes wrong:**
`SocketMultiplexer` uses a hand-rolled memory management scheme: socket jobs are allocated with `new` in various places, stored as raw pointers in a `std::list<ISocketMultiplexerJob*>`, and deleted in `SocketMultiplexer::removeSocket()` or `serviceThread()`. The ownership model is: "whoever removes the job from the list owns deleting it." Additionally, the cursor mark pattern at line 36 uses `reinterpret_cast<ISocketMultiplexerJob*>(this)` to create a sentinel value in the linked list -- this pointer is never dereferenced but is compared against other pointers.

A naive smart pointer migration (replacing `ISocketMultiplexerJob*` with `std::unique_ptr<ISocketMultiplexerJob>` in the list) would break immediately because:
1. The cursor sentinel (`m_cursorMark`) is not a real `ISocketMultiplexerJob` -- wrapping it in a smart pointer would call `delete` on `this` (the `SocketMultiplexer` object itself).
2. `nextCursor()` and `deleteCursor()` move iterators through the list, and `std::list<std::unique_ptr>` iterators behave differently when splicing.
3. The job objects are returned from `job->run()` and may replace themselves in the list (line 200: `if (newJob != job) { delete job; *jobCursor = newJob; }`). With `unique_ptr`, this transfer must be done via `std::move` or `reset()`.

**Why it happens:**
The codebase has a comment acknowledging the problem: `// TODO: Remove this evilness` at line 35. The cursor mark pattern was a clever optimization for 2004-era C++ but is fundamentally incompatible with smart pointers. Developers see "replace raw pointers with smart pointers" as a straightforward find-and-replace without understanding the ownership semantics.

**How to avoid:**
1. Replace the `reinterpret_cast` cursor mark with a proper sentinel. Options: use `std::optional<JobCursor>`, use an index-based approach instead of pointer-based iteration, or use a `std::variant<ISocketMultiplexerJob*, Sentinel>` tagged union.
2. THEN migrate the `std::list<ISocketMultiplexerJob*>` to `std::list<std::unique_ptr<ISocketMultiplexerJob>>`.
3. Update `nextCursor()` to use `std::move` semantics when splicing.
4. Update `serviceThread()` job replacement (line 200-204) to use `unique_ptr::reset()`.
5. Do NOT mix `unique_ptr` and raw `new`/`delete` for the same objects during migration. Complete the migration for one class at a time.

**Warning signs:**
- `unique_ptr` delete on wrong pointer causing double-free or crash on `this`.
- Use-after-free when cursor iteration encounters a moved-from `unique_ptr`.
- Memory leaks where `unique_ptr` was supposed to take ownership but old raw `delete` was removed prematurely.
- Tests pass in debug but crash in release (due to iterator debug checks).

**Phase to address:**
Phase 4 (Modernization). This should NOT be attempted until the network layer and TLS removal are complete. The SocketMultiplexer is the most dangerous code to modify. Get the architectural changes right first, then modernize memory management on a stable foundation.

---

### Pitfall 6: `exit(0)` Bypasses RAII Cleanup, Causing Resource Leaks and Stuck State

**What goes wrong:**
`InputFilter::RestartServer::perform()` at line 265 calls `exit(0)` directly. This terminates the process without calling destructors for stack-allocated objects, without flushing buffers, and without closing sockets cleanly. In a KVM application, this means:
- TCP connections are dropped without FIN packets (the OS eventually cleans up, but the other side sees an abrupt disconnect).
- On the client side, keys that were fake-pressed remain stuck (see Pitfall 4).
- RAII-managed resources (file handles, mutexes, threads) are leaked.
- The OS-specific platform screen cleanup (releasing input hooks, restoring cursor visibility) is skipped.

**Why it happens:**
The comment says `// HACK Super hack we should gracefully exit`. This was a quick workaround that was never revisited. The `exit()` call was probably added because properly shutting down all subsystems in the right order was complicated, and the developer chose the simple path.

**How to avoid:**
1. Replace `exit(0)` with posting a quit event to the event queue: `m_events->addEvent(Event(EventTypes::Quit, getEventTarget()))`.
2. Ensure the quit event triggers a clean shutdown sequence: stop accepting new connections, release all clients, close sockets with proper FIN, release platform hooks, then exit the main loop.
3. Search the entire codebase for other `exit()` calls and replace them similarly.

**Warning signs:**
- Client reports "connection reset by peer" instead of "server disconnected."
- After server restart, client does not reconnect automatically (stuck in bad state).
- Keys stuck on client after server restart.

**Phase to address:**
Phase 1 (Network Fix). Fix this early because it affects reliability of testing -- every test that triggers a restart currently leaks resources.

---

### Pitfall 7: Static Local Variables in SecureSocket Create Data Races and Persist Stale State

**What goes wrong:**
`SecureSocket.cpp` contains multiple `static` local variables inside member functions:
- `doRead()` line 120-121: `static uint8_t buffer[4096]` -- shared read buffer
- `doWrite()` lines 177-180: `static bool s_retry`, `static int s_retrySize`, `static int s_staticBufferSize`, `static void *s_staticBuffer` -- shared write state
- `secureRead()` line 235: `static int retry`
- `secureWrite()` line 263: `static int retry`
- `secureAccept()` line 421: `static int retry`
- `secureConnect()` line 482: `static int retry`

Even though TLS is being removed, these statics reveal a deeper design problem. If similar patterns exist in `TCPSocket` or are introduced during refactoring, they will cause:
1. Data corruption: Two `SecureSocket` instances calling `doRead()` share the same `buffer[4096]`. Instance A reads into buffer, instance B reads into buffer before A processes it -- A's data is lost.
2. Stale state: The `retry` counter in `secureAccept` is `static`, so if one accept fails, the next accept call on a DIFFERENT connection starts with the previous connection's retry count.
3. Memory leak: `s_staticBuffer` is allocated via `realloc` but never freed.

**Why it happens:**
These statics were likely introduced as "optimizations" to avoid stack allocation or to persist retry state across calls. In a single-connection scenario, they work. With multiple connections through the multiplexer, they are fundamentally broken.

**How to avoid:**
1. When removing TLS/SecureSocket, delete all of these entirely -- do not port the pattern.
2. Audit `TCPSocket.cpp` for similar static locals. (Checked: `TCPSocket` does NOT have this problem -- it uses local stack buffers.)
3. Add a code review rule: NO static local variables in any method that processes per-connection data. Use member variables instead.
4. Consider adding `-Werror=static-in-inline` or a custom clang-tidy check.

**Warning signs:**
- Intermittent data corruption in network communication.
- Retry logic behaves differently depending on previous connections.
- Memory usage grows over time (from `realloc` without `free`).

**Phase to address:**
Phase 2 (TLS Removal). These statics are all in SecureSocket, which will be deleted. The pitfall is more about not replicating this pattern in the new code. Add a coding guideline during this phase.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| `static` local buffers in socket methods | Avoid stack allocation, reuse memory | Data corruption with multiple connections | Never -- use member variables |
| `exit(0)` for "clean" shutdown | Simple restart implementation | RAII bypass, stuck keys, leaked sockets | Never -- post quit event |
| `reinterpret_cast<>` for sentinel values | Clever linked-list cursor | Undefined behavior, incompatible with smart pointers | Never -- use proper sentinel type |
| Raw `new`/`delete` in socket job management | Simple ownership (delete on remove) | Use-after-free on exception, incompatible with modern C++ | Only until smart pointer migration phase |
| Custom two-phase locking (`lockJobListLock` then `lockJobList`) | Fine-grained control | Extremely fragile, no test coverage, deadlock risk | Never -- replace with `std::mutex` + `std::lock_guard` |
| Suppressing compiler warnings globally (`_CRT_SECURE_NO_WARNINGS`) | Builds cleanly on MSVC | Hides buffer overflow vulnerabilities | Never -- fix the warnings |
| Suppressing deprecation pragmas on macOS | Builds cleanly | Sudden breakage when Apple removes deprecated API | Only temporarily, with tracking issue |
| Inlining platform conditionals in business logic | Avoids virtual dispatch overhead | Dead code paths, hard to remove platforms later | Never -- use virtual interface pattern |

## Integration Gotchas

Common mistakes when connecting to external services.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| macOS CGEvent API | Using deprecated Carbon calls wrapped in `#pragma clang diagnostic ignored "-Wdeprecated-declarations"` | Audit each usage and migrate to modern CGEvent replacements. Track which macOS version will break each call. |
| Windows input hooks | Setting hooks without considering UAC/elevation boundaries | Test with both elevated and non-elevated processes. Document elevation requirements. |
| OpenSSL (during removal) | Deleting OpenSSL files but leaving `find_package(OpenSSL)` in CMake | Remove `find_package`, remove from `target_link_libraries`, remove from CI dependency lists, remove from vcpapt/conan config. |
| Qt 6 event system | Posting events to freed objects (event target is a raw pointer) | Use `QObject` with parent-child ownership or `std::shared_ptr` for event targets. Verify event targets outlive their events. |
| Network socket options | Disabling Nagle algorithm (`setNoDelayOnSocket(true)`) without considering packet storms for mouse move events | Keep Nagle off for keyboard events, consider batching mouse move events into single writes. The existing code already disables Nagle, which is correct for low-latency input. |

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Single-threaded poll loop with blocking operations | Mouse stutters when another client connects or TLS handshakes | Keep all operations in the poll loop non-blocking. Use timeouts on poll. | 2+ clients, any TLS handshake, any slow network write |
| Static retry counters shared across connections | Retry logic behaves erratically for concurrent connections | Use per-connection member variables for all state | 2+ concurrent connections |
| Fixed-size protocol buffers (`kMaxHelloLength = 1024`) | Connection fails if configuration exceeds buffer | Use variable-length framing with size prefix | Complex screen configurations, many screens in grid |
| Busy-polling with `Arch::sleep(0.1)` for watchdog | 100ms latency in detecting process state changes, wasted CPU | Use event-driven waits (`WaitForSingleObject` on Windows) | Any production use of watchdog |
| `realloc` without `free` for write buffer in SecureSocket | Gradual memory growth over connection lifetime | Use `std::vector<uint8_t>` for dynamic buffers, RAII-managed | Long-running sessions with many writes |

## Security Mistakes

Domain-specific security issues beyond general web security.

| Mistake | Risk | Prevention |
|---------|------|------------|
| Removing TLS without documenting the LAN-only trust model | Users expose keyboard input (passwords!) over network if used outside LAN | Add startup warning if non-loopback address detected. Document trust model prominently. Consider binding to specific interfaces only. |
| Keeping fingerprint database code after TLS removal | Dead code that may confuse users into thinking there IS authentication | Remove all fingerprint UI and code. Remove fingerprint database files from config directory. |
| No input validation on protocol messages from network | Malicious client could send crafted packets causing buffer overflows or crashes | Validate all protocol message sizes before processing. Add maximum message size limits. Never trust network input. |
| `_CRT_SECURE_NO_WARNINGS` hides buffer overflows on Windows | sprintf/strcpy overflows in platform code | Remove global suppression, fix individual warnings, use `std::string`/`std::format` instead |
| Writing fingerprint database without restrictive permissions (even during removal) | Other users on shared machine could tamper with trust database | Remove the code entirely rather than fix it, since TLS is going away |

## UX Pitfalls

Common user experience mistakes in this domain.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Keys get stuck after network interruption | User must physically go to the client machine to press the stuck key, breaking the entire workflow | Auto-release all pressed keys on disconnect. Show visual indicator of stuck keys. |
| No visual feedback when connection is degraded | User thinks mouse is frozen, tries to fix hardware | Show connection quality indicator (latency/packet loss). Degrade gracefully with visual feedback. |
| Mouse movement stutters under network jitter | Makes the software unusable for precise tasks (design, gaming) | Buffer and interpolate mouse positions. Prioritize mouse events over other traffic. Consider UDP for mouse data. |
| Server restart drops all clients without reconnection | User must manually restart all clients | Implement automatic reconnection with exponential backoff. Client should reconnect when server comes back. |
| Removing TLS with no user communication | Users who relied on TLS for multi-subnet setups suddenly have no encryption | Document the change prominently. Consider keeping TLS as optional (but off by default) if the code is already there. |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Platform removal:** Often missing `#ifdef` branches in shared (non-platform) code -- search for `WINAPI_LIBEI`, `HAVE_X11`, `BUILD_X11_SUPPORT`, `Q_OS_LINUX` across all `.cpp` and `.h` files
- [ ] **TLS removal:** Often missing GUI fingerprint management code and settings keys -- verify `MainWindow` no longer references `FingerprintDatabase`, verify `Settings::Security::Certificate` keys are removed
- [ ] **TLS removal:** Often missing CMake OpenSSL dependency -- verify `find_package(OpenSSL)` and `OpenSSL::SSL OpenSSL::Crypto` are removed from all CMakeLists.txt files
- [ ] **TLS removal:** Often missing `deskflow-daemon` references in IPC -- verify `CoreIpc.h` functions related to TLS are cleaned up
- [ ] **Smart pointer migration:** Often missing the `reinterpret_cast` sentinel pattern -- verify `SocketMultiplexer::m_cursorMark` is replaced before converting `std::list` to use smart pointers
- [ ] **Network fix:** Often missing the stuck-key-on-disconnect handler -- verify that `fakeKeyUp` is called for all held keys when connection drops
- [ ] **Network fix:** Often missing the `exit(0)` replacement -- verify all `exit()` calls in `src/lib/` are replaced with event-based shutdown
- [ ] **Build verification:** Often missing the CI pipeline update after removing Linux/TLS dependencies -- verify CI no longer attempts to install `libei-dev`, `libportal-dev`, `libssl-dev` on Linux

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Stuck keys after disconnect | LOW | Add `releaseAllKeys()` to disconnect handler. Test by unplugging network cable during key hold. |
| Dead code from incomplete TLS removal | LOW | Grep for `SecureSocket`, `SecurityLevel`, `FingerprintDatabase`, `SslLogger` in all source. Each reference is either deleted or replaced. |
| Dead code from incomplete Linux removal | MEDIUM | Delete the 44 Linux files, simplify CMake, then fix compile errors. Then grep for `X11`, `Wayland`, `EI`, `Portal` in remaining code. |
| Smart pointer migration breaks SocketMultiplexer | HIGH | Revert to raw pointers. First replace the `reinterpret_cast` sentinel with a proper approach, then retry smart pointer migration. |
| Network stutter persists after TLS removal | HIGH | Profile with Instruments (macOS) or xperf (Windows) to identify remaining blocking calls. Check for any remaining `sleep()`, `wait()` without timeout, or synchronous operations in the poll loop. |
| `exit(0)` replacement causes shutdown hang | MEDIUM | The event-based shutdown may deadlock if subsystems shut down in wrong order. Add shutdown timeout (e.g., 3 seconds) then force exit. |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Single-threaded poll blocks all connections | Phase 1: Network Fix | Test with 2+ clients, simulate network jitter with `tc netem`, verify no mouse stutter |
| Stuck keys on disconnect | Phase 1: Network Fix | Hold key, kill server process, verify client releases key within 1 second |
| `exit(0)` bypasses RAII | Phase 1: Network Fix | Trigger server restart via hotkey, verify clean shutdown in logs |
| Static locals in SecureSocket | Phase 2: TLS Removal | Deleted with SecureSocket. Audit TCPSocket for similar patterns. |
| TLS removal leaves stale dispatch code | Phase 2: TLS Removal | Grep for `SecureSocket`, `SecurityLevel`, `FingerprintDatabase` -- zero hits in remaining code |
| Platform removal breaks interface | Phase 3: Platform Removal | Build on macOS and Windows. Verify no Linux/X11/Wayland references in remaining code. |
| Smart pointer migration breaks multiplexer | Phase 4: Modernization | Run unit tests (add tests for SocketMultiplexer FIRST). Run ASan/UBSan. Test with valgrind on Linux... wait, no Linux. Test with Instruments/ASan on macOS. |

## Sources

- Deskflow codebase analysis (SocketMultiplexer.cpp, SecureSocket.cpp, TCPSocket.cpp, platform/CMakeLists.txt)
- CONCERNS.md audit (2026-05-12)
- [Refactoring of Preprocessor Directives in the #ifdef Hell -- CMU/TSE 2017](https://www.cs.cmu.edu/~./ckaestne/pdf/tse17_refactoringifdef.pdf)
- [Beyond new and delete: A Practical Guide to Refactoring Raw Pointers to Smart Pointers -- dev.to](https://dev.to/legacycpp/beyond-new-and-delete-a-practical-guide-to-refactoring-raw-pointers-to-smart-pointers-3pcd)
- [Preventing Memory Leaks in Large C++ Codebases Using Smart Pointers -- Medium](https://medium.com/@koradeganesh/preventing-memory-leaks-in-large-c-codebases-using-smart-pointers-b25218f4ee41)
- [Smart Pointers as a Production-Grade Stability Layer -- LinkedIn](https://www.linkedin.com/pulse/smart-pointers-production-grade-stability-layer-real-story-alheraki-ldpuf)
- [Deskflow Issue #8695: Mouse lags and input skips](https://github.com/deskflow/deskflow/issues/8695)
- [Deskflow Issue #8379: Mouse moving slowly on client](https://github.com/deskflow/deskflow/issues/8379)
- [Barrier Issue #2031: Decline and stagnation discussion](https://github.com/debauchee/barrier/issues/2031)
- [KVM switch stuck/repeating keys -- Gentoo Forums](https://forums.gentoo.org/viewtopic.php?t=1168817)
- [KVM random repeated keystrokes -- GitHub jetkvm/kvm #325](https://github.com/jetkvm/kvm/issues/325)

---
*Pitfalls research for: KVM/keyboard-mouse sharing software optimization (Deskflow fork)*
*Researched: 2026-05-12*
