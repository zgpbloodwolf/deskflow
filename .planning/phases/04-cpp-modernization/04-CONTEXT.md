# Phase 4: C++ Modernization - Context

**Gathered:** 2026-05-13
**Status:** Ready for planning

<domain>
## Phase Boundary

将手动内存管理、自定义线程 unsafe 用法、reinterpret_cast 游标标记模式和 exit(0) 直接调用替换为现代 C++20 安全惯用法。目标：消除所有残留的 raw new/delete、不安全的指针转换和进程粗暴退出。

**包含：**
- SocketMultiplexer raw new/delete 成员指针替换为 unique_ptr（如 Phase 2/3 后仍存在）
- SocketMultiplexer reinterpret_cast cursor-mark sentinel 替换为类型安全迭代
- EventQueue raw Mutex*/CondVar* 成员指针替换为 unique_ptr
- 5 个 raw new Thread 调用点替换为 unique_ptr<Thread>
- InputFilter.cpp exit(0) hack 替换为 EventTypes::Quit 事件投递
- 确认 Phase 2/3 后 SocketMultiplexer 是否仍存在，存在则重构

**不包含：**
- 替换自定义 Thread 类本身（Apple Clang 不支持 std::jthread）
- 平台 API 互操作的 reinterpret_cast（Win32/X11/macOS，~45处，无法移除）
- 数据序列化类的 reinterpret_cast（char*→uint8_t*，~14处）
- ArgParser 中的 exit() 调用（CLI 解析阶段，早于 EventQueue 初始化）
- Windows 致命错误处理的 exit(1)（ArchMiscWindows.cpp）
- 两阶段锁协议重构（CondVar<bool> 保留，仅替换成员指针为智能指针）

</domain>

<decisions>
## Implementation Decisions

### Thread 替代方案
- **D-01:** 保留自定义 Thread 类（它封装了 ArchThread 平台抽象），不替换为 std::jthread 或 std::thread。原因：Apple Clang 不支持 std::jthread（代码库中有明确注释记录此限制），自定义 Thread 类提供了跨平台线程管理
- **D-02:** 仅将 5 个 raw new Thread 调用点替换为 unique_ptr<Thread>，不修改已使用 unique_ptr 包装的站点
- **D-03:** 不做条件编译 jthread 方案（避免维护两套代码路径）

### SocketMultiplexer 重构范围
- **D-04:** 将 7 个 raw 指针成员（Mutex*, Thread*, CondVar<bool>*×3）替换为 unique_ptr
- **D-05:** 将 reinterpret_cast cursor-mark sentinel 替换为类型安全的迭代方式（如 std::optional 迭代器或哨兵节点类型）
- **D-06:** 不重构两阶段锁协议（CondVar<bool> 机制保留），仅替换成员指针管理方式
- **D-07:** 先确认 Phase 2/3 执行后 SocketMultiplexer 是否仍存在。如已被删除，则跳过 SocketMultiplexer 相关重构

### EventQueue 智能指针化
- **D-08:** EventQueue.cpp 的 m_readyMutex* 和 m_readyCondVar* 成员指针替换为 unique_ptr
- **D-09:** 仅替换指针管理方式，不修改 EventQueue 的其他逻辑

### reinterpret_cast 清理范围
- **D-10:** 仅清理 SocketMultiplexer 的 cursor-mark sentinel 模式（1处），这是唯一的不安全 reinterpret_cast 用法
- **D-11:** 平台 API 互操作的 reinterpret_cast（~45处）和数据序列化的 reinterpret_cast（~14处）不纳入范围

### exit(0) 替换策略
- **D-12:** 仅替换 InputFilter.cpp:265 的 exit(0) hack，改为投递 EventTypes::Quit 事件到 EventQueue
- **D-13:** CoreArgParser.cpp 的 3 处 exit() 保留不变（CLI 解析阶段，早于 EventQueue 初始化）
- **D-14:** ArchMiscWindows.cpp:501 的 exit(1) 保留不变（致命错误处理）

### Claude's Discretion
- unique_ptr 替换的具体构造方式（make_unique vs 直接构造）
- sentinel 替换的具体类型安全实现（可选方案：哨兵类型、std::optional、index-based 迭代）
- 各重构点的执行顺序和分 plan 策略
- 是否需要在重构后补充单元测试

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### SocketMultiplexer 核心
- `src/lib/net/SocketMultiplexer.h` — SocketMultiplexer 接口，7 个 raw 指针成员声明
- `src/lib/net/SocketMultiplexer.cpp` — 实现（317 行），包含所有 raw new/delete 和 cursor-mark sentinel
- `src/lib/net/ISocketMultiplexerJob.h` — Job 接口，sentinel 模式的目标类型

