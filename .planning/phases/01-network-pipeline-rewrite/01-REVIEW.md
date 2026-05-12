---
phase: 01-network-pipeline-rewrite
reviewed: 2026-05-12T00:00:00Z
depth: standard
files_reviewed: 20
files_reviewed_list:
  - CMakeLists.txt
  - cmake/FetchAsio.cmake
  - src/lib/net/CMakeLists.txt
  - src/lib/net/AsioTCPSocket.h
  - src/lib/net/AsioTCPSocket.cpp
  - src/lib/net/AsioTCPListenSocket.h
  - src/lib/net/AsioTCPListenSocket.cpp
  - src/lib/net/AsioTCPSocketFactory.h
  - src/lib/net/AsioTCPSocketFactory.cpp
  - src/lib/net/SpscQueue.h
  - src/lib/net/InputEventBuffer.h
  - src/lib/net/InputEventBuffer.cpp
  - src/lib/net/KeyStateTable.h
  - src/lib/net/KeyStateTable.cpp
  - src/lib/server/ClientProxy1_0.cpp
  - src/lib/server/ClientProxy1_1.cpp
  - src/lib/server/ClientProxy1_8.cpp
  - src/lib/server/CMakeLists.txt
  - src/lib/deskflow/ClientApp.cpp
  - src/lib/deskflow/ServerApp.cpp
findings:
  critical: 4
  warning: 8
  info: 3
  total: 15
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-05-12
**Depth:** standard
**Files Reviewed:** 20
**Status:** issues_found

## Summary

Reviewed all 20 source files for the Phase 1 Network Pipeline Rewrite. The implementation replaces the blocking SocketMultiplexer with Asio-based async I/O using lock-free SPSC buffers. Several significant issues were found across thread safety, resource management, and correctness domains. The most severe are: SPSC queue push silently drops keyboard events when full, accepted sockets run on the wrong io_context, `releaseAllKeys` attempts network writes after disconnect, and `onWriteComplete` accesses shared state outside the mutex. Additionally, the auto-reconnect system has no upper bound on retry attempts and will conflict with the existing ClientApp-level retry logic.

## Critical Issues

### CR-01: SPSC queue push failure silently drops keyboard events

**File:** `src/lib/server/ClientProxy1_0.cpp:274`, `src/lib/server/ClientProxy1_1.cpp:37`, `src/lib/server/ClientProxy1_8.cpp:51`
**Issue:** The `pushKeyboardEvent()` return value (bool indicating success/failure) is completely ignored in all three ClientProxy overrides. When the SPSC FIFO is full (capacity 256), keyboard events are silently discarded. For a keyboard/mouse sharing application, lost key-down events result in permanently stuck keys on the remote screen. The user would have to physically press and release the stuck key to recover.

**Fix:**
```cpp
// In ClientProxy1_0::keyDown (and similar for keyUp, keyRepeat, 1_1, 1_8):
auto *asioSocket = dynamic_cast<AsioTCPSocket *>(getStream());
if (asioSocket) {
  KeyboardEvent evt;
  evt.type = KeyboardEvent::Type::Down;
  evt.key = key;
  evt.mask = mask;
  evt.button = button;
  if (!asioSocket->eventBuffer().pushKeyboardEvent(evt)) {
    LOG_WARN("keyboard FIFO full, falling back to direct send");
    ProtocolUtil::writef(getStream(), kMsgDKeyDown1_0, key, mask);
  }
} else {
  ProtocolUtil::writef(getStream(), kMsgDKeyDown1_0, key, mask);
}
```

### CR-02: Accepted socket runs on wrong io_context -- callbacks execute on acceptor's thread

**File:** `src/lib/net/AsioTCPListenSocket.cpp:40-53`, `src/lib/net/AsioTCPSocket.cpp:40-54`
**Issue:** When `AsioTCPListenSocket::startAsyncAccept` accepts a connection, it creates `AsioTCPSocket(m_events, std::move(socket))`. The accepted `tcp::socket` was created from the listen socket's `m_ioContext`. The `AsioTCPSocket` constructor moves this socket but then creates its own `m_ioContext` and `m_strand` from that *new* io_context. However, the moved-from socket is still associated with the *acceptor's* io_context. The `m_strand` wraps the new (empty) io_context, so `asio::bind_executor(m_strand, ...)` binds to the wrong executor. Asio `async_read_some` and `async_write` will post completions on the acceptor's io_context thread, not on the new socket's io_context thread. The new `m_ioThread` running `m_ioContext.run()` will have nothing to do, while all actual I/O callbacks race on the acceptor's thread without strand protection. This is a fundamental design error in how accepted sockets transfer io_context ownership.

