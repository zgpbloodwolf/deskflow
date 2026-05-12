# Stack Research

**Domain:** Low-latency KVM/keyboard-mouse sharing over LAN (C++, cross-platform macOS + Windows)
**Researched:** 2026-05-12
**Confidence:** HIGH

## Executive Summary

Deskflow's existing network layer uses a hand-rolled `SocketMultiplexer` built on `poll()` with manual memory management (`new`/`delete`), raw condition variables, and a cursor-based linked list for safe concurrent iteration. The design dates from the early 2000s and has known issues: blocking I/O stalls the input processing thread during network jitter, raw pointers risk use-after-free and memory leaks, and the threading model is overly complex (lock-a-lock-to-lock-the-lock pattern in `lockJobListLock`/`lockJobList`).

The primary network stutter problem is architectural, not protocol-level: the existing code already sets `TCP_NODELAY` (confirmed in `TCPSocket::init()` line 285), so Nagle's algorithm is not the culprit. The real issue is that `poll()` with infinite timeout (`-1`) in `serviceThread` blocks until kernel events arrive, and when combined with the heavy mutex/condvar dance for job list management, any network fluctuation causes the entire input pipeline to stall.

The recommended approach: **modernize incrementally, not rewrite from scratch.** Replace the `SocketMultiplexer` with standalone Asio for async I/O, use C++20 features (`std::jthread`, `std::stop_token`, smart pointers, `std::format`) to clean up the surrounding code, and keep the existing `IArchNetwork` abstraction layer as a bridge during migration.

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **Standalone Asio** | 1.38.0 | Async I/O and networking | Header-only (no Boost dependency). Cross-platform: uses `epoll` on Linux, `kqueue` on macOS, IOCP on Windows. C++20 coroutine support (`co_await` with `use_awaitable`) is ~10% faster than callback-based async. Direct replacement for the current `SocketMultiplexer` + `poll()` architecture. Industry standard; likely basis for future `std::networking`. |
| **C++20** | N/A | Language standard | Already set in CMakeLists.txt. Provides `std::jthread`, `std::stop_token`, `std::format`, `std::span`, concepts, and structured bindings -- all directly applicable to the modernization goals. |
| **Qt 6** | 6.x (existing) | GUI framework | Already in use. No change needed. |
| **CMake** | 3.24+ (existing) | Build system | Already in use. No change needed. |

### Networking Layer Detail

The single most impactful change: replace `SocketMultiplexer` with Asio's `io_context`.

**Current architecture (the problem):**
```
serviceThread() -> pollSocket(pfds, -1)  // blocks indefinitely
  -> lockJobListLock() -> lockJobList()  // complex mutex dance
    -> iterate jobs with cursors         // fragile linked list iteration
      -> job->run(read, write, error)    // synchronous handler
```

**Target architecture with Asio:**
```
io_context.run()                          // single event loop, all platforms
  -> async_read_some(handler)             // non-blocking, kernel-driven
  -> async_write(handler)                 // non-blocking, kernel-driven
  // No manual poll(), no mutex dance, no cursor management
  // Strand for thread safety instead of Mutex + CondVar
```

Key Asio features that solve the current problems:

1. **`io_context`** replaces `SocketMultiplexer`: single event loop that uses the best available OS mechanism (`kqueue` on macOS, IOCP on Windows). No manual `poll()` calls.
2. **`asio::strand`** replaces the `Mutex` + `CondVar` + cursor pattern: serialized handler execution without explicit locking. Eliminates the `lockJobListLock`/`lockJobList`/`unlockJobList` complexity entirely.
3. **`asio::ip::tcp::socket::lowest_layer().set_option(tcp::no_delay(true))`** replaces `ARCH->setNoDelayOnSocket()` -- but this is already set, so the improvement comes from the async model, not socket options.
4. **C++20 coroutines** (`co_await` + `use_awaitable`): makes async code read like synchronous code, eliminating callback spaghetti.

**Why standalone Asio, not Boost.Asio:**
- Header-only, no Boost build dependency (Deskflow does not use Boost)
- Same API, namespace `asio::` instead of `boost::asio::`
- Faster access to bug fixes (updated independently of Boost releases)
- Current version: Asio 1.38.0 (Dec 2025), Boost-integrated: Asio 1.38.1 in Boost 1.91

