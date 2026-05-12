# Architecture Research: Low-Latency Input Sharing

**Domain:** KVM / keyboard-mouse sharing software (real-time input event pipeline)
**Researched:** 2026-05-12
**Confidence:** HIGH (source code analysis + established patterns from game networking and competitor analysis)

## Executive Summary

Deskflow's stuttering during network fluctuations is an architectural problem, not a tunable parameter. The current `SocketMultiplexer` runs a single thread that holds the job list lock during the entire `pollSocket()` call (which blocks indefinitely with timeout=-1). This means network I/O, event processing, and socket management all contend for the same lock, creating a single choke point where network latency directly stalls input event dispatch.

Modern real-time input systems solve this with three principles: (1) decouple input capture from network I/O using separate threads communicating through lock-free queues, (2) buffer and coalesce mouse movement events so network hiccups produce smooth catch-up rather than stutter, and (3) use non-blocking I/O with bounded timeouts so the network layer never blocks indefinitely.

The recommended architecture replaces the monolithic `SocketMultiplexer` with a pipelined design: Input Capture Thread -> Event Queue (lock-free) -> Network Send Thread on the server side, and Network Receive Thread -> Event Queue (lock-free) -> Input Injection Thread on the client side. Mouse movements should be coalesced (only the latest position matters), while keyboard events must be delivered in order without loss.

## Current Architecture Diagnosis

### The Root Cause: Blocking SocketMultiplexer

The current `SocketMultiplexer::serviceThread()` (in `src/lib/net/SocketMultiplexer.cpp`) has a critical design flaw:

```
serviceThread() loop:
  1. Wait for jobsReady condition variable
  2. Acquire jobListLock  <-- blocks all other socket operations
  3. Collect poll entries from job list
  4. Call ARCH->pollSocket(pfds, size, -1)  <-- BLOCKS INDEFINITELY with lock held
  5. Process results (run jobs)
  6. Release jobListLock
  7. Go to step 1
```

The `-1` timeout on `pollSocket` means the service thread blocks forever waiting for data, **while holding the job list lock**. Any thread that calls `addSocket()` or `removeSocket()` (via `TCPSocket::setJob()`, called during `write()`, `close()`, etc.) must first acquire this same lock. This creates a deadlock-like stall: the poll blocks, the lock is held, and input processing threads cannot update their socket state.

Additionally, the lock handoff protocol (`lockJobListLock` -> `lockJobList`) uses a two-stage locking scheme with raw `new Thread(getCurrentThread())` copies and manual `CondVar` signaling, which is fragile and prone to edge cases (the code even has a `// TODO: Remove this evilness` comment about the cursor mark).

### Second Problem: Synchronous Write Path

When `Server::onMouseMoveSecondary()` calls `m_active->mouseMove(x, y)`, which calls `ProtocolUtil::writef(getStream(), kMsgDMouseMove, xAbs, yAbs)`, which calls `TCPSocket::write()`, the data goes into the output buffer and calls `setJob(newJob())`. This `setJob` call goes into `SocketMultiplexer::addSocket()`, which must acquire the job list lock. If the multiplexer is in `pollSocket`, this write path blocks. The server's input event handler is now blocked on network I/O.

### Third Problem: No Event Coalescing

A mouse at 1000Hz report rate generates 1000 `DMMV` messages per second. Each goes through `Server::onMouseMoveSecondary` -> `ProtocolUtil::writef` -> `TCPSocket::write` -> `SocketMultiplexer::addSocket`. There is no batching or coalescing. When the network slows momentarily, these events queue up in the TCP buffer, and when they arrive at the client, they all must be processed sequentially, causing the cursor to "catch up" in a stuttering pattern.

## Recommended Architecture

### System Overview

