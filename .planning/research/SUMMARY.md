# Project Research Summary

**Project:** Deskflow (personal streamlined fork)
**Domain:** LAN-only KVM/keyboard-mouse sharing (C++20, cross-platform macOS + Windows)
**Researched:** 2026-05-12
**Confidence:** HIGH

## Executive Summary

Deskflow is a mature C++ keyboard-mouse sharing tool (Synergy/Barrier lineage) whose core value is fluent, low-latency input sharing over LAN. The root cause of its stuttering under network jitter is well-understood: the monolithic `SocketMultiplexer` runs a single-threaded `poll()` loop with infinite timeout (`-1`) while holding the job list lock, meaning any network fluctuation blocks the entire input pipeline. This is not a protocol tuning problem -- TCP_NODELAY is already set -- it is a fundamental architectural flaw in how I/O, event processing, and socket management contend for a single lock.

The recommended approach is a **pipelined architecture** with lock-free SPSC ring buffers decoupling input capture from network I/O. The single most impactful change is replacing the `SocketMultiplexer` with dedicated send/receive threads using non-blocking I/O with bounded poll timeouts (1ms), combined with mouse move coalescing (only the latest position matters). This should reduce end-to-end mouse latency from ~45ms to under 8ms and eliminate the stutter catch-up pattern entirely. The recommended async I/O library is standalone Asio (header-only, no Boost dependency), though the pipelined design can also be implemented with direct non-blocking OS APIs if the Asio dependency is undesirable.

Key risks are: (1) the TLS/encryption layer is deeply woven through client, server, GUI, and factory code -- removing it requires tracing every `SecurityLevel` dispatch path, not just deleting files; (2) the custom cursor-mark linked list with `reinterpret_cast` sentinels in `SocketMultiplexer` makes naive smart pointer migration dangerous; (3) the `exit(0)` call in `InputFilter::RestartServer::perform()` bypasses RAII cleanup, causing stuck keys on the client side. All three must be addressed in the correct order to avoid compounding problems.

## Key Findings

### Recommended Stack

The stack is largely already in place (C++20, Qt 6, CMake). The single new dependency is **standalone Asio** for async/non-blocking I/O, replacing the hand-rolled `SocketMultiplexer`. All other changes are adoption of C++20 standard library features: `std::jthread` (auto-join threads with cooperative cancellation), `std::unique_ptr` (replace raw new/delete), `std::format` (replace printf-style logging), `std::span` (replace raw pointer+size buffer params), `std::atomic` (replace mutex-protected bool flags).

**Core technologies:**
- **Standalone Asio 1.38.0** (async I/O) -- header-only, cross-platform (kqueue on macOS, IOCP on Windows), direct replacement for `SocketMultiplexer` + `poll()`
- **C++20 stdlib features** (`jthread`, `unique_ptr`, `format`, `span`, `atomic`) -- modernize the ~2004-era memory management and threading patterns
- **Qt 6** (GUI) -- already in use, no change needed
- **CMake 3.24+** (build) -- already in use, add FetchContent for Asio

### Expected Features

The core product (mouse/keyboard forwarding, clipboard text, screen edge config, Scroll Lock toggle, auto-reconnect) is already implemented and working. The critical gap is **smooth cursor transition under network jitter** -- partially working but broken by the blocking I/O architecture.

**Must have (table stakes):**
- Mouse cursor movement across screen edges -- the product IS this feature
- Keyboard input forwarding -- second fundamental operation
- Mouse button and scroll forwarding -- immediately noticed if broken
- Clipboard text sharing -- every competitor has this
- Smooth cursor transition under network jitter -- CORE VALUE, currently broken
- Auto-reconnect after network interruption -- LAN has transient disconnects
- Modifier key sync (Caps Lock, Num Lock) -- wrong state is extremely frustrating

**Should have (competitive):**
- Clipboard image sharing -- already implemented, needs testing with new network layer
- Relative mouse movement -- important for gaming
- Latency metrics display -- helps users diagnose network issues
- Lock all screens simultaneously -- trivial to add via existing protocol

**Defer (v2+):**
- File drag-and-drop -- protocol exists but feature deprecated, high complexity to reimpl
- Multi-monitor layout profiles -- low priority for personal use
- Keyboard layout sync -- already implemented, low risk, can validate later

**Explicitly remove:**
- TLS/SSL encryption layer (~3000+ lines)
- Linux platform support (44 files, ~5000+ lines)
- Windows daemon service mode
- Commercial licensing code

### Architecture Approach

The recommended architecture replaces the monolithic `SocketMultiplexer` with a **pipelined design**: Input Capture Thread -> Event Queue -> Server Logic -> Send-Side Event Buffer (lock-free SPSC) -> Network Send Thread on the server, and Network Receive Thread -> Receive-Side Event Buffer (lock-free SPSC) -> Client Event Processor -> Input Injection on the client. The critical rule: the network thread never calls into the event queue or server logic directly -- all communication goes through lock-free buffer components.

