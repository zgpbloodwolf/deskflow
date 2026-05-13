# Phase 4: C++ Modernization - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-13
**Phase:** 04-cpp-modernization
**Areas discussed:** Thread 替代方案, SocketMultiplexer 重构范围, reinterpret_cast 清理范围, exit(0) 替换策略

---

## Thread 替代方案

| Option | Description | Selected |
|--------|-------------|----------|
| 保留 Thread 类 + 智能指针管理 | 保留自定义 Thread 类，将 raw new/delete 替换为 unique_ptr<Thread> | ✓ |
| 条件编译 jthread | Windows/Clang>=16 用 std::jthread，macOS 保留自定义 Thread | |
| std::thread + 手动 stop | 用 std::thread 替换，手动实现 cooperative cancellation | |
| Claude 决定 | 让 Claude 根据代码侦察结果选择 | |

**User's choice:** 保留 Thread 类 + 智能指针管理（推荐）
**Notes:** ROADMAP 要求 std::jthread + std::stop_token，但 Apple Clang 不支持 std::jthread。代码库明确记录此限制（AppUtilWindows.cpp: `// NOSONAR - No jthread on Windows`，ExitTimeout.h: Apple Clang 无 jthread 支持）

---

## Thread 智能指针化范围

| Option | Description | Selected |
|--------|-------------|----------|
| 仅修复 raw new 站点 | 仅替换 5 个 raw new Thread 调用点为 unique_ptr<Thread> | ✓ |
| 全面审查所有 Thread 使用 | 审查 13 个 Thread 导入文件，确保所有使用通过智能指针管理 | |
| Claude 决定 | 让 Claude 决定 | |

**User's choice:** 仅修复 raw new 站点（推荐）
**Notes:** 5 个 raw new Thread 站点：SocketMultiplexer(2)、MSWindowsScreenSaver(2)、MSWindowsDesks(1)、PortalInputCapture(1)、PortalRemoteDesktop(1)。其中 Portal* 可能被 Phase 3 删除。

---

## SocketMultiplexer 重构程度

| Option | Description | Selected |
|--------|-------------|----------|
| 智能指针 + sentinel 重构 | 替换 7 个 raw 指针成员为 unique_ptr + 重构 cursor-mark sentinel | ✓ |
| 仅智能指针替换 | 仅替换 raw 指针，保留 sentinel 模式 | |
| 全面重构（含锁协议） | 在智能指针 + sentinel 基础上还重构两阶段锁为 std::mutex | |
| Claude 决定 | 让 Claude 决定 | |

**User's choice:** 智能指针 + sentinel 重构（推荐）
**Notes:** 三层问题中的两层被纳入：raw new/delete 和 sentinel。两阶段锁协议（CondVar<bool>）保留不变。

---

## SocketMultiplexer 存在性确认

| Option | Description | Selected |
|--------|-------------|----------|
| 确认存在后再重构 | Phase 1 已用 AsioTCPSocket 替代，先确认 Phase 2/3 后是否仍存在 | ✓ |
| 无条件计划重构 | 不管是否被移除都计划重构 | |

**User's choice:** 确认存在后再重构（推荐）
**Notes:** Phase 1 已引入 AsioTCPSocket 替代 SocketMultiplexer。如果 Phase 2/3 删除了 SocketMultiplexer 代码，则跳过相关重构。

---

## EventQueue 纳入范围

| Option | Description | Selected |
|--------|-------------|----------|
| 仅智能指针替换成员 | EventQueue 的 m_readyMutex*/m_readyCondVar* 替换为 unique_ptr | ✓ |
| 不纳入 Phase 范围 | EventQueue 不在 SUCCESS CRITERIA 中，留给未来 | |

**User's choice:** 仅智能指针替换成员（推荐）
**Notes:** EventQueue.cpp 构造函数 raw new（行 32, 41-42），析构函数 raw delete。仅替换指针管理，不修改事件分发逻辑。

---

## reinterpret_cast 清理范围

| Option | Description | Selected |
|--------|-------------|----------|
| 仅 sentinel 模式 | 仅清理 SocketMultiplexer cursor-mark sentinel（1处不安全模式） | ✓ |
| sentinel + 序列化 cast | 额外清理数据序列化类 cast（~14处） | |
| Claude 决定 | 让 Claude 决定 | |

**User's choice:** 仅 sentinel 模式（推荐）
**Notes:** 60 处 reinterpret_cast 中 3 类：sentinel（1处不安全）、平台 API 互操作（~45处无法移除）、数据序列化（~14处可用 static_cast 但非必要）。

---

## exit(0) 替换范围

| Option | Description | Selected |
|--------|-------------|----------|
| 仅 InputFilter hack | 仅替换 InputFilter.cpp:265 的 exit(0) | ✓ |
| InputFilter + ArgParser | 额外替换 CoreArgParser.cpp 3 处 exit() | |
| 全部替换 | 替换所有 exit() 调用 | |
| Claude 决定 | 让 Claude 决定 | |

**User's choice:** 仅 InputFilter hack（推荐）
**Notes:** 5 处 exit() 调用中：InputFilter.cpp:265 是需要替换的 hack（`// HACK Super hack we should gracefully exit`）；CoreArgParser 3 处在 CLI 解析阶段（早于 EventQueue）；ArchMiscWindows.cpp:501 是致命错误处理。

---

## Claude's Discretion

- unique_ptr 替换的具体构造方式（make_unique vs 直接构造）
- sentinel 替换的具体类型安全实现方式
- 各重构点的执行顺序和分 plan 策略
- 是否需要在重构后补充单元测试

## Deferred Ideas

None — discussion stayed within phase scope