```
SERVER (Primary) SIDE:
=============================================================================

┌─────────────────────────────────────────────────────────────────────┐
│                    Platform Input Capture                           │
│  (OSXScreen::onMouseMove / MSWindowsScreen::onMouseMove)           │
│  Hooks into OS event system, detects edge crossing                  │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ (OS callback, platform thread)
                                v
┌─────────────────────────────────────────────────────────────────────┐
│                    EventQueue (existing, enhanced)                  │
│  Dispatches events to registered handlers                          │
│  Runs on dedicated thread (already separate from GUI)              │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ (event dispatch)
                                v
┌─────────────────────────────────────────────────────────────────────┐
│               Server Logic (Server.cpp)                            │
│  onMouseMovePrimary: edge detection, screen switching              │
│  onMouseMoveSecondary: coordinate mapping                         │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ (non-blocking enqueue)
                                v
┌─────────────────────────────────────────────────────────────────────┐
│           Send-Side Event Buffer (NEW)                             │
│  - Mouse move coalescing: keep only latest position                │
│  - Keyboard/key events: FIFO queue, no dropping                   │
│  - Clipboard/file: separate bulk channel                          │
│  - Lock-free SPSC ring buffer (server thread -> send thread)       │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ (send thread owns this)
                                v
┌─────────────────────────────────────────────────────────────────────┐
│           Network Send Thread (NEW, replaces SocketMultiplexer)    │
│  - Owns the socket write side                                      │
│  - Non-blocking writes with bounded retry                          │
│  - Serializes protocol messages                                    │
│  - Flushes coalesced mouse positions at capped rate (e.g. 200Hz)  │
│  - Never blocks input processing                                   │
└─────────────────────────────────────────────────────────────────────┘

CLIENT (Secondary) SIDE:
=============================================================================

┌─────────────────────────────────────────────────────────────────────┐
│           Network Receive Thread (NEW)                             │
│  - Non-blocking poll with short timeout (1-5ms)                    │
│  - Deserializes protocol messages                                  │
│  - Enqueues events into receive-side buffer                        │
│  - Measures RTT / jitter for adaptive smoothing                    │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ (lock-free enqueue)
                                v
┌─────────────────────────────────────────────────────────────────────┐
│          Receive-Side Event Buffer (NEW)                           │
│  - Mouse moves: latest-position-only slot (atomic write)           │
│  - Keyboard events: FIFO queue                                     │
│  - Jitter buffer for mouse: optional short delay for smoothing     │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ (client event loop polls this)
                                v
┌─────────────────────────────────────────────────────────────────────┐
│          Client Event Processing Thread                            │
│  - ServerProxy parses messages (existing logic)                    │
│  - Dispatches to Screen for input injection                        │
│  - Injects mouse at display refresh cadence when possible          │
└───────────────────────────────┬─────────────────────────────────────┘
                                │
                                v
┌─────────────────────────────────────────────────────────────────────┐
│          Platform Input Injection                                  │
│  (OSXScreen::fakeMouseMove / MSWindowsScreen::fakeMouseMove)       │
│  OS API calls to synthesize input                                  │
└─────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Implementation |
|-----------|----------------|----------------|
| InputCapture (platform) | Hook OS events, detect edge crossing, report coordinates | Existing platform layer, minimal changes |
| EventQueue | Dispatch input events to handlers | Existing `EventQueue` in `src/lib/base/` |
| Server Logic | Screen switching, coordinate mapping | Existing `Server.cpp`, minimal changes |
| SendEventBuffer | Coalesce mouse moves, queue keyboard events, decouple send from processing | **NEW** - lock-free SPSC ring buffer |
| NetworkSendThread | Serialize and send protocol messages, non-blocking I/O | **NEW** - replaces `SocketMultiplexer` for data path |
| NetworkReceiveThread | Poll socket, deserialize messages, measure jitter | **NEW** - replaces `SocketMultiplexer` for data path |
| ReceiveEventBuffer | Buffer incoming events, smooth mouse movement | **NEW** - lock-free queue with mouse position slot |
| ClientEventProcessor | Parse messages, dispatch to input injection | Existing `ServerProxy` logic, refactored |
| InputInjection (platform) | Synthesize keyboard/mouse events via OS API | Existing platform layer, minimal changes |
| ConnectionManager | Accept connections, manage lifecycle, keep-alive | Simplified from existing `SocketMultiplexer` for control path |

### Component Boundaries

```
                    WRITES                          READS
InputCapture  ────>  EventQueue  ────>  ServerLogic  ────>  SendEventBuffer
                                                                    |
                                                                    v
                                                            NetworkSendThread
                                                                    |
                                                              [TCP Socket]
                                                                    |
                                                            NetworkRecvThread
                                                                    |
                                                                    v
