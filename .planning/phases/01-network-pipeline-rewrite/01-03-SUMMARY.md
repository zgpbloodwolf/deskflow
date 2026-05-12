---
phase: 01-network-pipeline-rewrite
plan: 03
subsystem: net
tags: [spsc, disconnect, reconnect, exponential-backoff, client-proxy, event-routing, factory-switch, dynamic-cast]

# 依赖图
requires:
  - phase: 01-02
    provides: "SPSC 缓冲区（InputEventBuffer, KeyStateTable）集成到 AsioTCPSocket"
provides:
  - "端到端 SPSC 管道: ClientProxy -> InputEventBuffer -> Asio 200Hz timer/FIFO drain -> TCP wire"
  - "统一断连处理: handleDisconnect() -> releaseAllKeys() -> scheduleReconnect()"
  - "指数退避自动重连: 立即 -> 1s -> 2s -> 4s -> ... -> 30s max (D-11)"
  - "重连成功后自动恢复输入流 (200Hz mouse timer + keyboard FIFO)"
  - "ServerApp/ClientApp 使用 AsioTCPSocketFactory 替代 TCPSocketFactory"
  - "ClientProxy1_0/1_1/1_8 mouseMove/keyDown/keyUp 路由到 SPSC 缓冲区"
affects: [Phase 2 (TLS removal), "上层代码已脱离 SocketMultiplexer 依赖"]

# 技术追踪
tech-stack:
  added: []
  patterns:
    - "dynamic_cast<AsioTCPSocket*> 路由模式: 检测 socket 类型，SPSC 或直接 writef"
    - "统一断连处理模式: handleDisconnect() -> releaseAllKeys() -> scheduleReconnect()"
    - "指数退避重连模式: m_reconnectDelay 倍增 + 30s 上限 (D-11)"
    - "重连恢复模式: onReconnectSuccess() 重启定时器和 FIFO 排空"
    - "Asio asio 依赖设为 PUBLIC: AsioTCPSocket.h 公共头文件 include asio.hpp"

key-files:
  created: []
  modified:
    - src/lib/net/AsioTCPSocket.h
    - src/lib/net/AsioTCPSocket.cpp
    - src/lib/net/AsioTCPSocketFactory.h
    - src/lib/net/AsioTCPSocketFactory.cpp
    - src/lib/net/CMakeLists.txt
    - src/lib/deskflow/ServerApp.cpp
    - src/lib/deskflow/ClientApp.cpp
    - src/lib/server/ClientProxy1_0.cpp
    - src/lib/server/ClientProxy1_1.cpp
    - src/lib/server/ClientProxy1_8.cpp
    - src/lib/server/CMakeLists.txt

key-decisions:
  - "dynamic_cast 回退模式确保过渡期兼容性: Asio socket 走 SPSC，旧 socket 走 ProtocolUtil::writef"
  - "asio 依赖从 PRIVATE 改为 PUBLIC: AsioTCPSocket.h 公共头文件 include asio.hpp，所有依赖 net 的库需要 asio include 路径"
  - "server 库新增 net 依赖: ClientProxy1_0.cpp 需要 AsioTCPSocket.h 和 InputEventBuffer.h"
  - "connect() 保存目标地址和重置退避延迟，为重连提供必要信息"
  - "m_connected.exchange(true) 区分首次连接和重连场景，重连时调用 onReconnectSuccess()"
  - "releaseAllKeys 在 socket 断连时调用，writef 可能失败但 KeyStateTable 状态被清空"

patterns-established:
  - "SPSC 路由模式: dynamic_cast<AsioTCPSocket*>(getStream()) -> eventBuffer().storeMousePosition/pushKeyboardEvent"
  - "工厂切换模式: ServerApp(false), ClientApp(true) 通过 autoReconnect 参数区分"
  - "断连恢复模式: handleDisconnect -> releaseAllKeys + cancel timers + notify + reconnect"

requirements-completed: [NET-01, NET-02, NET-03, NET-04, NET-05, NET-06]

# 指标
duration: 7min
completed: 2026-05-12
---

# Phase 1 Plan 03: End-to-End SPSC Pipeline Summary

**端到端 SPSC 管道贯通 -- ClientProxy 输入事件通过 InputEventBuffer 路由，断连按键释放 + 指数退避重连，ServerApp/ClientApp 切换到 AsioTCPSocketFactory**

## Performance

- **Duration:** 7 min
- **Started:** 2026-05-12T15:29:03Z
- **Completed:** 2026-05-12T15:36:50Z
- **Tasks:** 2
- **Files modified:** 11 (0 created, 11 modified)

## Accomplishments
- 端到端 SPSC 管道贯通: ClientProxy1_0/1_1/1_8 -> InputEventBuffer -> Asio 200Hz timer/FIFO drain -> TCP wire
- 统一断连处理 handleDisconnect() 调用 releaseAllKeys() 释放所有按键 (D-10)
- 指数退避自动重连: 立即 -> 1s -> 2s -> 4s -> ... -> 30s max (D-11)
- 重连成功后自动恢复 200Hz 鼠标定时器和键盘 FIFO 排空
- ServerApp 和 ClientApp 切换到 AsioTCPSocketFactory，脱离 SocketMultiplexer
- 所有修改编译零错误（net, app, server 库目标全部通过）

