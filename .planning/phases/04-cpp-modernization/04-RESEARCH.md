# Phase 4: C++ Modernization - Research

**Researched:** 2026-05-13
**Domain:** C++ memory safety, smart pointer migration, type-safe iteration patterns
**Confidence:** HIGH

## Summary

Phase 4 对 Deskflow 代码库进行 C++ 现代化改造，涉及四个独立的技术领域：(1) SocketMultiplexer 和 EventQueue 中的 raw new/delete 替换为 unique_ptr；(2) 7 处 raw `new Thread` 调用点替换为 `unique_ptr<Thread>`；(3) SocketMultiplexer 的 reinterpret_cast cursor-mark sentinel 替换为类型安全迭代；(4) InputFilter.cpp 的 `exit(0)` hack 替换为 EventTypes::Quit 事件投递。

关键发现：SocketMultiplexer 在 Phase 1 引入 AsioTCPSocket 后**仍然存在**且仍被 TCPSocket/App 等大量引用，必须重构。PortalInputCapture/PortalRemoteDesktop 文件也仍然存在（Phase 3 尚未执行）。RestartServer 类**没有** IEventQueue 成员指针，需要添加才能投递 Quit 事件。MSWindowsDesks::Desk 的 m_thread 是 struct 内的裸指针，其生命周期管理方式（new/removeDesk 中 delete）与 MSWindowsScreenSaver 不同，需要不同的 unique_ptr 策略。

**Primary recommendation:** 按 SocketMultiplexer -> EventQueue -> Thread sites -> exit(0) 的顺序分 3-4 个 plan 执行，SocketMultiplexer 最复杂应独立成一个 plan。

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** 保留自定义 Thread 类（Apple Clang 不支持 std::jthread），不替换为 std::jthread 或 std::thread
- **D-02:** 仅将 5 个 raw new Thread 调用点替换为 unique_ptr<Thread>，不修改已使用 unique_ptr 包装的站点
- **D-03:** 不做条件编译 jthread 方案（避免维护两套代码路径）
- **D-04:** 将 7 个 raw 指针成员（Mutex*, Thread*, CondVar<bool>*x3）替换为 unique_ptr
- **D-05:** 将 reinterpret_cast cursor-mark sentinel 替换为类型安全的迭代方式
- **D-06:** 不重构两阶段锁协议（CondVar<bool> 机制保留），仅替换成员指针管理方式
- **D-07:** 先确认 Phase 2/3 执行后 SocketMultiplexer 是否仍存在。如已被删除，则跳过
- **D-08:** EventQueue.cpp 的 m_readyMutex* 和 m_readyCondVar* 成员指针替换为 unique_ptr
- **D-09:** 仅替换指针管理方式，不修改 EventQueue 的其他逻辑
- **D-10:** 仅清理 SocketMultiplexer 的 cursor-mark sentinel 模式（1处）
- **D-11:** 平台 API 互操作的 reinterpret_cast（~45处）和数据序列化的 reinterpret_cast（~14处）不纳入范围
- **D-12:** 仅替换 InputFilter.cpp:265 的 exit(0) hack，改为投递 EventTypes::Quit 事件
- **D-13:** CoreArgParser.cpp 的 3 处 exit() 保留不变
- **D-14:** ArchMiscWindows.cpp:501 的 exit(1) 保留不变

### Claude's Discretion
- unique_ptr 替换的具体构造方式（make_unique vs 直接构造）
- sentinel 替换的具体类型安全实现（可选方案：哨兵类型、std::optional、index-based 迭代）
- 各重构点的执行顺序和分 plan 策略
- 是否需要在重构后补充单元测试

