# Phase 1: Network Pipeline Rewrite - Research

**Researched:** 2026-05-12
**Domain:** C++ 网络异步 I/O (Standalone Asio) + Lock-free SPSC 缓冲区
**Confidence:** HIGH

## Summary

Deskflow 当前的网络层使用自定义的 `SocketMultiplexer`（317 行手动 new/delete、reinterpret_cast cursor-mark sentinel、自定义两阶段 CondVar 锁协议）通过 `ARCH->pollSocket()` 阻塞式轮询所有连接。该多路复用器在单线程中处理所有 socket I/O（包括 TLS 握手），在高延迟或抖动时会阻塞鼠标/键盘事件流，导致输入卡顿。

本阶段将其替换为基于 Standalone Asio 的异步管道架构。核心策略：(1) 每连接独立 `io_context` 线程处理 TCP 收发，(2) lock-free SPSC 缓冲区分离键盘（FIFO）和鼠标（atomic 最新位置槽）事件流，(3) Asio 完成回调桥接到现有 `EventQueue`，上层 Server/Client 逻辑零修改。

**Primary recommendation:** 使用 Standalone Asio 1.38.0（header-only，Boost Software License，商业友好）通过 CMake FetchContent 引入。替换 `SocketMultiplexer`+`ISocketMultiplexerJob`+`TSocketMultiplexerMethodJob` 整套机制，保留 `IDataSocket`/`IStream` 接口不变，在 `TCPSocket` 内部重写为 Asio 异步操作。SPSC 缓冲区手写实现（约 50 行，alignas(64) 防 false sharing），不引入第三方 lock-free 库。

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** 使用 Standalone Asio (header-only) 作为网络 I/O 库，替代现有 SocketMultiplexer 的阻塞 I/O
- **D-02:** Asio 仅负责 socket I/O 和定时器，运行在独立线程；不替代现有 EventQueue
- **D-03:** 使用 Asio steady_timer 处理 poll 超时和连接超时，替代 Arch::sleep()
- **D-04:** 每连接单线程模型 -- 每个连接拥有独立的 Asio io_context 线程处理发送和接收
- **D-05:** Lock-free SPSC 缓冲区采用分离队列组织：键盘事件走 FIFO 队列（保证不丢失），鼠标移动走 atomic 最新位置槽（覆盖旧值，只发最新位置）
- **D-06:** Asio I/O 完成后通过桥接层投递事件到 EventQueue，上层 Server/Client 逻辑不变
- **D-07:** 完全兼容现有 ProtocolUtil::writef 二进制协议帧格式，不做任何协议修改
- **D-08:** 鼠标移动合并仅在 SPSC 缓冲区层实现（写入端覆盖旧位置，读取端取最新值后发 DMMV），协议层无感知
- **D-09:** 依赖 Asio 异步读写错误返回检测断连，辅以 Asio steady_timer 超时
- **D-10:** 按键释放通过发送端状态表实现 -- 维护所有"按下"状态的键，断连时遍历发送释放事件
- **D-11:** 断连后自动重连，使用指数退避策略（立即重试一次，之后 1s->2s->4s->最大 30s），重连成功后自动恢复输入流