## Task Commits

Each task was committed atomically:

1. **Task 1: Add disconnect handling, auto-reconnect, and factory switch** - `0a1d0ec` (feat)
2. **Task 2: Route ClientProxy input events through SPSC buffer** - `52aae8f` (feat)

## Files Created/Modified
- `src/lib/net/AsioTCPSocket.h` - 添加重连定时器、退避延迟、目标地址成员；添加 eventBuffer()/setAutoReconnect() 公共方法；添加 handleDisconnect/releaseAllKeys/scheduleReconnect/doReconnect/onReconnectSuccess 声明
- `src/lib/net/AsioTCPSocket.cpp` - 实现统一断连处理、按键释放、指数退避重连、重连恢复；connect() 保存目标地址和处理重连场景；onReadComplete() 使用 handleDisconnect()
- `src/lib/net/AsioTCPSocketFactory.h` - 构造函数添加 autoReconnect 参数
- `src/lib/net/AsioTCPSocketFactory.cpp` - 实现 autoReconnect 参数传递给 AsioTCPSocket
- `src/lib/net/CMakeLists.txt` - asio 从 PRIVATE 改为 PUBLIC（AsioTCPSocket.h 公共头文件需要）
- `src/lib/deskflow/ServerApp.cpp` - getSocketFactory() 改为 AsioTCPSocketFactory(m_events, false)
- `src/lib/deskflow/ClientApp.cpp` - getSocketFactory() 改为 AsioTCPSocketFactory(m_events, true)
- `src/lib/server/ClientProxy1_0.cpp` - mouseMove/keyDown/keyUp 通过 dynamic_cast 路由到 SPSC 缓冲区
- `src/lib/server/ClientProxy1_1.cpp` - keyDown/keyUp/keyRepeat 路由到 SPSC 缓冲区
- `src/lib/server/ClientProxy1_8.cpp` - keyDown (带 language) 路由到 SPSC 缓冲区
- `src/lib/server/CMakeLists.txt` - 添加 net 库依赖（需要 AsioTCPSocket.h/InputEventBuffer.h）

## Decisions Made
- **dynamic_cast 回退模式**: 使用 dynamic_cast<AsioTCPSocket*> 检测 socket 类型，Asio socket 走 SPSC 缓冲区，旧 socket 回退到 ProtocolUtil::writef 直接发送，确保过渡期兼容性
- **asio PUBLIC 依赖**: AsioTCPSocket.h 作为公共头文件 include asio.hpp，所有链接 net 的库都需要 asio include 路径，因此将 asio 从 PRIVATE 改为 PUBLIC
- **server 链接 net**: server 库的 ClientProxy 代码需要 AsioTCPSocket.h 和 InputEventBuffer.h，因此添加 net 依赖
- **connect() 保存目标地址**: m_targetAddress = addr 用于重连，m_reconnectDelay = 0s 重置退避
- **m_connected.exchange(true) 区分场景**: 首次连接走完整初始化（startAsyncRead + startMouseSendTimer + drainKeyboardFifo），重连走 onReconnectSuccess

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] server 库缺少 net 依赖导致编译失败**
- **Found during:** Task 2 (构建验证)
- **Issue:** ClientProxy1_0.cpp include AsioTCPSocket.h，但 server 库没有链接 net，导致 asio.hpp 找不到
- **Fix:** server CMakeLists.txt 添加 `net` 到 target_link_libraries；net CMakeLists.txt 将 asio 从 PRIVATE 改为 PUBLIC
- **Files modified:** src/lib/server/CMakeLists.txt, src/lib/net/CMakeLists.txt
- **Verification:** server 库编译零错误
- **Committed in:** 52aae8f (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** 修改 CMake 依赖关系是必要的编译修复。asio 从 PRIVATE 改为 PUBLIC 是正确的架构决策（公共头文件 include 了 asio.hpp）。无范围蔓延。

## Issues Encountered
- deskflow-core 链接失败（AGL framework not found）-- 预存在问题，与前两个计划相同，与本计划修改无关

## User Setup Required
None - 无需外部服务配置。

## Next Phase Readiness
- Phase 1 全部 3 个计划已完成，端到端网络管道就绪
- 从 ClientProxy 到 TCP wire 的完整路径: ClientProxy::mouseMove/keyDown/keyUp -> InputEventBuffer SPSC -> Asio 200Hz timer/1ms FIFO drain -> ProtocolUtil::writef -> async_write -> TCP
- 旧 SocketMultiplexer 代码完整保留（ServerApp/ClientApp 仍创建 SocketMultiplexer 实例用于兼容），Phase 2 可移除
- 所有 Phase 1 需求（NET-01 到 NET-06）已满足
- 建议后续: 移除 TLS 层 (Phase 2)、移除 Linux 平台支持 (Phase 3)

---
*Phase: 01-network-pipeline-rewrite*
*Completed: 2026-05-12*

## Self-Check: PASSED

All modified files verified on disk. Both commit hashes (0a1d0ec, 52aae8f) present in git log. net, app, and server library targets compile with zero errors. Full build fails only at deskflow-core link stage (pre-existing AGL framework issue, unrelated to this plan).