### Deferred Ideas (OUT OF SCOPE)
None
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| MOD-01 | SocketMultiplexer 中的手动 new/delete 替换为智能指针 | SocketMultiplexer 存在 7 个 raw 指针成员 + unique_ptr 构造模式参考（MSWindowsWatchdog） |
| MOD-02 | 自定义 Thread 类替换为 std::jthread + std::stop_token | D-01 锁定为保留 Thread 类 + unique_ptr<Thread>，7 处 raw new Thread 已定位 |
| MOD-03 | reinterpret_cast 游标标记模式替换为安全的迭代方式 | SocketMultiplexer 的 m_cursorMark reinterpret_cast 模式已完整分析，替换方案已研究 |
| MOD-04 | exit(0) 调用替换为优雅的事件队列退出 | InputFilter::RestartServer::perform() 已分析，需添加 IEventQueue* 成员 |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| SocketMultiplexer 智能指针化 | API / Backend | -- | 网络层的内存管理完全在 backend 库内部 |
| EventQueue 智能指针化 | API / Backend | -- | 核心事件基础设施，无 UI 依赖 |
| Thread raw new 替换 | API / Backend | -- | 线程创建在 platform 和 net 层，不涉及 UI |
| cursor-mark sentinel 替换 | API / Backend | -- | SocketMultiplexer 内部迭代逻辑 |
| exit(0) 替换 | API / Backend | -- | InputFilter 在 server 库中，事件系统是后端基础设施 |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| std::unique_ptr | C++11 (full in C++20) | 独占所有权智能指针 | 项目已广泛使用，无需引入新依赖 [VERIFIED: codebase grep] |
| std::make_unique | C++14 | 安全构造 unique_ptr | 代码库已有使用先例 [VERIFIED: MSWindowsWatchdog.cpp:75] |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::list | C++98 | SocketMultiplexer 的 job list 容器 | 保持现有容器，仅改迭代方式 |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| unique_ptr + 自定义 Thread | std::jthread + stop_token | Apple Clang 不支持，D-01 已锁定 [VERIFIED: Thread.h 注释] |
| reinterpret_cast sentinel | index-based iteration | index 更安全但需改更多代码；std::optional sentinel 更贴合现有架构 |

**Installation:**
无新依赖需安装。所有改造使用标准库。

## Architecture Patterns

### System Architecture Diagram

```
调用方 (App, TCPSocket)
    |
    v
SocketMultiplexer::addSocket / removeSocket
    |                       |
    v                       v
lockJobListLock()       lockJobList()
    |                       |
    +-- new Thread ----------+   <-- raw new (需替换为 unique_ptr)
    |                       |
    v                       v
CondVar<bool> 等待      Cursor 迭代     <-- reinterpret_cast sentinel (需替换)
    |                       |
    v                       v
serviceThread()         nextCursor()
    |                       |
    v                       v
ISocketMultiplexerJob::run()
    |
    v
返回新 job 或 nullptr
```

```
EventQueue 构造
    |
    v
new Mutex, new CondVar<bool>  <-- raw new (需替换为 make_unique)
    |
    v
waitForReady() 使用 m_readyMutex/m_readyCondVar
    |
    v
loop() 使用 *m_readyCondVar
    |
    v
析构: delete m_readyCondVar, delete m_readyMutex  <-- raw delete (unique_ptr 自动处理)
```

```
InputFilter::RestartServer::perform()
    |
    v
exit(0)  <-- 需替换为 m_events->addEvent(Event(EventTypes::Quit))
    |
    v
EventQueue::loop() 收到 Quit -> 优雅退出
```

### Recommended Project Structure
```
src/lib/net/
  SocketMultiplexer.h    # 7 个 raw 指针成员 -> unique_ptr
  SocketMultiplexer.cpp  # 构造/析构/lockJobListLock/unlockJobList 改造
src/lib/base/
  EventQueue.h           # m_readyMutex*/m_readyCondVar* -> unique_ptr
  EventQueue.cpp         # 构造/析构改造
src/lib/mt/
  Thread.h               # 不修改（D-01 保留）
src/lib/server/
  InputFilter.h          # RestartServer 添加 IEventQueue* 成员
  InputFilter.cpp        # RestartServer::perform() 替换 exit(0)
src/lib/platform/
  MSWindowsScreenSaver.h  # m_watch: Thread* -> unique_ptr<Thread>
  MSWindowsScreenSaver.cpp
  MSWindowsDesks.h        # Desk::m_thread: Thread* -> unique_ptr<Thread>
  MSWindowsDesks.cpp
  PortalInputCapture.h    # m_glibThread: Thread* -> unique_ptr<Thread> (如仍存在)
  PortalInputCapture.cpp
  PortalRemoteDesktop.h   # m_glibThread: Thread* -> unique_ptr<Thread> (如仍存在)
  PortalRemoteDesktop.cpp
```