InputInjection  <────  ClientProcessor  <────  RecvEventBuffer  <───┘
```

Key boundary rule: **the network thread never calls into the event queue or server logic directly**. Communication is always through the buffer components, which use lock-free data structures.

## Architectural Patterns

### Pattern 1: Lock-Free SPSC Ring Buffer Between Pipeline Stages

**What:** A single-producer, single-consumer ring buffer where the producer writes and the consumer reads without any locks. The producer thread (input capture/event processing) writes events into the buffer. The consumer thread (network send) reads from it.

**When to use:** Between any two pipeline stages where one thread produces and one thread consumes. This is the core pattern for the entire input pipeline.

**Trade-offs:**
- Pro: Zero contention between input processing and network I/O
- Pro: Bounded memory usage (fixed-size ring)
- Pro: Cache-friendly (sequential access pattern)
- Con: Fixed capacity -- if the consumer cannot keep up, the producer must decide: block, drop, or coalesce

**Implementation sketch:**

```cpp
// Lock-free SPSC ring buffer for mouse events
// Only the latest position matters, so "coalesce" = overwrite
struct MousePosition {
    int32_t x;
    int32_t y;
    std::atomic<bool> valid{false};
};

alignas(64) MousePosition g_latestMousePos;  // cache-line aligned

// Producer (server event thread):
void enqueueMouseMove(int32_t x, int32_t y) {
    g_latestMousePos.x.store(x, std::memory_order_relaxed);
    g_latestMousePos.y.store(y, std::memory_order_relaxed);
    g_latestMousePos.valid.store(true, std::memory_order_release);
}

// Consumer (network send thread):
bool dequeueMouseMove(int32_t& x, int32_t& y) {
    if (!g_latestMousePos.valid.load(std::memory_order_acquire))
        return false;
    x = g_latestMousePos.x.load(std::memory_order_relaxed);
    y = g_latestMousePos.y.load(std::memory_order_relaxed);
    g_latestMousePos.valid.store(false, std::memory_order_release);
    return true;
}

// Keyboard events need ordering, use a proper SPSC ring:
template<typename T, size_t N>
class SPSCQueue {
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::array<T, N> buffer_;
public:
    bool push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) % N;
        if (next == tail_.load(std::memory_order_acquire)) return false; // full
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false; // empty
        item = buffer_[tail];
        tail_.store((tail + 1) % N, std::memory_order_release);
        return true;
    }
};
```

### Pattern 2: Non-Blocking I/O with Bounded Poll Timeout

**What:** Replace the infinite `pollSocket(pfds, size, -1)` call with `pollSocket(pfds, size, 1)` (1ms timeout) or use `select`/`poll`/`kqueue`/`WSAPoll` with a short timeout. The thread loops: poll -> process ready sockets -> check send buffer -> poll again.

**When to use:** Always, for the network thread. Never block indefinitely on network I/O in a real-time system.

**Trade-offs:**
- Pro: Network thread is always responsive, can check for new send data every 1ms
- Pro: Avoids the lock contention issue entirely (no need to hold a lock during poll)
- Con: Slight CPU overhead from polling (negligible at 1ms intervals)
- Con: Up to 1ms added latency in the worst case (acceptable for LAN)

**Implementation sketch:**

```cpp
// Network send thread main loop
void NetworkSendThread::run() {
    while (!stopped_) {
        // 1. Check for outgoing data in send buffer
        int32_t mouseX, mouseY;
        bool hasMouse = sendBuffer_.dequeueMouseMove(mouseX, mouseY);

        InputEvent keyEvent;
        bool hasKey = sendBuffer_.dequeueKey(keyEvent);

        // 2. Serialize and send anything pending
        if (hasMouse) {
            ProtocolUtil::writef(stream_, kMsgDMouseMove, mouseX, mouseY);
        }
        if (hasKey) {
            sendKeyEvent(keyEvent);
        }

        // 3. Non-blocking poll for incoming data (keep-alive, etc.)
        //    Short timeout so we can check send buffer frequently
        ARCH->pollSocket(pfds, npfd, 0.001); // 1ms timeout

        // 4. Process any incoming data (keep-alive, clipboard acks)
        // ...
    }
}
```

### Pattern 3: Mouse Event Coalescing at Multiple Points

**What:** For mouse movement, only the latest position matters. Coalesce at three points: (1) when enqueueing from server logic to send buffer, (2) when network thread reads the buffer, and (3) when client injects the position. At each point, discard intermediate positions.

**When to use:** For mouse movement events specifically. NOT for keyboard events, button presses, or any event where ordering matters.

**Trade-offs:**
- Pro: During network congestion, mouse position is always the "right now" position -- no catch-up stutter
- Pro: Dramatically reduces bandwidth with high-report-rate mice (1000Hz -> 200Hz effective)
- Con: Some mouse positions are lost (intermediate frames), which is acceptable for cursor movement
- Con: Must never coalesce keyboard or button events

**Why 200Hz target send rate:** A 1000Hz mouse generates 1000 moves/sec, but most displays are 60-144Hz. Sending at 200Hz gives smooth movement up to 200fps while cutting bandwidth by 80%. The client side can further downsample to match display refresh.

### Pattern 4: Separate Control and Data Channels

**What:** Connection management (accept, handshake, keep-alive, disconnect) runs on a separate path from the high-frequency input event data stream. Keep-alive and connection health are not mixed with mouse move serialization.

**When to use:** When the system has both high-frequency data (mouse moves) and low-frequency control (connection management, clipboard sync).

**Implementation approach:** The connection manager thread handles socket accept, the initial handshake, and keep-alive pings. Once a connection is established, it hands the socket file descriptor to the send/receive threads for the data path. The connection manager retains responsibility for detecting dead connections (via keep-alive timeout).

## Data Flow

### Primary Flow: Mouse Movement (Server -> Client)

```
1. OS fires mouse move callback (1000Hz mouse = every 1ms)
2. PlatformScreen::onMouseMove() posts event to EventQueue
3. EventQueue dispatches to Server::onMouseMoveSecondary()
4. Server computes absolute coordinates, calls:
       sendBuffer_.enqueueMouseMove(x, y)    // NON-BLOCKING, overwrites previous