### 自定义 Thread 类
- `src/lib/mt/Thread.h` — Thread 类定义，了解 ArchThread 封装和 IJob 采用模式
- `src/lib/mt/Thread.cpp` — Thread 实现

### Thread raw new 调用点（需替换为 unique_ptr）
- `src/lib/net/SocketMultiplexer.cpp` — 2 处 raw new Thread（行 40, 272）
- `src/lib/platform/MSWindowsScreenSaver.cpp` — 2 处 raw new Thread（行 176, 189）
- `src/lib/platform/MSWindowsDesks.cpp` — 1 处 raw new Thread（行 761）
- `src/lib/platform/PortalInputCapture.cpp` — 1 处 raw new Thread（行 33）
- `src/lib/platform/PortalRemoteDesktop.cpp` — 1 处 raw new Thread（行 24）

### EventQueue
- `src/lib/base/EventQueue.h` — EventQueue 类，m_readyMutex*/m_readyCondVar* 成员
- `src/lib/base/EventQueue.cpp` — 构造函数中 raw new，析构函数中 raw delete
- `src/lib/base/EventTypes.h` — EventTypes::Quit 事件类型（exit(0) 替换目标）

### exit(0) 替换目标
- `src/lib/server/InputFilter.cpp` — 行 265，exit(0) hack，需替换为 EventTypes::Quit

### 已现代化的参考站点
- `src/lib/platform/MSWindowsWatchdog.h` — 已用 unique_ptr<Thread>（行 133-135），可参考模式
- `src/lib/platform/OSXScreen.h` — 已用 unique_ptr<Thread>（行 311），可参考模式

### 约束与参考
- `.planning/codebase/CONCERNS.md` — SocketMultiplexer 无测试、手动内存管理、线程安全问题
- `.planning/codebase/CONVENTIONS.md` — 编码规范（PascalCase 类名、m_ 成员前缀、unique_ptr 模式）

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `unique_ptr<Thread>` 模式：MSWindowsWatchdog 和 OSXScreen 已成功使用此模式，可直接参考
- `std::unique_ptr` 广泛使用：SocketMultiplexer 本身就存储为 `unique_ptr<SocketMultiplexer>`（App.h:139），说明代码库已熟悉此模式
- `EventTypes::Quit` 机制：EventQueue 已有完整的事件驱动退出机制，ServerApp/ClientApp 多处使用

### Established Patterns
- 智能指针模式：unique_ptr 用于独占所有权（线程、Mutex、CondVar），shared_ptr 用于共享所有权（配置、socket）
- RAII 模式：Thread 类的析构函数调用 ARCH->closeThread()，unique_ptr 的自定义删除器可能需要考虑
- IJob 采用语义：Thread 构造函数接受 IJob* 并在完成后 delete，需要确保 unique_ptr 不破坏此约定

### Integration Points
- SocketMultiplexer 与 Thread 的耦合：m_thread 成员和 m_jobListLocker/m_jobListLockLocker 都是 Thread*，替换时需保持 lock/unlock 语义
- EventQueue 是全局核心组件：修改其成员指针需确保不破坏事件分发逻辑
- InputFilter::RestartServer::perform() 需要访问 EventQueue 实例来投递 Quit 事件

### 关键风险
- **SocketMultiplexer 可能已被删除**：Phase 1 用 AsioTCPSocket 替代了它，Phase 2/3 可能移除了代码。执行前必须确认
- **PortalInputCapture/PortalRemoteDesktop 是 Linux 平台代码**：Phase 3 可能已移除这些文件，其 raw new Thread 站点可能不存在
- **Apple Clang 不支持 std::jthread**：这是硬约束，不可违反

</code_context>

<specifics>
## Specific Ideas

- 重构前先运行 grep 确认目标文件和调用点是否仍存在（Phase 2/3 可能已删除部分代码）
- 参考 MSWindowsWatchdog.h 中 unique_ptr<Thread> 的使用方式作为模板
- InputFilter exit(0) 替换可参考 ServerApp.cpp 中已有的 EventTypes::Quit 投递模式

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 4-C++ Modernization*
*Context gathered: 2026-05-13*