### Pattern 1: unique_ptr<Thread> 替换 raw new Thread
**What:** 将 `Thread* m = new Thread(job)` 替换为 `std::unique_ptr<Thread> m = std::make_unique<Thread>(job)`
**When to use:** 所有 Thread 的独占所有权场景
**Example:**
```cpp
// 来源: MSWindowsWatchdog.cpp:75 (已现代化参考实现)
m_mainThread = std::make_unique<Thread>(new TMethodJob(this, &MSWindowsWatchdog::mainLoop, nullptr));
m_outputThread = std::make_unique<Thread>(new TMethodJob(this, &MSWindowsWatchdog::outputLoop, nullptr));

// 停止线程时:
if (!m_mainThread->wait(kThreadWaitSeconds)) {
  LOG_WARN("main thread did not stop in time");
}
// unique_ptr 自动析构，不需要 delete
```

### Pattern 2: unique_ptr 替换 Mutex*/CondVar* 成员
**What:** 将 `Mutex* m_mutex = new Mutex` 替换为 `std::unique_ptr<Mutex> m_mutex = std::make_unique<Mutex>()`
**When to use:** Mutex/CondVar 的成员指针，CondVar 需要 Mutex* 参数
**Example:**
```cpp
// 替换前:
EventQueue::EventQueue()
    : m_readyMutex(new Mutex),
      m_readyCondVar(new CondVar<bool>(m_readyMutex, false)) {}

// 替换后:
EventQueue::EventQueue()
    : m_readyMutex(std::make_unique<Mutex>()),
      m_readyCondVar(std::make_unique<CondVar<bool>>(m_readyMutex.get(), false)) {}

// 使用处需加 .get():
// 替换前: Lock lock(m_readyMutex);
// 替换后: Lock lock(m_readyMutex.get());
// 或保持不变，因为 Lock 接受 Mutex*，unique_ptr 隐式转换?

// 注意: Lock 类接受 Mutex& 或 Mutex* 参数，需确认接口
```

### Pattern 3: Sentinel 类型替换 reinterpret_cast
**What:** 将 `reinterpret_cast<ISocketMultiplexerJob*>(this)` 替换为类型安全的 sentinel
**When to use:** cursor-mark 模式需要特殊标记值区分 dummy node 和真实 job
**Example:**
```cpp
// 替换前:
m_cursorMark = reinterpret_cast<ISocketMultiplexerJob *>(this);
// ...判断: if (*i != m_cursorMark)

// 替换方案 A: 静态哨兵地址（最简单）
struct CursorMarkTag {};
static CursorMarkTag s_cursorMark;
// m_cursorMark = reinterpret_cast<ISocketMultiplexerJob*>(&s_cursorMark);
// 仍用 reinterpret_cast 但类型安全 -- 不推荐

// 替换方案 B: index-based 迭代（推荐）
// 不再使用 dummy node 插入，改用 index 跟踪迭代位置
// newCursor() 返回 size_t index
// nextCursor() 从 index 向后查找非空元素
// deleteCursor() 不需要（没有 dummy node）

// 替换方案 C: std::optional<JobCursor> 哨兵
// 最小改动，用特定值标记 cursor 位置
```