### Claude's Discretion
- SPSC 队列的具体容量大小
- Asio 引入方式（FetchContent vs 系统安装）
- 桥接层的具体实现细节

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| NET-01 | 鼠标移动在网络波动时保持流畅，不出现持续卡顿或掉帧 | D-05 atomic 最新位置槽 + D-04 独立 Asio 线程，I/O 线程不阻塞输入采集 |
| NET-02 | 键盘事件可靠送达，不丢失按键事件 | D-05 键盘 FIFO 队列（SPSC，保证顺序和完整），D-10 断连状态表保障 |
| NET-03 | 网络断开时自动释放所有按键（防卡键） | D-10 发送端状态表追踪所有 pressed keys，断连时遍历发送 keyUp |
| NET-04 | 鼠标移动事件合并发送（1000Hz 输入 -> 200Hz 发送） | D-08 SPSC 缓冲区写入端覆盖，读取端以 200Hz steady_timer 定时取最新值 |
| NET-05 | 输入处理线程不被网络 I/O 阻塞（解耦架构） | D-04 独立 io_context 线程 + D-05 SPSC 缓冲区实现生产者-消费者解耦 |
| NET-06 | 端到端延迟降至 15ms 以下（当前约 45ms） | D-04 per-connection 消除全局轮询瓶颈 + SPSC 缓冲区 zero-copy + Asio async 减少 poll 开销 |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| TCP socket I/O (async read/write) | Asio io_context 线程 | -- | Asio 异步操作独占 socket 生命周期，跨线程访问需 post |
| 键盘/鼠标事件缓冲 | SPSC 缓冲区 (lock-free) | -- | 生产者(EventQueue 线程) -> SPSC -> 消费者(Asio 线程)，零锁竞争 |
| 事件调度和上层逻辑 | EventQueue (主线程) | -- | 现有架构，Asio 桥接层通过 addEvent 投递，不修改 |
| 协议序列化/反序列化 | ProtocolUtil (上层调用) | -- | D-07 锁定：writef/readf 格式不可变，SPSC 层不感知协议 |
| 断连检测与按键释放 | Asio 线程 (错误回调) | EventQueue (通知上层) | D-09 Asio 错误检测 + D-10 状态表在 Asio 线程遍历发送 keyUp |
| 自动重连 | Asio 线程 (steady_timer 退避) | EventQueue (通知上层) | D-11 指数退避在 Asio 线程中用 steady_timer 实现 |

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Standalone Asio | 1.38.0 (2025-12-30) | 异步 TCP socket I/O + 定时器 | D-01 锁定选择；header-only；Boost Software License 商业友好；C++11 即可使用，项目已用 C++20 [VERIFIED: GitHub release] |
| std::atomic (C++20) | 语言标准 | SPSC 缓冲区 head/tail 索引 | memory_order_acquire/release 实现无锁队列；MSVC/GCC/Clang 均有良好支持 [CITED: cppreference.com] |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| CMake FetchContent | 3.24+ (项目最低要求) | 下载 Asio 源码 | 构建时自动获取，无需系统安装 [VERIFIED: CMakeLists.txt cmake_minimum_required] |
| pthread / ws2_32 | 系统库 | Asio 底层依赖 | Asio 自动链接；Windows 下需 ws2_32（已在 net CMakeLists.txt 中链接） |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Standalone Asio | Boost.Asio | Boost.Asio 需引入整个 Boost 依赖，Standlone 无此负担；API 100% 兼容 |
| FetchContent | 系统安装 + find_package | 系统安装需每个开发环境手动配置，FetchContent 自动化更可靠 [ASSUMED] |
| 手写 SPSC | boost::lockfree::spsc_queue / atomic_queue | 引入额外依赖；手写 SPSC 约 50 行代码足够，减少依赖数量 [ASSUMED] |

**Installation:**

```cmake
# cmake/FetchAsio.cmake (新增文件)
include(FetchContent)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-38-0
)

# Standalone Asio 不含 CMakeLists.txt，需要手动创建 interface library
FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
  FetchContent_Populate(asio)
  add_library(asio INTERFACE)
  target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
  # Windows 需要 winsock
  if(WIN32)
    target_link_libraries(asio INTERFACE ws2_32)
  endif()
endif()
```

**Version verification:**
- Standalone Asio: 1.38.0 -- 最新稳定版，发布于 2025-12-30 [VERIFIED: GitHub releases]
- CMake: 3.24+ -- 项目已设定 `cmake_minimum_required(VERSION 3.24)` [VERIFIED: CMakeLists.txt line 6]
- C++20 标准 -- 项目已设定 `CMAKE_CXX_STANDARD 20` [VERIFIED: CMakeLists.txt line 20]

## Architecture Patterns

### System Architecture Diagram

```
[平台输入采集]                    [平台输入采集]
  1000Hz 鼠标                      按键事件
      |                               |
      v                               v
+-------------------------------+  +-------------------------------+
| EventQueue (主线程)           |  | EventQueue (主线程)           |
| - Server::handleMotionPrimary |  | - Server::handleKeyDownEvent  |
| - 调用 active->mouseMove()    |  | - 调用 active->keyDown()      |
+---------------+---------------+  +---------------+---------------+
                |                                  |
                v                                  v
+---------------+---------------+  +---------------+---------------+
| SPSC: 鼠标 atomic 位置槽      |  | SPSC: 键盘 FIFO 队列          |
| - store(x,y) memory_order_rel |  | - push(event) memory_order_rel|
| - 覆盖旧值，只保留最新位置      |  | - 保证顺序和完整性            |
+---------------+---------------+  +---------------+---------------+
                |                                  |
                +----------------+-----------------+
                                 |
                                 v
                +-------------------------------+
                | Asio io_context 线程 (per-connection) |
                | - 200Hz steady_timer 读鼠标最新位置   |
                | - 键盘 FIFO 非空时立即读取            |
                | - ProtocolUtil::writef 序列化         |
                | - async_write 发送                    |
                | - async_read 接收 -> 解析协议          |
                +---------------+----------------------+
                                |
                        TCP socket (Asio)
                                |
                +---------------+----------------------+
                | Asio 完成回调                        |
                | - 读取完成 -> EventQueue::addEvent    |
                |   (StreamInputReady 等)               |
                | - 错误 -> 断连检测                     |
                |   -> 遍历按键状态表发送 keyUp          |
                |   -> 指数退避重连                      |
                +---------------------------------------+
```

