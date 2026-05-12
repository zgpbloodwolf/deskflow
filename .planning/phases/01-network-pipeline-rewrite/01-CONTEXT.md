# Phase 1: Network Pipeline Rewrite - Context

**Gathered:** 2026-05-12
**Status:** Ready for planning

<domain>
## Phase Boundary

替换阻塞式 SocketMultiplexer 为基于 Standalone Asio 的非阻塞管道架构，使用 lock-free SPSC 缓冲区分离键盘和鼠标事件流，实现鼠标移动合并发送（200Hz）和按键释放保障。目标：网络波动时鼠标移动流畅、键盘事件不丢失、断连时无卡键。

**包含：**
- 引入 Standalone Asio 替代 SocketMultiplexer 的阻塞 I/O
- 实现 lock-free SPSC 事件缓冲区（键盘 FIFO + 鼠标 atomic 最新位置槽）
- 鼠标移动事件合并（1000Hz 输入 → 200Hz 发送）
- 断连时按键释放机制（发送端状态表）
- 自动重连（指数退避）
- 有界 poll 超时防止线程阻塞

**不包含：**
- TLS/SSL 层修改（Phase 2）
- 平台层修改（Phase 3）
- C++ 现代化（智能指针替换、jthread，Phase 4）
- 协议格式修改
- EventQueue 替换

</domain>

<decisions>
## Implementation Decisions

### I/O 模型选择
- **D-01:** 使用 Standalone Asio (header-only) 作为网络 I/O 库，替代现有 SocketMultiplexer 的阻塞 I/O
- **D-02:** Asio 仅负责 socket I/O 和定时器，运行在独立线程；不替代现有 EventQueue
- **D-03:** 使用 Asio steady_timer 处理 poll 超时和连接超时，替代 Arch::sleep()

### 线程架构设计
- **D-04:** 每连接单线程模型 — 每个连接拥有独立的 Asio io_context 线程处理发送和接收
- **D-05:** Lock-free SPSC 缓冲区采用分离队列组织：键盘事件走 FIFO 队列（保证不丢失），鼠标移动走 atomic 最新位置槽（覆盖旧值，只发最新位置）
- **D-06:** Asio I/O 完成后通过桥接层投递事件到 EventQueue，上层 Server/Client 逻辑不变

### 协议兼容性
- **D-07:** 完全兼容现有 ProtocolUtil::writef 二进制协议帧格式，不做任何协议修改
- **D-08:** 鼠标移动合并仅在 SPSC 缓冲区层实现（写入端覆盖旧位置，读取端取最新值后发 DMOVM），协议层无感知

### 断连检测与恢复
- **D-09:** 依赖 Asio 异步读写错误返回检测断连，辅以 Asio steady_timer 超时
- **D-10:** 按键释放通过发送端状态表实现 — 维护所有"按下"状态的键，断连时遍历发送释放事件
- **D-11:** 断连后自动重连，使用指数退避策略（立即重试一次，之后 1s→2s→4s→最大 30s），重连成功后自动恢复输入流

### Claude's Discretion
- SPSC 队列的具体容量大小
- Asio 引入方式（FetchContent vs 系统安装）
- 桥接层的具体实现细节

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 网络层核心代码
- `src/lib/net/SocketMultiplexer.h` — 当前多路复用器接口，需要被替换的核心组件
- `src/lib/net/SocketMultiplexer.cpp` — 当前实现（317 行，手动 new/delete，cursor-mark sentinel）
- `src/lib/net/TCPSocket.h` / `src/lib/net/TCPSocket.cpp` — 当前 TCP socket 实现，需要适配 Asio
- `src/lib/net/IDataSocket.h` — socket 接口（继承 ISocket + IStream），上层依赖此接口
- `src/lib/net/ISocketMultiplexerJob.h` — 当前 job 接口，替换后可能移除
- `src/lib/net/TCPSocketFactory.h` — socket 工厂，需要重写以创建 Asio socket

### 协议与数据流
- `src/lib/deskflow/ProtocolUtil.h` — 协议工具，writef 格式化消息（协议格式不可变）
- `src/lib/server/Server.cpp` — 服务端核心，通过 EventQueue 接收网络事件
- `src/lib/server/ClientProxy1_8.cpp` — 客户端代理，通过 ProtocolUtil::writef 发送消息
- `src/lib/client/ServerProxy.cpp` — 客户端协议解析（852 行）

### 事件系统
- `src/lib/base/EventQueue.h` — 核心事件队列，Asio 桥接层需要投递到此
- `src/lib/base/EventTypes.h` — 事件类型定义

### 约束与担忧
- `.planning/codebase/CONCERNS.md` — SocketMultiplexer 无测试、手动内存管理、线程安全问题

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `IDataSocket` / `IStream` 接口: 上层代码通过 `IStream*` 读写数据，保留此接口可最小化上层改动
- `ProtocolUtil::writef`: 协议序列化工具，完全不需要修改
- `EventQueue`: 核心调度机制，Asio 桥接层只需投递事件到此队列

### Established Patterns
- 工厂模式: `ISocketFactory` → `TCPSocketFactory`，新的 Asio socket 实现需要通过工厂创建
- Job 模式: `ISocketMultiplexerJob` 的 `run(readable, writable, error)` 回调 — Asio 替代后不再需要此模式
- 事件驱动: 上层组件通过 `EventQueue::addHandler()` 注册事件处理器，新实现需保持此集成方式

### Integration Points
- `Client::connect()` — 客户端连接入口，创建 socket 并注册到多路复用器
- `ClientListener` — 服务端监听入口，接受连接后创建 ClientProxy
- `TCPSocket::newJob()` — socket 状态切换点（connecting → connected），需适配 Asio 异步操作
- `Server::onMouseMovePrimary()` → `ClientProxy::mouseMove()` — 鼠标事件发送路径，SPSC 缓冲区在此路径中插入

</code_context>

<specifics>
## Specific Ideas

- 重连成功后应自动恢复输入流，用户无需手动操作
- 鼠标合并的 200Hz 上限是网络发送频率，不影响本地输入采样率（1000Hz）

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 1-Network Pipeline Rewrite*
*Context gathered: 2026-05-12*