5. NetworkSendThread (separate thread, loops every ~1ms):
   a. Reads latest mouse position from buffer
   b. Serializes: "DMMV" + x(2 bytes) + y(2 bytes) = 8 bytes
   c. Writes to socket (non-blocking)
   d. Polls socket for 1ms (processes keep-alives, etc.)
6. NetworkReceiveThread on client (separate thread):
   a. Polls socket with 1ms timeout
   b. Reads available data, deserializes messages
   c. For DMMV: updates latestMousePosition in receive buffer
7. ClientEventProcessor (event loop):
   a. Reads latest mouse position from receive buffer
   b. Calls Screen::mouseMove(x, y)
   c. PlatformScreen::fakeMouseMove(x, y) injects via OS API
```

Total added latency from pipeline: ~2-4ms (1ms send thread interval + 1ms recv thread interval + serialization).
This is well below human perception for LAN scenarios.

### Secondary Flow: Keyboard Events (Server -> Client)

```
1. OS fires key down/up callback
2. PlatformScreen posts event to EventQueue
3. Server::handleKeyDownEvent() processes key mapping
4. sendBuffer_.pushKey(keyEvent)    // FIFO queue, NO coalescing
5. NetworkSendThread dequeues and sends in order
6. NetworkReceiveThread receives in order
7. ClientEventProcessor injects in order via OS API
```

Keyboard events are never coalesced. Every key down/up must be delivered.

### Flow: Clipboard Sync (Bulk Data)

```
1. Clipboard grab detected -> StreamChunker breaks into chunks
2. Chunks enqueued in separate bulk data queue (lower priority)
3. NetworkSendThread sends chunks interleaved with input events
4. Client reassembles chunks, sets clipboard
```

Clipboard data should not starve input events. The send thread prioritizes input events over clipboard chunks.

## Build Order (Component Dependencies)

The architectural changes should be built in this order because each step is independently testable and later steps depend on earlier ones:

```
Phase 1: Foundation (no behavioral changes)
  1a. Replace raw new/delete in SocketMultiplexer with smart pointers
  1b. Add SPSCQueue template class to src/lib/mt/
  1c. Add SendEventBuffer and RecvEventBuffer classes
  1d. Unit tests for lock-free queues

Phase 2: Network Layer Rewrite
  2a. Create NetworkSendThread class (non-blocking, bounded poll)
  2b. Create NetworkReceiveThread class (non-blocking, bounded poll)
  2c. Create ConnectionManager class (accept, handshake, keep-alive)
  2d. Integration test: send/receive thread pair over localhost

Phase 3: Pipeline Integration
  3a. Wire Server logic output to SendEventBuffer instead of direct socket write
  3b. Wire NetworkReceiveThread output to RecvEventBuffer
  3c. Wire Client event processing to read from RecvEventBuffer
  3d. Mouse move coalescing in SendEventBuffer
  3e. End-to-end latency testing