### Recommended Project Structure

```
src/lib/net/
├── AsioTCPSocket.h          # 新：Asio 版 TCP socket（实现 IDataSocket）
├── AsioTCPSocket.cpp        # 新：async read/write、连接、断连逻辑
├── AsioTCPListenSocket.h    # 新：Asio 版 listen socket（实现 IListenSocket）
├── AsioTCPListenSocket.cpp  # 新：async accept
├── AsioTCPSocketFactory.h   # 新：工厂（替代 TCPSocketFactory）
├── AsioTCPSocketFactory.cpp # 新：创建 AsioTCPSocket 实例
├── SpscQueue.h              # 新：lock-free SPSC 缓冲区模板
├── InputEventBuffer.h       # 新：组合键盘FIFO + 鼠标位置槽的缓冲区
├── InputEventBuffer.cpp     # 新：缓冲区实现
├── AsioEventBridge.h        # 新：Asio -> EventQueue 桥接层
├── AsioEventBridge.cpp      # 新：桥接实现
├── KeyStateTable.h          # 新：发送端按键状态追踪表
├── KeyStateTable.cpp        # 新：keyDown 记录、断连时遍历发送 keyUp
├── SocketMultiplexer.h      # 保留但标记 deprecated，Phase 2 后移除
├── SocketMultiplexer.cpp    # 保留但标记 deprecated
├── TCPSocket.h              # 保留（TLS SecureSocket 继承链需要），后续 Phase 移除
├── TCPSocket.cpp            # 保留
├── IDataSocket.h            # 不变
├── ISocket.h                # 不变
├── IListenSocket.h          # 不变
├── ISocketFactory.h         # 不变
├── ProtocolUtil.h           # 不变（D-07 协议兼容）
└── ... (其余文件不变)

src/lib/deskflow/
├── App.h                    # 修改：m_socketMultiplexer 改为可选，新增 Asio 初始化
├── ServerApp.cpp            # 修改：创建 AsioTCPSocketFactory 替代 TCPSocketFactory
└── ClientApp.cpp            # 修改：同上
```

### Pattern 1: Asio Per-Connection I/O Thread

**What:** 每个网络连接创建独立的 `io_context`，在专属线程中运行 `io_context::run()`。
**When to use:** D-04 锁定策略 -- 每连接单线程模型。

```cpp
// 来源: Asio 官方示例 + 项目需求 D-04
class AsioTCPSocket : public IDataSocket
{
public:
  AsioTCPSocket(IEventQueue *events)
    : m_ioContext()
    , m_socket(m_ioContext)
    , m_strand(m_ioContext.get_executor())
    , m_events(events)
  {
    // 在独立线程运行 io_context
    m_ioThread = std::thread([this]() {
      asio::io_context::work work(m_ioContext);
      m_ioContext.run();
    });
  }

  ~AsioTCPSocket() override
  {
    m_ioContext.stop();
    if (m_ioThread.joinable()) {
      m_ioThread.join();
    }
  }

private:
  asio::io_context m_ioContext;
  asio::ip::tcp::socket m_socket;
  asio::strand<asio::io_context::executor_type> m_strand;
  std::thread m_ioThread;
  IEventQueue *m_events;
};
```

### Pattern 2: Lock-Free SPSC Mouse Position Slot

**What:** 原子操作存储/读取最新鼠标位置，覆盖旧值。
**When to use:** 鼠标移动事件合并（D-05/D-08）。