**Fix:** The accepted socket must either (a) use the same io_context as the acceptor (sharing a thread), or (b) the `AsioTCPSocket` constructor must re-associate the socket with its own io_context by closing the old socket and reconnecting, or (c) pass the acceptor's io_context reference to the accepted socket so it uses the same one. Option (a) is simplest and most correct:
```cpp
// In AsioTCPSocket constructor for accepted sockets, the socket
// must be re-associated with the local io_context:
AsioTCPSocket::AsioTCPSocket(IEventQueue *events, asio::ip::tcp::socket socket)
    : IDataSocket(events),
      m_ioContext(),  // new empty io_context
      m_socket(m_ioContext),  // create socket on local io_context
      ...
{
  // Cannot move a socket between io_contexts in Asio.
  // Must extract endpoint and reconnect on local io_context.
  auto ep = socket.remote_endpoint();
  // Or better: redesign to share io_context between listener and accepted sockets.
}
```

### CR-03: releaseAllKeys sends data on disconnected socket, causing recursive error

**File:** `src/lib/net/AsioTCPSocket.cpp:499-508`
**Issue:** `handleDisconnect` is called from `onReadComplete` when a disconnect error occurs. It calls `releaseAllKeys()` which iterates all pressed keys and calls `ProtocolUtil::writef()` on `this` (the socket). But at this point the socket is already disconnected. The `write()` method checks `m_writable` (line 127), but `m_writable` is still `true` at this point because `handleDisconnect` hasn't yet set it to `false` -- that happens at line 490, *after* `releaseAllKeys()` returns at line 482. So each key release will attempt to buffer data for a dead socket. Even after `m_writable` is set to false, `write()` calls `sendEvent(StreamOutputError)` which triggers `handleWriteError -> disconnect`, potentially causing event recursion.

**Fix:**
```cpp
void AsioTCPSocket::handleDisconnect(const std::error_code &ec)
{
  if (ec == asio::error::operation_aborted) {
    return;
  }

  // 1. FIRST mark as disconnected to prevent write attempts
  m_connected.store(false, std::memory_order_release);
  m_writable.store(false, std::memory_order_release);

  // 2. Now release keys -- only clears KeyStateTable, skip network send
  auto releases = m_keyState.releaseAll();
  for (const auto &r : releases) {
    LOG_DEBUG("断连释放按键: key=%d, mask=0x%04x", r.key, r.mask);
    // Do NOT call ProtocolUtil::writef on a dead socket
  }

  // 3. Cancel timers
  m_mouseTimer.cancel();
  m_keyboardPollTimer.cancel();

  // 4. Notify upper layer
  sendEvent(EventTypes::SocketDisconnected);

  // 5. Auto reconnect
  if (m_autoReconnect) {
    scheduleReconnect();
  }
}
```

### CR-04: onWriteComplete accesses m_outputBuffer without mutex, causing data race

**File:** `src/lib/net/AsioTCPSocket.cpp:363-383`
**Issue:** At line 370, `onWriteComplete` checks `m_outputBuffer.getSize()` outside the mutex lock. The lock scope (lines 365-369) only protects the `pop()` call. After the lock is released, `m_outputBuffer.getSize()` at line 370 races with `write()` (line 134) which modifies `m_outputBuffer` under `m_mutex`. The producer thread (EventQueue thread calling `write()`) and consumer thread (io_context thread in `onWriteComplete`) can access `m_outputBuffer` concurrently without synchronization.

**Fix:**
```cpp
void AsioTCPSocket::onWriteComplete(const std::error_code &ec, size_t bytesTransferred)
{
  bool hasMore = false;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outputBuffer.pop(static_cast<uint32_t>(bytesTransferred));
    hasMore = (m_outputBuffer.getSize() > 0);
  }

  if (!hasMore) {
    sendEvent(EventTypes::StreamOutputFlushed);
    {
      std::lock_guard<std::mutex> flushLock(m_flushMutex);
      m_flushed = true;
      m_flushCV.notify_all();
    }
  } else {
    asio::post(m_strand, [this]() { startAsyncWrite(); });
  }
}
```

## Warnings

### WR-01: Input buffer overflow discards data without notifying upper layer

**File:** `src/lib/net/AsioTCPSocket.cpp:317-319`
**Issue:** When the input buffer exceeds 1MB, data is silently discarded by `m_inputBuffer.pop(...)`. This can corrupt protocol messages because partial message data is removed from the middle. The upper layer (ClientProxy1_0::handleData) reads fixed-size message codes and will read garbage after a truncation event, likely leading to disconnection or undefined protocol behavior.