Phase 4: Cleanup
  4a. Remove old SocketMultiplexer (after new path is validated)
  4b. Remove TLS layer (as planned in PROJECT.md)
  4c. Performance benchmarking and tuning
```

### Dependency Graph

```
1a (smart pointers) ───> 2a/2b (new threads can use modern patterns)
1b (SPSCQueue) ────────> 1c (buffers need queues) ──> 3a/3b/3c (wiring)
2a/2b/2c (threads) ───> 3a/3b/3c (wiring)
3a-3e (integration) ──> 4a (remove old code)
4b (TLS removal) is independent, can be done in parallel
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Blocking the Input Thread on Network I/O

**What people do:** Call socket write directly from the input event handler, or use a blocking send/receive in the event processing path.

**Why it is wrong:** Any network delay (jitter, brief packet loss, TCP retransmit) directly stalls input processing. The user perceives this as mouse/keyboard lag or stuttering. This is exactly what Deskflow does today: `Server::onMouseMoveSecondary` -> `mouseMove` -> `ProtocolUtil::writef` -> `TCPSocket::write` -> `SocketMultiplexer::addSocket` (locks, can block).

**Do this instead:** Enqueue into a lock-free buffer and let a dedicated network thread handle serialization and socket writes. The input processing thread should never touch a socket.

### Anti-Pattern 2: Coalescing All Event Types

**What people do:** Apply the same "keep only latest" strategy to all events, including keyboard and button presses.

**Why it is wrong:** Mouse movement is state-based (current position is all that matters). Keyboard and button events are transition-based (press then release). Dropping a key release event means a stuck key. Dropping a click means a lost action.

**Do this instead:** Coalesce only mouse move events. Use FIFO queues for keyboard events, button events, and all other message types. Tag events with their coalescing policy at the buffer level.

### Anti-Pattern 3: Shared Mutex Between Threads in the Hot Path

**What people do:** Use `std::mutex` or a custom `Mutex`/`CondVar` to synchronize between the input processing thread and the network thread on every event.

**Why it is wrong:** Mutex contention in the hot path (every mouse move) adds unpredictable latency spikes. The current `SocketMultiplexer` has a complex multi-stage locking protocol (`lockJobListLock` -> `lockJobList`) that is hard to reason about and creates subtle contention.

**Do this instead:** Use lock-free atomics or SPSC ring buffers for the hot path. Reserve mutexes for initialization, teardown, and the control path (connection management).

### Anti-Pattern 4: Infinite Timeout on Poll

**What people do:** Call `poll()` or `select()` with an infinite timeout, relying on socket state changes to wake the thread.

**Why it is wrong:** The thread cannot check for new outgoing data or other state changes while blocked. Other threads must use signaling mechanisms (like `unblockPollSocket`) to wake the polling thread, which introduces complexity and latency.

**Do this instead:** Use a short bounded timeout (1-5ms). This ensures the network thread can check its send buffer frequently and respond to shutdown requests promptly.

### Anti-Pattern 5: Sending Every Mouse Event Over the Network

**What people do:** Treat every mouse move event from the OS as a separate network message.

**Why it is wrong:** High-report-rate mice (500Hz, 1000Hz) generate far more events than the network can usefully transmit or the receiving display can render. Each event goes through serialization, TCP overhead, and deserialization. This saturates the TCP send buffer during normal operation and creates a backlog during any momentary network delay.

**Do this instead:** Coalesce mouse moves at the send buffer level. Send at a capped rate (200-250Hz is sufficient for any display). During network congestion, the backlog is a single "latest position" rather than hundreds of queued intermediate positions.

## Scalability Considerations

| Scenario | Architecture Handling |
|----------|-----------------------|
| Single client, 1000Hz mouse, perfect LAN | Coalesce to 200Hz, sub-5ms total pipeline latency |
| Single client, WiFi with 50ms jitter spikes | Receive buffer smooths delivery; mouse always shows latest position |
| Single client, brief network dropout (200ms) | Keyboard events queue (bounded), mouse resumes at latest position; no stutter catch-up |
| Clipboard/file transfer in progress | Bulk data uses lower-priority queue; input events always prioritized |
| Multiple clients (server switching between 2-4) | One SendEventBuffer per client; only active client's buffer is consumed at high rate |

### Performance Targets