```cpp
// 来源: 基于 C++20 std::atomic 标准模式
struct alignas(64) MousePositionSlot
{
  std::atomic<int32_t> x{0};
  std::atomic<int32_t> y{0};
  std::atomic<bool> updated{false};

  void store(int32_t newX, int32_t newY) noexcept
  {
    x.store(newX, std::memory_order_relaxed);
    y.store(newY, std::memory_order_relaxed);
    updated.store(true, std::memory_order_release);
  }

  bool load(int32_t &outX, int32_t &outY) noexcept
  {
    if (!updated.exchange(false, std::memory_order_acquire)) {
      return false;
    }
    outX = x.load(std::memory_order_relaxed);
    outY = y.load(std::memory_order_relaxed);
    return true;
  }
};
```

### Pattern 3: Lock-Free SPSC Keyboard FIFO

**What:** 单生产者单消费者环形缓冲区，保证键盘事件不丢失不乱序。
**When to use:** 键盘事件缓冲（D-05）。

```cpp
// 来源: 标准SPSC ring buffer 模式
template <typename T, size_t Capacity>
class alignas(64) SpscFifo
{
  static constexpr size_t kMask = Capacity - 1;
  static_assert((Capacity & kMask) == 0, "Capacity must be power of 2");

  std::array<T, Capacity> buffer_{};
  alignas(64) std::atomic<size_t> head_{0};
  alignas(64) std::atomic<size_t> tail_{0};

public:
  bool push(const T &item) noexcept
  {
    size_t tail = tail_.load(std::memory_order_relaxed);
    size_t next = (tail + 1) & kMask;
    if (next == head_.load(std::memory_order_acquire)) {
      return false; // 满了
    }
    buffer_[tail] = item;
    tail_.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T &item) noexcept
  {
    size_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_acquire)) {
      return false; // 空了
    }
    item = buffer_[head];
    head_.store((head + 1) & kMask, std::memory_order_release);
    return true;
  }
};
```

### Pattern 4: Asio -> EventQueue Bridge

**What:** Asio 完成回调中投递事件到 EventQueue。
**When to use:** D-06 -- 所有 Asio 回调需要通知上层时。

```cpp
// 来源: 分析 EventQueue::addEvent API + D-06 决策
void AsioTCPSocket::onReadComplete(const std::error_code &ec, size_t bytesTransferred)
{
  if (ec) {
    // 断连处理（D-09）
    m_events->addEvent(Event(EventTypes::SocketDisconnected, getEventTarget()));
    return;
  }

  // 将数据写入输入缓冲区
  m_inputBuffer.write(m_readBuffer.data(), static_cast<uint32_t>(bytesTransferred));

  // 通知上层有数据可读（D-06 桥接）
  m_events->addEvent(Event(EventTypes::StreamInputReady, getEventTarget()));

  // 继续异步读取
  startAsyncRead();
}
```

### Pattern 5: Key Release State Table on Disconnect

**What:** 发送端维护所有"按下"状态的键，断连时遍历发送释放事件。
**When to use:** D-10 -- 断连时防卡键。

```cpp
// 来源: D-10 决策
class KeyStateTable
{
public:
  void recordKeyDown(KeyID key, KeyModifierMask mask, KeyButton button)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pressedKeys[key] = {mask, button};
  }

  void recordKeyUp(KeyID key)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pressedKeys.erase(key);
  }

  // 断连时调用，遍历所有 pressed keys 发送 keyUp
  std::vector<KeyReleaseInfo> releaseAll()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<KeyReleaseInfo> releases;
    for (auto &[key, info] : m_pressedKeys) {
      releases.push_back({key, info.mask, info.button});
    }
    m_pressedKeys.clear();
    return releases;
  }

private:
  struct KeyInfo { KeyModifierMask mask; KeyButton button; };
  std::mutex m_mutex; // 保护：EventQueue 线程写，Asio 线程读（断连时）
  std::unordered_map<KeyID, KeyInfo> m_pressedKeys;
};
```

### Anti-Patterns to Avoid