### Anti-Patterns to Avoid
- **直接 reset() 替换 new 赋值:** `m_thread.reset(new Thread(...))` 比 `make_unique` 不安全（如果 new Thread 抛异常）
- **忘记修改 CondVar 构造参数:** `CondVar<bool>` 构造需要 `Mutex*`，unique_ptr 后需 `.get()`
- **修改 Thread 类的 IJob 所有权语义:** Thread 构造函数 adopt IJob 并在 threadFunc 中 delete，unique_ptr<Thread> 不改变这个语义
- **在 struct Desk 中用 unique_ptr 但保持 Desk* raw 指针管理:** MSWindowsDesks::Desk 本身由 raw new/delete 管理（Desk* 在 map 中），仅改 m_thread 成员

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| 智能指针构造 | `unique_ptr<T>(new T(...))` | `make_unique<T>(...)` | 异常安全，避免内存泄漏 |
| 线程管理 | 自定义 Thread 析构逻辑 | Thread 类已有 ARCH->closeThread() | Thread::~Thread() 已实现正确的资源释放 [VERIFIED: Thread.cpp:41-44] |
| Mutex/CondVar 生命周期 | 手动 delete 顺序 | unique_ptr 自动析构 | 析构顺序与声明顺序相反（C++ 标准），CondVar 先析构再 Mutex，保证安全 |

**Key insight:** Thread 类的析构函数已经调用 `ARCH->closeThread(m_thread)` 来正确清理平台线程资源。unique_ptr<Thread> 的析构会调用 Thread::~Thread()，不需要自定义删除器。但必须确保在析构前线程已停止（cancel + wait），否则 closeThread 可能死锁。

## Common Pitfalls

### Pitfall 1: SocketMultiplexer 的 lockJobListLock/unlockJobList Thread 指针不对称
**What goes wrong:** lockJobListLock() 在 line 272 `new Thread(Thread::getCurrentThread())` 创建 m_jobListLockLocker，然后在 lockJobList() line 289 转移到 m_jobListLocker，最后在 unlockJobList() line 305 `delete m_jobListLocker`。如果 lock/unlock 路径不对称（异常、提前返回），会泄漏或 double-delete。
**Why it happens:** 手动 new/delete 配对在复杂控制流中极易出错。
**How to avoid:** 使用 unique_ptr 后，所有权转移用 `std::move()` 显式表达，unique_ptr 自身的非空检查防止 double-delete。
**Warning signs:** lockJobList() 和 unlockJobList() 调用次数不匹配时，raw 指针会泄漏。

### Pitfall 2: CondVar<bool> 构造需要 Mutex* 参数
**What goes wrong:** `unique_ptr<Mutex>` 不能隐式转换为 `Mutex*`，CondVar 构造函数需要 `Mutex*`。
**Why it happens:** unique_ptr 设计上不提供隐式指针转换（防止意外悬空指针）。
**How to avoid:** 使用 `m_mutex.get()` 传递给 CondVar 构造函数。所有接受 `Mutex*` 参数的地方（如 `Lock lock(m_mutex)`）也需要检查是否接受 `Mutex*` 参数。
**Warning signs:** 编译错误 "cannot convert std::unique_ptr<Mutex> to Mutex*"。

### Pitfall 3: MSWindowsDesks::Desk 的 unique_ptr<Thread> 与 Desk* raw 指针共存
**What goes wrong:** Desk struct 本身由 `new Desk` 创建、`delete desk` 销毁（在 removeDesks() line 774）。如果 Desk::m_thread 是 unique_ptr，当 `delete desk` 时 unique_ptr 自动析构 Thread，但如果 Thread 还没 wait() 就析构了，可能死锁。
**Why it happens:** 原代码在 removeDesks() 中先 `desk->m_thread->wait()` 再 `delete desk->m_thread` 再 `delete desk`，顺序很重要。
**How to avoid:** 保持 removeDesks() 中先 wait() 再 reset() unique_ptr 的顺序。unique_ptr 的析构会自动 delete Thread，所以不再需要手动 delete。
**Warning signs:** 析构顺序错误导致 ARCH->closeThread 在线程运行时调用。

### Pitfall 4: RestartServer 缺少 IEventQueue 访问
**What goes wrong:** RestartServer::perform() 调用 exit(0) 是因为它没有 m_events 成员来投递 Quit 事件。其他 Action 子类（LockCursorToScreenAction 等）都有 IEventQueue* m_events。
**Why it happens:** RestartServer 是后加的 Action 子类，设计时没有遵循其他 Action 的模式（构造函数接受 IEventQueue*）。
**How to avoid:** 添加 IEventQueue* 成员和构造函数参数。同时修改 Config.cpp 中创建 RestartServer 的地方传入 events 指针。clone() 也需要适配。
**Warning signs:** RestartServer 构造函数签名与其他 Action 不一致（无 IEventQueue 参数）。