**Why not libuv:**
- C library, not C++ native (requires wrapper code)
- Does not support C++20 coroutines
- Fundamental model mismatch with `io_uring` (open issue #4044)
- Less idiomatic for a C++ codebase

**Why not raw `io_uring`:**
- Linux-only (Deskflow needs macOS + Windows)
- Overkill for a single TCP connection (2 machines on LAN)
- `io_uring` can be slower than `epoll`/`kqueue` for simple network I/O with few sockets

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **`std::jthread`** | C++20 stdlib | Thread management with auto-join and cooperative cancellation | Replace all `Thread` class usage. `std::jthread` auto-joins on destruction (eliminates `delete m_thread` leaks) and provides `std::stop_token` for clean shutdown (replaces `Thread::cancel()` pattern). |
| **`std::stop_token`** | C++20 stdlib | Cooperative thread interruption | Replace the current cancel/testCancel pattern. `jthread` passes `stop_token` as first argument to thread function. |
| **`std::unique_ptr`** | C++11 stdlib | Single-ownership smart pointers | Replace all `new`/`delete` in SocketMultiplexer, TCPSocket, and platform code. Default choice for heap allocations. Use `std::make_unique`. |
| **`std::shared_ptr`** | C++11 stdlib | Shared-ownership smart pointers | Use only when truly shared ownership is needed (e.g., objects shared across async callback chains). Prefer `unique_ptr` + references. |
| **`std::format`** | C++20 stdlib | Type-safe string formatting | Replace `LOG_DEBUG("opening new socket: %08X", m_socket)` style printf formatting. Better type safety, compile-time format string checking. |
| **`std::span`** | C++20 stdlib | Non-owning view over contiguous data | Replace raw pointer + size parameters in buffer operations (e.g., `read(void* buffer, uint32_t n)` becomes `read(std::span<uint8_t> buffer)`). |
| **`std::atomic`** | C++11 stdlib | Lock-free atomic operations | Replace `m_update` bool flag and other simple shared state. Avoids mutex overhead for flag variables. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| **Clang-Tidy** | Static analysis and modernization checks | Use `modernize-use-auto`, `modernize-use-override`, `modernize-use-nullptr`, `modernize-use-smart-ptr` checks to automate migration. |
| **Clang-Format** | Code formatting | Already presumably in use. Ensure `.clang-format` config is consistent. |
| **AddressSanitizer (ASan)** | Memory error detection | Critical during migration: detects use-after-free, buffer overflows, leaks. Enable with `-fsanitize=address` during development builds. |
| **ThreadSanitizer (TSan)** | Data race detection | Essential when refactoring the threading model. Detects data races, deadlocks. Enable with `-fsanitize=thread`. |

## Alternatives Considered

| Category | Recommended | Alternative | When to Use Alternative |
|----------|-------------|-------------|-------------------------|
| Async I/O | **Standalone Asio** | Boost.Asio | If project already depends on Boost for other libraries. Functionally identical API. |
| Async I/O | **Standalone Asio** | libuv | If you need a C-compatible API or are integrating with a C codebase. Less idiomatic C++. |
| Async I/O | **Standalone Asio** | Raw OS APIs (poll/kqueue/IOCP) | Only for extreme performance optimization with very specific requirements. Massive cross-platform complexity for negligible gain with 1-2 sockets. |
| Transport | **TCP (keep)** | UDP + KCP/ENet | If LAN has >5% packet loss (unusual for wired LAN). TCP is simpler, already works, and avoids implementing reliability on top of UDP. |
| Transport | **TCP (keep)** | QUIC | Overkill for LAN. Encryption overhead (being removed per project requirements). Complex library dependency. |
| Threading | **std::jthread** | Raw pthreads / Win32 threads | Never. The current custom `Thread` class wrapping pthreads/Win32 is exactly the kind of code to eliminate. |
| Threading | **std::jthread** | Thread pools | For parallel computation tasks. Not needed here -- the main benefit is auto-join and stop_token, not pool management. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **Raw `new`/`delete`** | Primary source of memory leaks and use-after-free in current codebase (SocketMultiplexer destructor deletes 5 raw pointer members, any missed path leaks). | `std::make_unique` / `std::make_shared` |
| **`poll()` with infinite timeout** | Blocks the service thread during network jitter. Combined with the heavy mutex dance, this is the root cause of input stutter. | Asio `io_context::run()` with async operations |
| **Manual Mutex + CondVar for job list** | The `lockJobListLock` -> `lockJobList` -> `unlockJobList` pattern is error-prone and complex (3 condvars, 2 thread pointer members). | `asio::strand` for serialized handler execution |
| **Custom `Thread` class** | Wraps pthreads/Win32 with manual cancel/join semantics. Destructor does not join, causing resource leaks. | `std::jthread` (auto-join, cooperative cancellation) |
| **TLS/SSL layer** | Project decision: LAN-only use, encryption unnecessary. Removing ~3000+ lines. | Plain TCP (already the case for TCPSocket; SecureSocket is the TLS wrapper being removed) |
| **UDP + reliability layer** | Adds complexity for no benefit on stable LAN. TCP with `TCP_NODELAY` already provides low-latency delivery. If packet loss is a problem, fix the network, not the protocol. | Keep TCP with `TCP_NODELAY` |
| **`printf`-style formatting** | Not type-safe, no compile-time checking. Current code uses `%08X` for socket handles, `%s` for strings -- fragile. | `std::format` or `fmt::lib` (std::format backend) |
| **`memset(buffer, 0, sizeof(buffer))`** | Unnecessary zeroing before `readSocket` overwrites the buffer anyway (TCPSocket::doRead line 304). | Remove entirely, or use `std::array` initialization |
| **C-style void\* thread function signature** | `[[noreturn]] void serviceThread(const void *)` -- no type safety, requires casting. | Lambda with captures, or `std::jthread` function signature |

## Stack Patterns by Variant

**If the migration must be incremental (recommended):**
- Keep the `IArchNetwork` abstraction layer during Phase 1
- Implement Asio-based `SocketMultiplexer` replacement behind the same interface
- Gradually replace `IArchNetwork` calls with direct Asio calls in later phases
- This allows testing each platform (macOS, Windows) independently

**If doing a full rewrite of the network layer:**
- Remove `IArchNetwork`, `SocketMultiplexer`, and all arch network code
- Use Asio directly in `TCPSocket` and `TCPListenSocket`
- Single codebase for all platforms (Asio handles OS differences internally)
- Higher risk, but cleaner end state

**If network jitter persists after async I/O migration:**
- Add a small (1-2 frame) jitter buffer on the receiving side for mouse motion events
- Implement mouse motion prediction: when packets are delayed, extrapolate position based on last known velocity
- Add `TCP_QUICKACK` on Linux (macOS equivalent is automatic with `TCP_NODELAY`)
- Consider send-side batching: accumulate mouse motion events for up to 1ms and send as a single packet

## Networking Latency Analysis

### Root Cause of Current Stutter

Based on code analysis, the stutter during network fluctuation is caused by:

1. **Blocking `poll()` with infinite timeout**: `ARCH->pollSocket(&pfds[0], (int)pfds.size(), -1)` in `serviceThread` line 172. When the network stalls, this call blocks until data arrives or an error occurs. During this time, no new socket jobs can be processed.

2. **Heavy synchronization overhead**: Every socket operation (add/remove/run) requires locking the job list through a 3-stage mutex/condvar dance. During network fluctuation, error handling triggers rapid add/remove cycles, causing contention.

3. **No timeout or fallback**: The service thread has no mechanism to detect staleness. If `poll()` returns after a long delay, queued mouse events are stale but still processed.

4. **No separation of input and control channels**: Mouse/keyboard input events share the same TCP connection and processing pipeline as control messages (clipboard, screen info, etc.). A large clipboard transfer can momentarily delay input processing.

### What Already Works

- `TCP_NODELAY` is already set (line 285 of TCPSocket.cpp) -- Nagle's algorithm is disabled
- Keep-alive is enabled for dead connection detection
- The `doRead()` method does read-loop "slurp" to drain available data efficiently
- The protocol sends mouse motion as individual small packets (correct for low latency)

### What Needs to Change

1. **Replace blocking poll with async I/O** (Asio migration) -- this is the big win
2. **Add send-side timestamping** to detect and discard stale mouse events
3. **Prioritize input events over bulk transfers** (clipboard, file drag)
4. **Add reconnection with state preservation** so brief network drops don't lose cursor position

## C++20 Feature Migration Map

### Features to Adopt Now (High Value, Low Risk)

| Feature | Replaces | Where in Codebase | Impact |
|---------|----------|-------------------|--------|
| `std::jthread` | Custom `Thread` class | `src/lib/mt/Thread.h`, all thread creation | Auto-join eliminates resource leaks. `stop_token` replaces cancel pattern. |
| `std::unique_ptr` / `std::make_unique` | Raw `new`/`delete` | SocketMultiplexer, TCPSocket, platform code | Eliminates leaks and use-after-free. The destructor currently has 5 raw deletes -- any missed path leaks. |
| `std::format` | printf-style `LOG_*` macros | Throughout codebase | Type safety, compile-time checking. |
| `std::span` | Raw pointer + size parameters | `read()`, `write()`, buffer operations | Safer, more expressive. |
| `std::atomic<bool>` | `bool` flags protected by mutex | `m_update` in SocketMultiplexer, `m_connected`/`m_readable`/`m_writable` in TCPSocket | Eliminates unnecessary mutex overhead for simple flags. |
| `enum class` | Already used partially | Consistent usage throughout | Already adopted; ensure new code follows pattern. |
| Structured bindings | Iterator pair access | SocketJobMap iteration | Cleaner iteration code. |

### Features to Adopt Carefully (Medium Value, Some Complexity)

| Feature | Use Case | Caveat |
|---------|----------|--------|
| **C++20 coroutines** (`co_await`) | Async I/O handlers with Asio | Requires Asio integration first. ~10% faster than callbacks. Makes async code readable. Do NOT use as threading replacement. |
| **Concepts** | Template constraints for socket/platform types | Useful for new code. Not worth retrofitting everywhere. |
| **`constexpr`** | Compile-time computation for protocol constants | Minor optimization. Low priority. |

### Features to Defer

| Feature | Why Defer |
|---------|-----------|
| **Modules** | Major build system change. CMake support still evolving. Not worth the disruption for this project. |
| **Ranges** | Nice for data transformation but not critical for this codebase. Use where natural in new code. |
| **`std::expected`** (C++23) | Not available in C++20. Could be backported via third-party lib but adds dependency. Use exceptions as currently done. |

## Version Compatibility

| Component | Version | Compatible With | Notes |
|-----------|---------|-----------------|-------|
| Standalone Asio 1.38.0 | Dec 2025 | C++11+ (C++20 for coroutines) | Header-only, no linking required. Define `ASIO_STANDALONE` before including. |
| Qt 6.x | Current | C++17+ | Already in use. No change. |
| CMake 3.24+ | Existing | All platforms | Already set as minimum. |
| Clang 15+ / GCC 12+ / MSVC 19.34+ | C++20 support | All platforms | Required for full C++20 support including jthread, format, concepts. |

## Installation

```bash
# Asio (header-only, no build needed)
# Option 1: vcpkg
vcpkg install asio

# Option 2: FetchContent in CMake (recommended for this project)
# Add to CMakeLists.txt:
# include(FetchContent)
# FetchContent_Declare(asio GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git GIT_TAG asio-1-38-0)
# FetchContent_MakeAvailable(asio)
# target_link_libraries(deskflow PRIVATE asio)

# No additional libraries needed -- all other recommended
# technologies are C++20 standard library features.
```

## Sources

- Context7 `/chriskohlhoff/asio` -- Async TCP patterns, C++20 coroutine support, strand-based concurrency, io_context threading model
- [Asio C++ Library official site](https://think-async.com/) -- Standalone vs Boost.Asio comparison, version 1.38.0 (Dec 2025)
- [Asio and Boost.Asio comparison](https://think-async.com/Asio/AsioAndBoostAsio.html) -- Header-only benefits, namespace differences (HIGH confidence)
- [Boost.Asio revision history](https://www.boost.org/doc/libs/latest/doc/html/boost_asio/history.html) -- Version 1.38.1 in Boost 1.91
- [C++ Alliance: Asio, Coroutines, Modules (April 2025)](https://cppalliance.org/ruben/2025/04/10/Ruben2025Q1Update.html) -- C++20 coroutines ~10% faster than yield_context (MEDIUM confidence)
- [StackOverflow: Comparing Boost.Asio, libunifex, liburing, CppCoro](https://stackoverflow.com/questions/73128677/comparing-boost-asio-libunifex-liburing-and-cppcoro) -- Library comparison (MEDIUM confidence)
- [libuv io_uring issue #4044](https://github.com/libuv/libuv/issues/4044) -- Fundamental model mismatch between libuv and io_uring (HIGH confidence)
- [KCP Protocol GitHub](https://github.com/skywind3000/kcp) -- Reliable UDP alternative (not recommended for this project; kept TCP)
- ["It's always TCP_NODELAY"](https://brooker.co.za/blog/2024/05/09/nagle.html) -- TCP_NODELAY best practice (already set in current codebase)
- [Red Hat: Improving Network Latency Using TCP_NODELAY](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux_for_real_time/8/html/optimizing_rhel_8_for_real_time_for_low_latency_operation/assembly_improving-network-latency-using-tcp_nodelay_optimizing-rhel8-for-real-time-for-low-latency-operation) -- Official documentation
- [C++20 std::jthread and std::stop_token](https://zhuanlan.zhihu.com/p/702409325) -- Cooperative thread cancellation (HIGH confidence, standard feature)
- [KDAB: Practical Programmer's Guide to C++20](https://www.kdab.com/the-practical-programmers-guide-to-cpp20/) -- Coroutines are NOT a threading replacement (HIGH confidence)

---
*Stack research for: Deskflow KVM/keyboard-mouse sharing network modernization*
*Researched: 2026-05-12*