- **在 Asio 回调中直接调用上层代码:** Asio 回调在 io_context 线程执行，直接操作 Server/Client 对象会导致线程安全问题。必须通过 EventQueue::addEvent 桥接。
- **共享 io_context 给所有连接:** 虽然可行，但违反 D-04 per-connection 单线程决策，且一个连接的阻塞操作会影响其他连接。
- **在 SPSC 中使用 mutex:** SPSC 的核心价值是无锁。如果需要 mutex，说明设计错误。
- **将 ArchSocket (原生 socket) 和 asio::ip::tcp::socket 混用:** Asio 拥有 socket 生命周期，不能在 Asio socket 上使用 ARCH->readSocket/writeSocket。
- **忽略 strand:** 虽然每连接单线程，但 startAsyncRead + steady_timer 回调可能交叉。使用 strand 保证序列化。

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| 异步 TCP 连接/读写 | 自定义 epoll/kqueue 封装 | asio::async_connect / async_read_some / async_write | 跨平台（macOS kqueue / Windows IOCP）+ 异常安全 + 经验证的边缘情况处理 |
| 定时器和超时 | sleep + 条件变量组合 | asio::steady_timer + async_wait | 与 io_context 集成，取消语义正确，无需手动管理线程唤醒 |
| DNS 解析 | getaddrinfo 封装 | asio::ip::tcp::resolver::async_resolve | 跨平台异步 DNS，不阻塞 io_context 线程 |
| 网络字节序转换 | 手写 htons/ntohs | asio 内置（读写字节序已由 ProtocolUtil 处理） | ProtocolUtil 已使用网络字节序，Asio 不需要额外转换 |

**Key insight:** 网络异步 I/O 层的错误率极高（平台差异、边缘情况、竞态条件），Asio 已经过数十亿次生产验证。唯一手写的是 SPSC 缓冲区，因为它的需求非常简单和确定（固定大小、单类型、无动态分配），远低于通用队列的复杂度。

## Common Pitfalls

### Pitfall 1: Asio Socket 生命周期与回调竞态

**What goes wrong:** socket 对象在异步操作完成前被销毁，导致 use-after-free。
**Why it happens:** `async_read_some` 的回调持有 `this` 指针，但析构函数可能先执行。
**How to avoid:** 使用 `std::shared_ptr<AsioTCPSocket>` + `shared_from_this()` 模式。或者在析构时先 `close()` socket（取消所有挂起异步操作），再 `io_context.stop()`，最后 `thread.join()`。
**Warning signs:** 间歇性崩溃（特别是在快速断连/重连时），ASan 报告 heap-use-after-free。

### Pitfall 2: SPSC 队列的 False Sharing

**What goes wrong:** 生产者 head 和消费者 tail 在同一缓存行，导致多核间缓存一致性协议频繁触发，性能骤降。
**Why it happens:** 两个 atomic 变量紧邻声明，编译器将它们放在同一 64 字节缓存行。
**How to avoid:** 使用 `alignas(64)` 将 head 和 tail 分别对齐到独立缓存行。鼠标位置槽同样需要此处理。
**Warning signs:** 多核系统上 SPSC 吞吐量低于预期，perf 报告高 cache-miss 率。

### Pitfall 3: io_context::run() 在无操作时立即返回

**What goes wrong:** `io_context::run()` 在没有挂起的异步操作时立即返回，导致线程退出。
**Why it happens:** Asio 的 `run()` 仅在有工作（pending async operations）时阻塞。
**How to avoid:** 使用 `asio::executor_work_guard<asio::io_context::executor_type>`（或旧版 `io_context::work`）保持 io_context 持续运行，直到显式 `stop()`。
**Warning signs:** Asio 线程启动后立即退出，socket 操作失败。

### Pitfall 4: 跨线程调用 IDataSocket::write()

**What goes wrong:** 上层（EventQueue 线程）调用 `IStream::write()` 写数据到 socket，但 socket 内部缓冲区和 Asio async_write 只能从 io_context 线程发起。
**Why it happens:** `ProtocolUtil::writef(getStream(), ...)` 在 Server 线程调用，而 async_write 必须在 io_context 线程。
**How to avoid:** `write()` 方法将数据写入线程安全的输出缓冲区（或使用 asio::post 将实际发送操作调度到 io_context 线程）。
**Warning signs:** 偶发数据损坏、断连、或 ASan 报告数据竞争。

### Pitfall 5: 鼠标合并频率 vs 发送时机

**What goes wrong:** 200Hz steady_timer 到期时读取到"旧"位置（已有一帧新的更新但未可见），或 timer 和写入端的 memory ordering 不正确。
**Why it happens:** release/acquire 语义不匹配 -- 写入端用 relaxed store 但读取端用 acquire load。
**How to avoid:** 写入端：先 store x/y（relaxed），再 store updated flag（release）。读取端：先 exchange updated flag（acquire），再 load x/y（relaxed）。
**Warning signs:** 鼠标位置偶尔"跳回"旧坐标。

### Pitfall 6: SPSC 队列容量不足导致键盘事件丢弃