### Pitfall 5: SocketMultiplexer sentinel 替换影响迭代逻辑
**What goes wrong:** newCursor()/nextCursor()/deleteCursor() 三个方法构成完整的 cursor 生命周期，替换 sentinel 标记方式会同时影响这三个方法的实现。
**Why it happens:** dummy node 插入到 std::list 中作为 cursor 的锚点，nextCursor 通过跳过所有 m_cursorMark 节点找到下一个真实 job。替换为 index-based 方式需要同时改三个方法。
**How to avoid:** 选择最小改动的替换方案。推荐使用一个专门的静态 sentinel 对象指针（而非 reinterpret_cast），或者改用 index-based 迭代彻底消除 dummy node 机制。
**Warning signs:** nextCursor 返回错误迭代器导致 serviceThread 遍历跳过或重复处理 job。

## Code Examples

### unique_ptr<Thread> 参考实现 (MSWindowsWatchdog)
```cpp
// 来源: src/lib/platform/MSWindowsWatchdog.h:133-135 (已验证存在)
std::unique_ptr<Thread> m_mainThread;
std::unique_ptr<Thread> m_outputThread;
std::unique_ptr<Thread> m_sasThread;

// 来源: src/lib/platform/MSWindowsWatchdog.cpp:75-76
m_mainThread = std::make_unique<Thread>(new TMethodJob(this, &MSWindowsWatchdog::mainLoop, nullptr));
m_outputThread = std::make_unique<Thread>(new TMethodJob(this, &MSWindowsWatchdog::outputLoop, nullptr));

// 停止: src/lib/platform/MSWindowsWatchdog.cpp:86-94
// wait() 后不需要 delete，unique_ptr 析构自动处理
if (!m_mainThread->wait(kThreadWaitSeconds)) {
  LOG_WARN("main thread did not stop in time");
}
```

### SocketMultiplexer raw 指针成员声明 (需替换)
```cpp
// 来源: src/lib/net/SocketMultiplexer.h:90-98 (已验证存在)
Mutex *m_mutex = nullptr;                     // -> unique_ptr<Mutex>
Thread *m_thread = nullptr;                   // -> unique_ptr<Thread>
CondVar<bool> *m_jobsReady = nullptr;         // -> unique_ptr<CondVar<bool>>
CondVar<bool> *m_jobListLock = nullptr;       // -> unique_ptr<CondVar<bool>>
CondVar<bool> *m_jobListLockLocked = nullptr; // -> unique_ptr<CondVar<bool>>
Thread *m_jobListLocker = nullptr;            // -> unique_ptr<Thread>
Thread *m_jobListLockLocker = nullptr;        // -> unique_ptr<Thread>
```

### SocketMultiplexer 析构函数 raw delete (需替换)
```cpp
// 来源: src/lib/net/SocketMultiplexer.cpp:43-59 (已验证存在)
SocketMultiplexer::~SocketMultiplexer()
{
  m_thread->cancel();
  m_thread->unblockPollSocket();
  m_thread->wait();
  delete m_thread;
  delete m_jobsReady;
  delete m_jobListLock;
  delete m_jobListLockLocked;
  delete m_jobListLocker;
  delete m_jobListLockLocker;
  delete m_mutex;
  // ...
}
// unique_ptr 替换后: cancel/wait 保留，所有 delete 消除
```

### SocketMultiplexer reinterpret_cast sentinel (需替换)
```cpp
// 来源: src/lib/net/SocketMultiplexer.cpp:35-36 (已验证存在)
// TODO: Remove this evilness
m_cursorMark = reinterpret_cast<ISocketMultiplexerJob *>(this);

// 来源: src/lib/net/SocketMultiplexer.cpp:237-253
// nextCursor 通过比较 *i != m_cursorMark 来跳过 dummy 节点
```