**Major components:**
1. **SendEventBuffer / RecvEventBuffer** (NEW) -- lock-free SPSC ring buffers; mouse moves use atomic latest-position slot (coalesced), keyboard events use FIFO queue (no drops)
2. **NetworkSendThread / NetworkReceiveThread** (NEW) -- dedicated non-blocking I/O threads with 1ms bounded poll timeout, replace `SocketMultiplexer` for data path
3. **ConnectionManager** (NEW) -- handles socket accept, handshake, keep-alive; separate from data path
4. **InputCapture / InputInjection** (existing platform layer) -- minimal changes, continues to use CGEvent (macOS) and SendInput (Windows)

### Critical Pitfalls

1. **Single-threaded SocketMultiplexer blocks all connections** -- The `poll()` with infinite timeout and lock held is the root cause of stutter. Fix: pipelined architecture with non-blocking I/O. (Phase 1)
2. **TLS removal leaves stale SecurityLevel dispatch code** -- `SecurityLevel` enum leaks into client, server, GUI, and factory code. Naive file deletion creates dead paths and linker errors. Fix: trace every reference, hardcode plain-text mode, remove enum entirely. (Phase 2)
3. **Removing Linux platform code breaks abstract interface contract** -- `IPlatformScreen` has methods that only Linux overrides differently, plus `#ifdef WINAPI_LIBEI` guards in shared code. Fix: delete 44 files, simplify CMake, grep all `#ifdef` branches. (Phase 3)
4. **Stuck keys when network drops during key-down state** -- Client never releases fake-pressed keys on disconnect. Fix: add `releaseAllKeys()` to disconnect handler with key-state tracking set. (Phase 1)
5. **Naive smart pointer migration breaks SocketMultiplexer cursor marks** -- The `reinterpret_cast<ISocketMultiplexerJob*>(this)` sentinel pattern makes `unique_ptr` wrap the multiplexer object itself, causing double-free. Fix: replace sentinel with proper type before migrating to smart pointers. (Phase 4)

## Implications for Roadmap

Based on combined research, the following phase structure addresses dependencies correctly and avoids pitfalls:

### Phase 1: Network Pipeline Rewrite
**Rationale:** This is the core value proposition ("fluent LAN keyboard/mouse sharing"). All other work is secondary. The blocking `SocketMultiplexer` is the diagnosed root cause of stutter. Must be done first because TLS removal and platform cleanup depend on a stable network layer.
**Delivers:** Sub-8ms end-to-end mouse latency, no stutter under network jitter, clean disconnect handling
**Addresses:** Smooth cursor transition, auto-reconnect, stuck keys on disconnect, `exit(0)` RAII bypass
**Avoids:** Pitfall 1 (blocking poll), Pitfall 4 (stuck keys), Pitfall 6 (exit bypass)
**Uses:** Standalone Asio (or direct non-blocking OS APIs), `std::jthread`, lock-free SPSC queues

### Phase 2: TLS/Encryption Removal
**Rationale:** TLS is the next largest source of complexity and bugs (static local variables causing data races, handshake blocking the service thread). Can be done after the network layer is stable because the new pipeline does not need TLS code at all. Must be a dedicated phase because `SecurityLevel` dispatch code is scattered across 6+ modules.
**Delivers:** ~3000+ lines removed, no OpenSSL dependency, no certificate management, no static buffer data races
**Addresses:** Anti-feature removal (TLS), Pitfall 2 (stale dispatch code), Pitfall 7 (static locals)
**Avoids:** Incomplete removal leaving dead code paths that hide bugs

### Phase 3: Linux Platform Removal
**Rationale:** Large deletion (44 files, ~5000+ lines) that should be done as one coherent pass after the network and TLS changes are validated. Doing this first would complicate testing during network rewrite (need working builds on at least one platform). After Phase 2, the codebase is cleaner and the platform boundaries are clearer.
**Delivers:** macOS + Windows only, simplified CMake, no libei/libportal/xkbcommon dependencies
**Addresses:** Anti-feature removal (Linux support), Pitfall 3 (broken interface contract)
**Avoids:** Incomplete `#ifdef` removal in shared code causing silent behavioral changes

### Phase 4: C++ Modernization
**Rationale:** Smart pointer migration, `std::format` adoption, removing the cursor-mark sentinel pattern -- these are important but must NOT be attempted while the SocketMultiplexer architecture is still in flux. The cursor-mark pitfall (Pitfall 5) specifically requires a stable foundation before touch. Do this last when the architecture is proven.
**Delivers:** No raw new/delete, no reinterpret_cast sentinels, type-safe formatting, auto-join threads
**Addresses:** Pitfall 5 (smart pointer migration breaks multiplexer), memory safety, technical debt
**Uses:** `std::unique_ptr`, `std::format`, `std::span`, Clang-Tidy modernization checks, ASan/TSan

