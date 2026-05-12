# Walking Skeleton -- Deskflow Network Pipeline Rewrite

**Phase:** 1
**Generated:** 2026-05-12

## Capability Proven End-to-End

Asio 异步 TCP socket 可以通过工厂创建、建立连接、双向传输数据、检测断连并自动重连，鼠标/键盘事件通过 lock-free SPSC 缓冲区从 EventQueue 线程流向 Asio I/O 线程，上层 Server/Client 逻辑零修改。

## Architectural Decisions

| Decision | Choice | Rationale |
|---|---|---|
| 网络 I/O 库 | Standalone Asio 1.38.0 (header-only) | D-01 锁定。跨平台异步 I/O（macOS kqueue / Windows IOCP），header-only 无链接负担，Boost Software License 商业友好 |
| 引入方式 | CMake FetchContent | 构建时自动获取，无需每个开发环境手动安装。备选：预下载到 vendor/ 目录 |
| I/O 模型 | Per-connection 单线程（每连接独立 io_context） | D-04 锁定。消除全局轮询瓶颈，连接间互不阻塞 |
| 事件缓冲 | Lock-free SPSC（键盘 FIFO + 鼠标 atomic 最新位置槽） | D-05 锁定。生产者-消费者解耦，zero-lock 竞争。手写 ~50 行，无需第三方 lock-free 库 |
| 鼠标合并 | 200Hz steady_timer 读取最新位置，覆盖旧值 | D-08 锁定。1000Hz 输入 -> 200Hz 发送，5x 带宽节省，鼠标仍跟手 |
| EventQueue 集成 | Asio 完成回调通过 addEvent 桥接 | D-06 锁定。上层 Server/Client 逻辑不变，Asio 仅在独立线程处理 I/O |
| 断连检测 | Asio async read/write 错误返回 + steady_timer 超时 | D-09 锁定。无需自定义心跳机制 |
| 按键释放 | 发送端状态表（KeyStateTable），断连时遍历发送 keyUp | D-10 锁定。防止卡键 |
| 自动重连 | 指数退避（立即 -> 1s -> 2s -> 4s -> ... -> 30s max） | D-11 锁定。重连成功后自动恢复输入流 |
| 协议兼容 | 完全兼容现有 ProtocolUtil::writef 二进制帧格式 | D-07 锁定。零协议修改 |
| 线程安全 | strand 序列化 + mutex（仅 KeyStateTable 低频操作） | strand 保证同一 socket 的异步回调不并发。KeyStateTable 仅断连时跨线程访问 |

## Stack Touched in Phase 1

- [x] Asio 集成到 CMake 构建（FetchContent + interface library）
- [x] AsioTCPSocket 实现 IDataSocket（per-connection io_context 线程）
- [x] AsioTCPListenSocket 实现 IListenSocket（async_accept）
- [x] AsioTCPSocketFactory 实现 ISocketFactory
- [x] Lock-free SPSC 缓冲区（SpscFifo + MousePositionSlot）
- [x] InputEventBuffer 组合键盘 FIFO + 鼠标位置槽
- [x] KeyStateTable 按键状态追踪 + releaseAllKeys
- [x] 200Hz 鼠标发送定时器
- [x] 键盘 FIFO 消费 + 发送
- [x] 断连处理 + releaseAllKeys
- [x] 自动重连（指数退避）
- [x] ServerApp/ClientApp 切换到 AsioTCPSocketFactory

## Out of Scope (Deferred to Later Phases)

- TLS/SecureSocket 移除 -- Phase 2
- TCPSocket/SocketMultiplexer 代码删除 -- Phase 2（TLS 移除后清理）
- Linux 平台移除 -- Phase 3
- 智能指针替换（new/delete -> shared_ptr/unique_ptr）-- Phase 4
- 自定义 Thread -> std::jthread -- Phase 4
- reinterpret_cast cursor-mark 消除 -- Phase 4
- exit(0) -> 优雅退出 -- Phase 4
- GUI 回归测试 -- Phase 5
- 跨平台构建验证 -- Phase 5

## Subsequent Slice Plan

每个后续阶段在本骨架之上添加一个垂直切片，不改变其架构决策：

- Phase 2: 移除 TLS/SSL 加密层，删除 SecureSocket 继承链和所有 SecurityLevel 分发代码
- Phase 3: 移除 Linux 平台支持（44 个文件）和屏幕保护联动，清理残留代码
- Phase 4: C++ 现代化（智能指针、std::jthread、安全迭代、优雅退出）
- Phase 5: 双平台构建验证 + 测试 + GUI 回归

## Project Structure (New Files)

```
src/lib/net/
  AsioTCPSocket.h           -- Asio TCP socket (implements IDataSocket)
  AsioTCPSocket.cpp
  AsioTCPListenSocket.h      -- Asio listen socket (implements IListenSocket)
  AsioTCPListenSocket.cpp
  AsioTCPSocketFactory.h     -- Factory (implements ISocketFactory)
  AsioTCPSocketFactory.cpp
  SpscQueue.h                -- Lock-free SPSC FIFO + MousePositionSlot (header-only)
  InputEventBuffer.h         -- Combined keyboard FIFO + mouse position slot
  InputEventBuffer.cpp
  KeyStateTable.h            -- Key press state tracker
  KeyStateTable.cpp
cmake/
  FetchAsio.cmake            -- FetchContent for Standalone Asio 1.38.0
```