### EventQueue raw new/delete (需替换)
```cpp
// 来源: src/lib/base/EventQueue.h:121-122 (已验证存在)
Mutex *m_readyMutex = nullptr;
CondVar<bool> *m_readyCondVar = nullptr;

// 来源: src/lib/base/EventQueue.cpp:32-33
EventQueue::EventQueue()
    : m_readyMutex(new Mutex),
      m_readyCondVar(new CondVar<bool>(m_readyMutex, false))

// 来源: src/lib/base/EventQueue.cpp:41-42
~EventQueue() {
  delete m_readyCondVar;
  delete m_readyMutex;
}

// 使用处 (EventQueue.cpp:52):
Lock lock(m_readyMutex);  // Lock 接受 Mutex* 参数
```

### exit(0) hack (需替换)
```cpp
// 来源: src/lib/server/InputFilter.cpp:262-265 (已验证存在)
void InputFilter::RestartServer::perform(const Event &)
{
  // HACK Super hack we should gracefully exit
  exit(0);
}
```

### EventTypes::Quit 参考用法 (ServerApp)
```cpp
// 来源: src/lib/deskflow/ServerApp.cpp:164-167 (已验证存在)
getEvents()->addEvent(Event(EventTypes::Quit));
```

## File Existence Audit

| File | Exists? | Impact |
|------|---------|--------|
| src/lib/net/SocketMultiplexer.h/.cpp | YES [VERIFIED: find] | 必须重构 |
| src/lib/net/ISocketMultiplexerJob.h | YES [VERIFIED: find] | sentinel 类型依赖此接口 |
| src/lib/base/EventQueue.h/.cpp | YES [VERIFIED: read] | 必须重构 |
| src/lib/server/InputFilter.cpp | YES [VERIFIED: read] | exit(0) 在 line 265 |
| src/lib/server/InputFilter.h | YES [VERIFIED: read] | RestartServer 类声明 |
| src/lib/platform/PortalInputCapture.cpp/.h | YES [VERIFIED: find] | raw new Thread 在 line 33 |
| src/lib/platform/PortalRemoteDesktop.cpp/.h | YES [VERIFIED: find] | raw new Thread 在 line 24 |
| src/lib/platform/MSWindowsScreenSaver.cpp/.h | YES [VERIFIED: read] | 2 处 raw new Thread |
| src/lib/platform/MSWindowsDesks.cpp/.h | YES [VERIFIED: read] | 1 处 raw new Thread (Desk struct) |
| src/lib/platform/MSWindowsWatchdog.h | YES [VERIFIED: read] | unique_ptr<Thread> 参考实现 |
| src/lib/platform/OSXScreen.h | YES [VERIFIED: read] | unique_ptr<Thread> 参考实现 |

## Raw new Thread 完整列表

| # | File | Line | Code | 成员类型 | 重构策略 |
|---|------|------|------|---------|---------|
| 1 | SocketMultiplexer.cpp | 40 | `m_thread = new Thread(tMethodJob)` | Thread* | unique_ptr<Thread> + make_unique |
| 2 | SocketMultiplexer.cpp | 272 | `m_jobListLockLocker = new Thread(...)` | Thread* | unique_ptr<Thread> + make_unique |
| 3 | MSWindowsScreenSaver.cpp | 176 | `m_watch = new Thread(new TMethodJob...)` | Thread* | unique_ptr<Thread> + make_unique |
| 4 | MSWindowsScreenSaver.cpp | 189 | `m_watch = new Thread(new TMethodJob...)` | Thread* | 同上（同一成员） |
| 5 | MSWindowsDesks.cpp | 761 | `desk->m_thread = new Thread(...)` | Thread* (Desk struct) | unique_ptr<Thread> Desk 成员 |
| 6 | PortalInputCapture.cpp | 33 | `m_glibThread = new Thread(tMethodJob)` | Thread* | unique_ptr<Thread> + make_unique |
| 7 | PortalRemoteDesktop.cpp | 24 | `m_glibThread = new Thread(tMethodJob)` | Thread* | unique_ptr<Thread> + make_unique |

