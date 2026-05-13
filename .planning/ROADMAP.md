# Roadmap: Deskflow (个人精简版)

## Overview

Fix the core stuttering problem by rewriting the network pipeline, then systematically remove unused code (TLS, Linux, screensaver sync), modernize the remaining C++ to safe idioms, and validate everything works. The journey goes from a broken network layer to a lean, modern, macOS+Windows-only KVM tool with sub-15ms latency.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Network Pipeline Rewrite** - Replace blocking SocketMultiplexer with pipelined architecture using lock-free buffers
- [x] **Phase 2: TLS/Encryption Removal** - Remove entire TLS layer and all SecurityLevel dispatch code
- [ ] **Phase 3: Platform & Feature Cleanup** - Remove Linux support, screensaver sync, and residual dead code
- [ ] **Phase 4: C++ Modernization** - Replace raw new/delete, custom Thread, reinterpret_cast patterns with safe idioms
- [ ] **Phase 5: Build Validation & Regression** - Verify both platforms build, all tests pass, no feature regressions

## Phase Details

### Phase 1: Network Pipeline Rewrite
**Goal**: Mouse and keyboard input flows smoothly even during network jitter, with no stuttering or stuck keys
**Mode:** mvp
**Depends on**: Nothing (first phase, core value)
**Requirements**: NET-01, NET-02, NET-03, NET-04, NET-05, NET-06
**Success Criteria** (what must be TRUE):
  1. Mouse cursor moves smoothly across screen edges during simulated network jitter (packet loss up to 5%, latency spikes up to 200ms) with no visible stutter or catch-up bursts
  2. Keyboard input is never lost -- every keypress event is received by the client even under network stress
  3. When network connection drops, all pressed keys and mouse buttons are immediately released on the client (no stuck keys)
  4. Mouse move events are coalesced before sending -- at 1000Hz input rate, the network sends at most the latest position at 200Hz
  5. End-to-end mouse latency is 15ms or less under normal LAN conditions (no jitter)
**Plans**: 3 plans

Plans:
- [x] 01-01: Create Asio socket infrastructure (AsioTCPSocket, AsioTCPListenSocket, AsioTCPSocketFactory) and integrate Asio into build
- [x] 01-02: Implement lock-free SPSC event buffers with mouse move coalescing and keyboard FIFO queue, integrated into AsioTCPSocket
- [x] 01-03: Connect upper-layer ClientProxy to SPSC pipeline, add disconnect/reconnect handling, wire AsioTCPSocketFactory into ServerApp/ClientApp

### Phase 2: TLS/Encryption Removal
**Goal**: The TLS/SSL encryption layer is completely gone with no remnant dispatch code, and all connections use plain-text mode without any configuration option for encryption
**Mode:** mvp
**Depends on**: Phase 1 (stable network layer before removing code that touches it)
**Requirements**: CLEAN-01
**Success Criteria** (what must be TRUE):
  1. No SecurityLevel enum, no TLS-related code, and no OpenSSL references exist anywhere in the source tree
  2. Client and server connect successfully in plain-text mode with no encryption handshake
  3. GUI no longer shows any encryption or security level options
  4. Build succeeds without OpenSSL as a dependency
**Plans**: 3 plans

Plans:
- [x] 02-01: Trace and remove all SecurityLevel dispatch paths across client, server, GUI, and factory code
- [x] 02-02: Remove SecureSocket, TLS certificate files, and OpenSSL build dependencies
- [x] 02-03: Hardcode plain-text mode and remove all encryption-related GUI settings

### Phase 3: Platform & Feature Cleanup
**Goal**: The codebase only contains macOS and Windows platform code, with no Linux remnants and no disabled features leaving dead code paths
**Mode:** mvp
**Depends on**: Phase 2 (cleaner codebase makes platform boundaries clearer)
**Requirements**: CLEAN-02, CLEAN-03, CLEAN-04, CLEAN-05
**Success Criteria** (what must be TRUE):
  1. No Linux/X11/Wayland/EI source files or headers exist in the source tree (44 files removed)
  2. No `#ifdef` guards for Linux-specific code paths remain in shared code
  3. Screensaver sync feature is completely removed with no residual references
  4. All CMakeLists.txt files compile only macOS and Windows modules, with no Linux dependencies (libei, libportal, xkbcommon)
**Plans**: 3 plans

Plans:
- [ ] 03-01: Remove all Linux platform files (X11, Wayland, EI) and update CMakeLists.txt
- [ ] 03-02: Remove screensaver sync feature and clean all residual #ifdef branches
- [ ] 03-03: Grep for and remove all remaining dead code paths from removed features

### Phase 4: C++ Modernization
**Goal**: All manual memory management, custom threading, and unsafe pointer casts are replaced with modern C++20 idioms
**Mode:** mvp
**Depends on**: Phase 3 (architecture is stable, no more structural changes)
**Requirements**: MOD-01, MOD-02, MOD-03, MOD-04
**Success Criteria** (what must be TRUE):
  1. No raw `new`/`delete` calls remain in the SocketMultiplexer and related network code -- all replaced with smart pointers
  2. All custom `Thread` class usage replaced with `std::jthread` + `std::stop_token` for cooperative cancellation
  3. No `reinterpret_cast` cursor-mark sentinel patterns remain -- replaced with type-safe iteration
  4. No `exit(0)` calls remain anywhere in the codebase -- all replaced with graceful event queue shutdown
**Plans**: 3 plans

Plans:
- [ ] 04-01: Replace SocketMultiplexer cursor-mark sentinel pattern with type-safe iteration, then migrate to smart pointers
- [ ] 04-02: Replace all Thread class usage with std::jthread and std::stop_token
- [ ] 04-03: Replace all exit(0) calls with graceful event queue exit mechanism
- [ ] 04-04: Replace remaining reinterpret_cast patterns with safe iteration across codebase

### Phase 5: Build Validation & Regression
**Goal**: The application builds and runs correctly on both supported platforms with no regressions in existing functionality
**Mode:** mvp
**Depends on**: Phase 4 (all code changes complete)
**Requirements**: BQ-01, BQ-02, BQ-03, BQ-04, BQ-05
**Success Criteria** (what must be TRUE):
  1. Clean build succeeds on macOS (clang) and Windows (MSVC) with zero errors and zero warnings
  2. All existing automated tests pass on both platforms
  3. GUI configuration界面 opens, displays correct settings, and can configure client/server mode
  4. Clipboard text and image sharing works between macOS and Windows
  5. File drag-and-drop transfer works between macOS and Windows
**Plans**: 3 plans

Plans:
- [ ] 05-01: Fix build errors and warnings on macOS and Windows
- [ ] 05-02: Run and fix all existing automated tests
- [ ] 05-03: Manual smoke test of GUI, clipboard sharing, and file drag-and-drop on both platforms

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Network Pipeline Rewrite | 3/3 | Complete | 2026-05-13 |
| 2. TLS/Encryption Removal | 3/3 | Complete | 2026-05-13 |
| 3. Platform & Feature Cleanup | 0/3 | Not started | - |
| 4. C++ Modernization | 0/4 | Not started | - |
| 5. Build Validation & Regression | 0/3 | Not started | - |