**Fix:** Instead of silently truncating, disconnect the client or close the socket, and log at ERROR level:
```cpp
if (m_inputBuffer.getSize() > s_maxInputBufferSize) {
  LOG_ERR("输入缓冲区超过 1MB 限制，关闭连接");
  close();
  return;
}
```

### WR-02: ClientApp::getSocketFactory returns raw pointer, caller expects ownership via raw new

**File:** `src/lib/deskflow/ClientApp.cpp:385-389`
**Issue:** `getSocketFactory()` returns `new AsioTCPSocketFactory(...)` as a raw pointer, while `ServerApp::getSocketFactory()` (line 482) returns `std::make_unique<AsioTCPSocketFactory>(...)`. The inconsistency means `ClientApp`'s factory could leak if the caller doesn't wrap it in a unique_ptr. The `ISocketFactory` return type in ClientApp is `ISocketFactory*` (raw) vs ServerApp's `std::unique_ptr<ISocketFactory>`. If the base class interface changed to `unique_ptr`, ClientApp leaks; if it stayed raw, ServerApp is correct.

**Fix:** Make ClientApp consistent with ServerApp:
```cpp
std::unique_ptr<ISocketFactory> ClientApp::getSocketFactory() const
{
  return std::make_unique<AsioTCPSocketFactory>(getEvents(), true);
}
```

### WR-03: Auto-reconnect has no maximum retry count, can retry forever

**File:** `src/lib/net/AsioTCPSocket.cpp:510-532`
**Issue:** `scheduleReconnect` uses exponential backoff capped at 30 seconds, but never gives up. If the server is permanently gone, the client will retry every 30 seconds indefinitely. This is compounded by the fact that ClientApp also has its own retry logic (retryTime() in ClientApp.cpp:391), so there are two independent retry loops that can interact unpredictably.

**Fix:** Add a maximum retry count (e.g., 10 retries) and after that emit a permanent failure event:
```cpp
// Add member: int m_reconnectAttempts{0};
// In scheduleReconnect:
if (m_reconnectAttempts >= 10) {
  LOG_WARN("最大重连次数已达，停止重连");
  sendEvent(EventTypes::DataSocketConnectionFailed);
  return;
}
m_reconnectAttempts++;
// ... existing backoff logic ...
```
Also, in `onReconnectSuccess`, reset `m_reconnectAttempts = 0`.

### WR-04: KeyboardEvent contains std::string but is used in lock-free SPSC queue

**File:** `src/lib/net/InputEventBuffer.h:29`, `src/lib/net/SpscQueue.h:66-67`
**Issue:** `KeyboardEvent` has a `std::string language` member. The SPSC queue stores `KeyboardEvent` objects in a `std::array<T, Capacity>`. Assignment of `std::string` at line 67 (`buffer_[tail] = item;`) is NOT atomic and involves dynamic memory allocation/deallocation. If the producer and consumer happen to access adjacent slots where one is being constructed and another destroyed, and the string implementation uses SSO (small string optimization) with different internal representations, this can corrupt the string's internal state. While SPSC queue design ensures producer and consumer never access the *same* slot simultaneously, the `std::string` copy assignment involves both reading the old value (destruction) and writing the new value, which is safe for distinct slots. However, the SPSC design relies on trivial copyability for correctness guarantees -- `std::string` violates this assumption.

**Fix:** Replace `std::string language` with a fixed-size char array to ensure trivial copyability:
```cpp
struct KeyboardEvent
{
  // ...
  char language[16]{}; // Fixed-size language code, no heap allocation
};
```

### WR-05: m_atomic members written without atomic operations

**File:** `src/lib/net/AsioTCPSocket.cpp:269`
**Issue:** `m_writable = true;` at line 269 uses plain assignment on `std::atomic<bool>`. While this is technically well-defined in C++ (atomic types support implicit conversion), the code is inconsistent -- elsewhere `m_writable.store(false, std::memory_order_release)` is used with explicit ordering. The implicit conversion uses `memory_order_seq_cst` while the explicit stores use `memory_order_release`, creating an inconsistency in the memory ordering contract.

**Fix:**
```cpp
m_writable.store(true, std::memory_order_release);
```

### WR-06: AsioTCPSocketFactory creates raw pointers, violating RAII patterns