**What goes wrong:** 快速打字时 FIFO 队列满了，push() 返回 false，键盘事件被丢弃。
**Why it happens:** 网络发送速度跟不上输入速度（特别是高延迟网络）。
**How to avoid:** 设置合理容量（建议 256 个事件），并在队列满时记录警告日志。考虑实现 back-pressure（让上层等待），但不丢弃事件。关键点：键盘事件绝对不能丢弃（D-05），如果队列满应阻塞或降速。
**Warning signs:** 键盘输入偶尔丢失字符。

### Pitfall 7: Windows MSVC 对 alignas(64) 的支持

**What goes wrong:** MSVC 对 `alignas` 在动态分配内存上的支持有限（需要在堆上也对齐）。
**Why it happens:** `alignas` 仅影响栈分配和静态存储，`new` 分配的对象不保证对齐（除非使用 aligned new）。
**How to avoid:** 对于 SPSC 缓冲区，使用 `std::aligned_alloc` 或 C++17 `operator new(size_t, align_val_t)`。更简单的方案：将 SPSC 作为 `std::shared_ptr` 在栈上或使用 `unique_ptr` + placement new。实际上由于 SPSC 通常是成员变量（栈上或包含在对象内），`alignas` 对成员变量有效。
**Warning signs:** 性能不达预期，VTune/Perf 显示 false sharing。

## Code Examples

### Asio 异步读取循环（接收端）

```cpp
// 来源: Asio 官方 echo server 示例 + D-06 桥接需求
void AsioTCPSocket::startAsyncRead()
{
  auto self = shared_from_this();
  m_socket.async_read_some(
    asio::buffer(m_readBuffer),
    asio::bind_executor(m_strand,
      [this, self](const std::error_code &ec, size_t bytesTransferred) {
        if (ec) {
          if (ec == asio::error::eof || ec == asio::error::connection_reset) {
            // 远端断连（D-09）
            m_events->addEvent(Event(EventTypes::SocketDisconnected, getEventTarget()));
          }
          return;
        }
        // 数据写入输入缓冲区
        m_inputBuffer.write(m_readBuffer.data(), static_cast<uint32_t>(bytesTransferred));
        // 桥接到 EventQueue（D-06）
        m_events->addEvent(Event(EventTypes::StreamInputReady, getEventTarget()));
        // 继续读取
        startAsyncRead();
      }
    )
  );
}
```

### Asio 异步连接（客户端）

```cpp
// 来源: Asio 官方示例 + D-11 重连策略
void AsioTCPSocket::connect(const NetworkAddress &addr)
{
  auto self = shared_from_this();
  asio::ip::tcp::resolver resolver(m_ioContext);
  auto endpoints = resolver.resolve(addr.getHostname(), std::to_string(addr.getPort()));

  asio::async_connect(m_socket, endpoints,
    asio::bind_executor(m_strand,
      [this, self](const std::error_code &ec, const asio::ip::tcp::endpoint &) {
        if (ec) {
          // 连接失败
          m_events->addEvent(Event(EventTypes::DataSocketConnectionFailed, getEventTarget()));
          return;
        }
        // 连接成功
        m_connected = true;
        m_socket.set_option(asio::ip::tcp::no_delay(true));  // 禁用 Nagle
        m_socket.set_option(asio::socket_base::keep_alive(true));
        m_events->addEvent(Event(EventTypes::DataSocketConnected, getEventTarget()));
        startAsyncRead();
      }
    )
  );
}
```

### Asio Async Accept（服务端监听）

```cpp
// 来源: Asio 官方 echo server 示例
class AsioTCPListenSocket : public IListenSocket
{
public:
  void startAccept()
  {
    m_acceptor.async_accept(
      [this](const std::error_code &ec, asio::ip::tcp::socket socket) {
        if (!ec) {
          // 创建新的 AsioTCPSocket，将 accepted socket 转移给它
          // 通知上层有新连接
          m_events->addEvent(Event(EventTypes::ListenSocketConnecting, getEventTarget()));
        }
        // 继续接受下一个连接
        startAccept();
      }
    );
  }

private:
  asio::io_context m_ioContext;
  asio::ip::tcp::acceptor m_acceptor;
};
```

### 200Hz 鼠标位置读取定时器