| Metric | Target | How Architecture Achieves It |
|--------|--------|------------------------------|
| End-to-end mouse latency (LAN) | < 8ms | 1ms send interval + network + 1ms recv interval + injection |
| Stutter during 50ms network jitter | None visible | Mouse shows latest position on arrival; no positional catch-up queue |
| CPU overhead at idle | < 1% | Send/recv threads sleep on 1ms poll when no events |
| CPU overhead at 1000Hz mouse input | < 3% | Coalescing reduces effective rate to 200Hz |

## How Competitors Handle This

### Lan Mouse (Rust, UDP-based)

Lan Mouse uses **UDP** for transport rather than TCP. This is the most aggressive approach to avoiding head-of-line blocking: UDP packets are independent, so a lost packet does not delay subsequent ones. Mouse positions are sent as individual UDP datagrams; if one is lost, the next one corrects. Encryption is via DTLS.

**Applicability to Deskflow:** Switching from TCP to UDP is a larger architectural change and requires implementing reliability for keyboard events (which must not be lost). The lock-free pipeline approach achieves similar benefits with less disruption. UDP could be a future optimization if the TCP approach proves insufficient, but for LAN scenarios with good retransmit, TCP with coalescing is adequate.

### Input Leap / Barrier (C++, Synergy fork)

Input Leap shares the same `SocketMultiplexer` architecture as Deskflow (they are forks of the same codebase). They have the same fundamental problem. Some forks have experimented with TCP_NODELAY (which Deskflow already sets in `TCPSocket::init()`) and adjusting poll timeouts, but none have fundamentally restructured the pipeline.

### Game Networking (general reference)

Real-time multiplayer games have solved this problem thoroughly. The standard pattern is:
1. **Client-side prediction:** Interpolate between received positions
2. **Server-side rate limiting:** Send state updates at a fixed rate (e.g., 60Hz)
3. **Delta compression:** Only send changes
4. **Separate reliable and unreliable channels:** UDP for positions (loss-tolerant), reliable for state changes (must-deliver)

For Deskflow, the most relevant pattern is the **separate reliable/unreliable channel** concept. Mouse moves are unreliable (latest wins), keyboard events are reliable (FIFO, no drops).

## Integration Points

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Server Logic <-> SendEventBuffer | Lock-free SPSC enqueue | Server thread is producer, never blocks |
| SendEventBuffer <-> NetworkSendThread | Lock-free SPSC dequeue | Send thread is consumer, polls at 1ms intervals |
| NetworkReceiveThread <-> RecvEventBuffer | Lock-free SPSC enqueue | Receive thread is producer |
| RecvEventBuffer <-> ClientProcessor | Lock-free SPSC dequeue | Client event thread is consumer |
| ConnectionManager <-> NetworkThreads | Socket FD handoff + atomic flags | Connection manager owns lifecycle |
| PlatformScreen <-> EventQueue | Existing event dispatch | No changes needed |

### External Dependencies

| Dependency | Purpose | Notes |
|------------|---------|-------|
| TCP sockets (via `IArchNetwork`) | Transport layer | Keep existing abstraction; change how it is used |
| OS input APIs (CGEvent, SendInput) | Input capture and injection | No changes needed |
| Qt event loop (GUI) | GUI process, IPC | Unaffected by core pipeline changes |

## Sources

- Deskflow source code analysis: `src/lib/net/SocketMultiplexer.cpp`, `src/lib/net/TCPSocket.cpp`, `src/lib/server/Server.cpp`, `src/lib/base/EventQueue.h` (HIGH confidence -- direct code reading)
- Deskflow Issue #8527: High report rate mouse causing drift (HIGH confidence -- confirms the problem)
- Deskflow Issue #4132: Laggy mouse cursor on macOS clients (HIGH confidence -- long-standing report)
- [Lan Mouse GitHub](https://github.com/feschber/lan-mouse): UDP-based input sharing architecture (MEDIUM confidence -- README analysis, not deep source)
- [Stack Overflow: Non-blocking socket with poll](https://stackoverflow.com/questions/3360797/non-blocking-socket-with-poll) (HIGH confidence -- established pattern)
- Game networking patterns (lock-free pipelines, event coalescing): Industry standard, applied to this domain (HIGH confidence)
- [Unity NetCode input batching](https://docs.unity3d.com/Packages/com.unity.netcode@1.2/api/Unity.NetCode.html): Distinguishes coalescable vs non-coalescable inputs (MEDIUM confidence -- applies pattern to different domain)

---
*Architecture research for: Deskflow low-latency input pipeline*
*Researched: 2026-05-12*