**注意:** CONTEXT.md 说是 5 个，实际 grep 找到 7 个。D-02 说 "仅将 5 个 raw new Thread 调用点替换"，但实际有 7 处 new Thread 调用。MSWindowsScreenSaver 有 2 处但操作同一成员 m_watch，可算为 1 个调用点。SocketMultiplexer 有 2 处操作不同成员（m_thread 和 m_jobListLockLocker），是 2 个调用点。总计 6 个独立调用点（7 处代码行）。

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| raw new/delete | unique_ptr + make_unique | C++14/17 | 异常安全，自动析构 |
| reinterpret_cast sentinel | 类型安全迭代器/optional | C++17/20 | 消除 UB 风险 |
| exit(0) 进程终止 | 事件驱动优雅退出 | -- | 正确清理资源 |

**Deprecated/outdated:**
- `std::auto_ptr`: 已在 C++17 移除，代码库未使用
- raw new/delete: 现代 C++ 视为反模式，项目已部分迁移（MSWindowsWatchdog, App.h）

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Lock 类构造函数接受 Mutex* 参数 | Architecture Patterns | 如果 Lock 接受 Mutex&，unique_ptr<Mutex> 需要 * 解引用而非 .get() |
| A2 | PortalInputCapture/PortalRemoteDesktop 在 Phase 3 后可能被删除 | Thread sites | 如果仍存在则需重构，如果被删则跳过 |
| A3 | Config.cpp 创建 RestartServer 时有 IEventQueue 可用 | exit(0) 替换 | 需要确认 Config 构造 RestartServer 的上下文 |

## Open Questions

1. **Lock 类接口: Lock 接受 Mutex* 还是 Mutex&?**
   - What we know: EventQueue.cpp:52 使用 `Lock lock(m_readyMutex)` 其中 m_readyMutex 是 Mutex*
   - What's unclear: 如果 m_readyMutex 变为 unique_ptr<Mutex>，是否需要 `Lock lock(m_readyMutex.get())` 或 `Lock lock(*m_readyMutex)`
   - Recommendation: 执行时先查看 Lock 类定义，确定接口

2. **Phase 3 执行后 Portal 文件是否仍存在?**
   - What we know: 当前存在且包含 raw new Thread
   - What's unclear: Phase 3 (Platform & Feature Cleanup) 可能删除 Linux 平台支持
   - Recommendation: 在 Phase 4 执行前确认，或 conditional 地包含 Portal 文件重构

3. **RestartServer 修改是否需要同步修改 Config.cpp 的解析逻辑?**
   - What we know: Config.cpp:1112 `action = new InputFilter::RestartServer(mode)` 只传 mode，没有 IEventQueue
   - What's unclear: Config 类的 parseAction 上下文是否能获取 IEventQueue
   - Recommendation: 需要追踪 Config 类如何获取 IEventQueue（通过 ServerApp 或全局）

## Environment Availability

Step 2.6: SKIPPED (no external dependencies identified -- all changes use C++ standard library features already available in the project's C++20 build environment)

## Sources

### Primary (HIGH confidence)
- Codebase grep for SocketMultiplexer existence [VERIFIED]
- Codebase grep for raw `new Thread` call sites [VERIFIED: 7 sites found]
- Codebase read of SocketMultiplexer.h/cpp full content [VERIFIED]
- Codebase read of EventQueue.h/cpp full content [VERIFIED]
- Codebase read of InputFilter.cpp exit(0) context [VERIFIED]
- Codebase read of MSWindowsWatchdog.h reference implementation [VERIFIED]
- Codebase read of Thread.h/cpp IJob ownership semantics [VERIFIED]

### Secondary (MEDIUM confidence)
- CONTEXT.md canonical references [CITED: phase discussion output]
- CONCERNS.md SocketMultiplexer analysis [CITED: codebase audit]
- CONVENTIONS.md coding standards [CITED: codebase audit]

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - 使用代码库已有的 unique_ptr/make_unique 模式，无需新依赖
- Architecture: HIGH - 所有目标文件已读取验证，模式清晰
- Pitfalls: HIGH - 基于实际代码分析，特别是 Lock 类接口和 Desk struct 生命周期

**Research date:** 2026-05-13
**Valid until:** 2026-06-12 (C++ 标准库模式稳定，30 天有效)