```cpp
// 来源: D-04 + D-08 -- 独立 Asio 线程中 200Hz 定时取最新鼠标位置并发送
void AsioTCPSocket::startMouseSendTimer()
{
  m_mouseTimer.expires_after(std::chrono::milliseconds(5)); // 200Hz
  m_mouseTimer.async_wait(
    asio::bind_executor(m_strand,
      [this](const std::error_code &ec) {
        if (ec) return; // timer 被取消（正常关闭）
        int32_t x, y;
        if (m_mouseSlot.load(x, y)) {
          // 有新的鼠标位置，通过 ProtocolUtil 发送
          ProtocolUtil::writef(this, kMsgDMouseMove, x, y);
        }
        startMouseSendTimer(); // 重新调度
      }
    )
  );
}
```

### 指数退避重连

```cpp
// 来源: D-11 -- 断连后自动重连
void AsioTCPSocket::scheduleReconnect()
{
  if (m_reconnectDelay == std::chrono::seconds(0)) {
    // 首次立即重试（D-11）
    doReconnect();
    return;
  }

  m_reconnectTimer.expires_after(m_reconnectDelay);
  m_reconnectTimer.async_wait(
    asio::bind_executor(m_strand,
      [this](const std::error_code &ec) {
        if (ec) return; // 取消
        doReconnect();
      }
    )
  );

  // 指数退避：1s -> 2s -> 4s -> ... -> 30s max
  m_reconnectDelay = std::min(m_reconnectDelay * 2, std::chrono::seconds(30));
}

void AsioTCPSocket::doReconnect()
{
  // 重置延迟：首次失败后 1s
  m_reconnectDelay = std::chrono::seconds(1);
  connect(m_targetAddress); // 触发 async_connect
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SocketMultiplexer 全局单线程 poll | Per-connection Asio io_context 线程 | 本阶段 | 消除全局 I/O 瓶颈，连接间互不阻塞 |
| CondVar + Mutex 两阶段锁 | Lock-free SPSC (atomic ring buffer) | 本阶段 | 消除输入路径上的锁竞争 |
| reinterpret_cast cursor-mark sentinel | 独立 Asio 异步回调链 | 本阶段 | 消除手动内存管理和未定义行为 |
| Arch::sleep() 轮询 | asio::steady_timer | 本阶段 | 精确超时，不浪费 CPU 周期 |
| 无鼠标合并（每帧都发） | 200Hz 合并发送 | 本阶段 | 5x 网络带宽节省，鼠标仍跟手 |
| 无按键释放保障 | 发送端状态表 + 断连遍历 | 本阶段 | 消除卡键问题 |

**Deprecated/outdated:**
- `SocketMultiplexer` 整个类：将标记 deprecated，在所有调用者迁移后移除
- `ISocketMultiplexerJob` 接口：Asio 回调链替代
- `TSocketMultiplexerMethodJob<T>` 模板：Asio lambda 回调替代
- `ARCH->pollSocket()` 调用：Asio 内部使用平台原生异步 I/O（kqueue/IOCP）

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | FetchContent 是 Asio 引入的最佳方式（Claude's Discretion 范围） | Standard Stack | 如果有更好的集成方式可能需要调整构建配置 |
| A2 | 手写 SPSC 约 50 行足够，无需引入第三方库 | Don't Hand-Roll | 如果边缘情况（如 MSVC 内存对齐）比预期复杂，可能需要更多工作 |
| A3 | TCPListenSocket 也需要 Asio 化（当前使用 ARCH->acceptSocket + SocketMultiplexer） | Architecture | listen socket 是独立组件，可能影响 ClientListener 集成 |
| A4 | SecureSocket（TLS 层）在 Phase 2 移除，Phase 1 不需要适配 | Architecture | 如果 SecureSocket 继承链与新的 AsioTCPSocket 冲突，需要临时适配 |

## Open Questions

1. **TCPListenSocket 的 Asio 化范围**
   - What we know: `TCPListenSocket` 依赖 `SocketMultiplexer` 注册监听 job。`ClientListener` 通过 `TCPListenSocket::accept()` 创建新 socket。
   - What's unclear: Asio 的 `async_accept` 需要在 accept 时创建 `asio::ip::tcp::socket`（需要 io_context），但当前 `accept()` 返回 `std::unique_ptr<IDataSocket>`。Asio 的 acceptor 和新 socket 必须共享同一个 io_context。
   - Recommendation: `AsioTCPListenSocket` 拥有自己的 `io_context`（用于 acceptor），accept 后创建 `AsioTCPSocket`（拥有独立的 io_context）。

2. **SecureSocket 继承链在 Phase 1 的处理**
   - What we know: `SecureSocket` 继承 `TCPSocket`，Phase 2 会移除 TLS。当前 `TCPSocketFactory` 根据 `SecurityLevel` 选择创建 `TCPSocket` 或 `SecureSocket`。
   - What's unclear: Phase 1 是否需要保留 `SecureSocket` 的编译？如果 `TCPSocketFactory` 仍然存在（为 SecureSocket 服务），新的 `AsioTCPSocketFactory` 如何与之共存？
   - Recommendation: Phase 1 保留 `TCPSocketFactory` + `SecureSocket` 不变，新增 `AsioTCPSocketFactory` 仅创建 `AsioTCPSocket`。上层代码切换到使用新工厂。

3. **IStream::flush() 语义在异步模型中的实现**
   - What we know: 当前 `flush()` 使用 `CondVar<bool>` 阻塞等待所有输出缓冲区数据写完。上层代码（`ClientProxy::close()`）调用 `getStream()->flush()`。
   - What's unclear: 异步模型中 `flush()` 无法真正阻塞等待 async_write 完成（会阻塞 Asio 线程）。需要一个同步等待机制。
   - Recommendation: `flush()` 使用 `std::promise` / `std::future` 模式：post 一个 flush 任务到 io_context，等待 future 完成。

4. **App 类中 SocketMultiplexer 的过渡期处理**
   - What we know: `App` 基类持有 `std::unique_ptr<SocketMultiplexer> m_socketMultiplexer`。`ClientApp` 和 `ServerApp` 在初始化时创建它。
   - What's unclear: 过渡期是否需要同时存在 SocketMultiplexer 和 Asio 初始化？
   - Recommendation: 新增 `AsioContext` 或类似类，在 App 初始化时创建。SocketMultiplexer 暂保留（SecureSocket 可能需要），Phase 2 后移除。

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | 构建 | N/A (macOS 开发) | 3.24+ required | -- |
| C++20 编译器 | 全部 | N/A | MSVC 2019+ / Apple Clang 12+ | -- |
| Threading (pthread/std::thread) | Asio + SPSC | N/A | 系统提供 | -- |
| ws2_32 (Windows) | Asio socket | N/A | 系统库 | -- |
| Internet (构建时) | FetchContent 下载 Asio | N/A | -- | 预下载 Asio 到 vendor/ 目录 |

**Missing dependencies with no fallback:**
- 无（所有依赖均可通过 FetchContent 自动获取或为系统库）

**Missing dependencies with fallback:**
- 如果无网络（CI 离线构建），可使用预下载的 Asio 源码到 `vendor/asio/` 目录，修改 FetchContent 指向本地路径。

## Sources

### Primary (HIGH confidence)
- [Context7 /chriskohlhoff/asio] - async_read, async_write, ip::tcp::socket, io_context, steady_timer, strand, async_accept
- [GitHub chriskohlhoff/asio releases] - Asio 1.38.0 (2025-12-30), Boost Software License
- [代码库直接读取] - SocketMultiplexer.cpp, TCPSocket.cpp/h, IDataSocket.h, IStream.h, EventQueue.h, EventTypes.h, ProtocolUtil.h, IArchNetwork.h, TCPListenSocket.cpp, ClientProxy1_0.cpp, Server.cpp, App.h, TCPSocketFactory.cpp, CMakeLists.txt, net/CMakeLists.txt
- [.planning/codebase/CONCERNS.md] - SocketMultiplexer 线程安全问题、手动内存管理、cursor-mark 模式

### Secondary (MEDIUM confidence)
- [think-async.com/AsioStandalone.html] - Standalone Asio 官方说明，header-only，无需 Boost
- [GitHub OlivierLDff/asio.cmake] - CMake FetchContent 集成参考模式
- [atomic_queue GitHub Pages] - C++14 lock-free 队列参考，SPSC ring buffer 设计模式验证

### Tertiary (LOW confidence)
- [ASSUMED] SPSC 手写 50 行足够 -- 基于 lock-free 编程经验估算，未实际编写验证
- [ASSUMED] FetchContent 是最佳引入方式 -- Claude's Discretion 范围内的推荐

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Asio 版本已验证，CMake 集成模式已有成熟参考
- Architecture: HIGH - 现有代码库结构已完整读取，接口边界清晰
- Pitfalls: HIGH - 基于 Asio 官方文档和常见 C++ 异步 I/O 陷阱
- SPSC 实现: MEDIUM - 模式标准，但 MSVC 内存对齐细节需实际验证

**Research date:** 2026-05-12
**Valid until:** 2026-06-12 (30 天，Asio 稳定不会频繁变化)