**File:** `src/lib/net/AsioTCPSocketFactory.cpp:30-32`, `src/lib/net/AsioTCPSocketFactory.cpp:40`
**Issue:** Both `create()` and `createListen()` return raw `new` pointers. The caller must remember to `delete` these. If any exception occurs between factory creation and ownership transfer, the socket leaks. This is inconsistent with modern C++ best practices and with the ServerApp's use of `unique_ptr`.

**Fix:**
```cpp
IDataSocket *AsioTCPSocketFactory::create(...) const
{
  auto socket = std::make_unique<AsioTCPSocket>(m_events);
  socket->setAutoReconnect(m_autoReconnect);
  return socket.release(); // If interface requires raw pointer
}
```
Or better, change the `ISocketFactory` interface to return `unique_ptr`.

### WR-07: Connect method has shared_from_this() requirement but no protection against misuse

**File:** `src/lib/net/AsioTCPSocket.cpp:218`
**Issue:** `connect()` calls `shared_from_this()`, which requires the object to be owned by a `shared_ptr` at the time of the call. However, `AsioTCPSocketFactory::create()` returns a raw pointer (line 30), and the caller (Client/Server) may store it as a raw pointer or `unique_ptr`. If `connect()` is called on an object not managed by `shared_ptr`, `shared_from_this()` will throw `std::bad_weak_ptr`, crashing the application. This is an implicit lifetime requirement that is not documented or enforced.

**Fix:** Either document that AsioTCPSocket must always be held in a shared_ptr, or refactor to avoid shared_from_this() by using a different lifetime management pattern (e.g., a flag `m_destroying` checked in callbacks, or cancel all async operations in destructor before joining thread).

### WR-08: m_connected.exchange in close() sends SocketDisconnected even if never connected

**File:** `src/lib/net/AsioTCPSocket.cpp:86-88`
**Issue:** `close()` uses `m_connected.exchange(false)` and sends `SocketDisconnected` if it was previously true. But during `handleDisconnect` (line 489), `m_connected` is also set to false and `SocketDisconnected` is sent. If `handleDisconnect` triggers and then the destructor calls `close()`, the event is sent twice. Additionally, if `close()` is called on a socket that was never connected (e.g., after a failed DNS resolution), `m_connected` is already false so no event fires -- this is correct, but the double-send path remains.

**Fix:** `close()` should check whether disconnect has already been signaled:
```cpp
void AsioTCPSocket::close()
{
  if (m_connected.exchange(false)) {
    sendEvent(EventTypes::SocketDisconnected);
  }
  // ... rest of cleanup
}
```
This is already the case, but `handleDisconnect` also sends `SocketDisconnected` directly (line 491). Remove the direct send in `handleDisconnect` and rely on `close()` being called, or add a separate `m_disconnectNotified` flag to prevent double-sending.

## Info

### IN-01: Unused member variable m_accepting in AsioTCPListenSocket

**File:** `src/lib/net/AsioTCPListenSocket.h:56`
**Issue:** `std::atomic<bool> m_accepting{false}` is declared but never read or written anywhere in the implementation. This is dead code that adds confusion.

**Fix:** Remove the unused member variable.

### IN-02: LOG_WARN format string typo ("no-op from" missing argument)

**File:** `src/lib/server/ClientProxy1_0.cpp:154`
**Issue:** `LOG_DEBUG2("no-op from", getName().c_str());` -- the format string `"no-op from"` has no format specifier for the `getName()` argument. The argument is passed but never used in the output. Same issue at line 179.

**Fix:**
```cpp
LOG_DEBUG2("no-op from \"%s\"", getName().c_str());
```

### IN-03: SocketDisconnected event handled inconsistently -- ClientProxy1_0 doesn't handle it

**File:** `src/lib/server/ClientProxy1_0.cpp:30-43`
**Issue:** The ClientProxy1_0 constructor registers handlers for `StreamInputShutdown` and `StreamInputFormatError` calling `handleDisconnect()`, but does not register a handler for `SocketDisconnected` which is the new event sent by `AsioTCPSocket::handleDisconnect`. If the Asio socket disconnects, the `SocketDisconnected` event fires but ClientProxy1_0 won't handle it -- only `StreamInputShutdown` triggers cleanup. The Asio socket sends `SocketDisconnected` on error, not `StreamInputShutdown`, so the client proxy may never clean up properly.

**Fix:** Add a handler for `SocketDisconnected` in the ClientProxy1_0 constructor:
```cpp
m_events->addHandler(EventTypes::SocketDisconnected, stream->getEventTarget(), [this](const auto &) {
  handleDisconnect();
});
```

---

_Reviewed: 2026-05-12_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