### Phase Ordering Rationale

- **Network first** because every other change is lower priority than the core value (fluent input sharing)
- **TLS before Linux removal** because TLS code touches the network path and its removal simplifies testing; Linux removal is a larger but more mechanical deletion
- **Modernization last** because smart pointer migration of SocketMultiplexer is dangerous while the architecture is changing; get the design right first, then clean up memory management
- The pipeline architecture and TLS removal are **independent** (can be parallelized if team size allows), but Phase 3 (Linux removal) and Phase 4 (modernization) must be sequential

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 1:** The exact integration of Asio with the existing `IArchNetwork` abstraction needs API-level research. Decision needed: full Asio adoption vs. direct non-blocking OS API wrappers with bounded poll. The lock-free SPSC queue implementation also needs validation on both macOS and Windows (memory ordering guarantees).
- **Phase 2:** Need to audit the complete `SecurityLevel` dispatch graph at refactor time. The research identified 6+ locations but the actual count may be higher in the GUI and settings code.
- **Phase 3:** Need to verify no Linux-specific behavior is baked into the protocol or server logic (not just the platform layer).

Phases with standard patterns (skip research-phase):
- **Phase 4:** Smart pointer migration and `std::format` adoption are well-documented C++ modernization patterns. Clang-Tidy can automate much of this.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Asio is industry standard; C++20 features are well-documented. Source quality is high (official docs, Context7 library analysis, StackOverflow consensus). |
| Features | HIGH | Table stakes directly verified in Deskflow protocol types and competitor analysis. Anti-features aligned with PROJECT.md explicit removal targets. |
| Architecture | HIGH | Root cause verified by direct code analysis of SocketMultiplexer.cpp. Recommended pipelined pattern is established in game networking and real-time systems. Performance targets (sub-8ms) are conservative estimates. |
| Pitfalls | HIGH | Pitfalls 1, 4, 5, 6, 7 verified by direct codebase reading. Pitfalls 2, 3 verified by tracing code paths. Recovery strategies are practical. |

**Overall confidence:** HIGH

### Gaps to Address

- **Asio vs. direct OS API decision:** Research confirms both approaches work, but the final choice depends on whether the `IArchNetwork` abstraction is kept during migration (incremental) or removed (full rewrite). This decision should be made during Phase 1 planning based on risk tolerance.
- **Lock-free queue validation on Windows:** SPSC ring buffer correctness depends on memory ordering guarantees. Research assumes standard C++ `std::atomic` ordering is sufficient, but this needs validation on MSVC during Phase 1 implementation.
- **UDP transport for mouse events:** FEATURES.md research suggests UDP could bring latency from ~45ms to near-ShareMouse levels (<3.4ms). The architecture research recommends staying with TCP + coalescing for simplicity. This is a valid optimization to defer, but should be revisited if Phase 1 does not achieve sub-10ms latency targets.
- **Clipboard chunk priority during input events:** The architecture specifies prioritizing input events over bulk transfers, but the exact interleaving strategy (time-slicing, strict priority, bandwidth partitioning) needs design during Phase 1.

## Sources

### Primary (HIGH confidence)
- Deskflow source code analysis -- SocketMultiplexer.cpp, TCPSocket.cpp, SecureSocket.cpp, Server.cpp, platform/CMakeLists.txt, ProtocolTypes.h
- Context7 `/chriskohlhoff/asio` -- Async TCP patterns, C++20 coroutine support, strand-based concurrency
- Asio official site (think-async.com) -- Standalone vs Boost.Asio comparison, version 1.38.0
- C++20 standard library documentation -- jthread, stop_token, format, span, atomic

### Secondary (MEDIUM confidence)
- ShareMouse vs Barrier latency comparison (Alibaba LifeTips) -- ShareMouse <3.4ms, Barrier ~45ms
- Lan Mouse GitHub -- UDP/DTLS architecture analysis
- C++ Alliance: Asio and Coroutines (April 2025) -- C++20 coroutines ~10% faster than yield_context
- Deskflow Issues #8695, #8379, #8527, #4132 -- Confirmed mouse lag/stutter reports from real users
- CMU/TSE 2017 paper on #ifdef refactoring -- Patterns for platform code removal

### Tertiary (LOW confidence)
- ShareMouse internals (proprietary, inferred from latency characteristics) -- UDP-based transport assumption
- Game networking patterns applied to KVM domain -- Cross-domain pattern application, not domain-specific validation

---
*Research completed: 2026-05-12*
*Ready for roadmap: yes*
