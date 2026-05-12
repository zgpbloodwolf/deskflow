---
phase: 01-network-pipeline-rewrite
plan: 01
subsystem: net
tags: [asio, tcp, async-io, fetch-content, cmake, socket]

# 依赖图
requires:
  - phase: none
    provides: "初始状态，无前置依赖"
provides:
  - "AsioTCPSocket 实现 IDataSocket 接口（异步 connect/read/write/flush）"
  - "AsioTCPListenSocket 实现 IListenSocket 接口（异步 accept）"
  - "AsioTCPSocketFactory 实现 ISocketFactory 接口"
  - "Standalone Asio 1.38.0 通过 vendor 目录集成到构建系统"
affects: [02-network-pipeline-rewrite, "上层代码切换到 AsioTCPSocketFactory"]

# 技术追踪
tech-stack:
  added: [standalone-asio-1.38.0]
  patterns:
    - "Per-connection io_context 线程模型 (D-04)"
    - "Asio strand 序列化异步操作"
    - "Asio 完成回调桥接到 EventQueue (D-06)"
    - "work_guard 保持 io_context::run() 持续运行"
    - "shared_from_this() 管理 socket 生命周期"

key-files:
  created:
    - cmake/FetchAsio.cmake
    - src/lib/net/AsioTCPSocket.h
    - src/lib/net/AsioTCPSocket.cpp
    - src/lib/net/AsioTCPListenSocket.h
    - src/lib/net/AsioTCPListenSocket.cpp
    - src/lib/net/AsioTCPSocketFactory.h
    - src/lib/net/AsioTCPSocketFactory.cpp
  modified:
    - CMakeLists.txt
    - src/lib/net/CMakeLists.txt

key-decisions:
  - "使用 vendor 目录存放 Asio 源码，优先于 FetchContent 网络下载（网络不可靠）"
  - "AsioTCPSocket 继承 enable_shared_from_this 管理 async 回调中的 this 指针生命周期"
  - "flush() 使用 condition_variable + atomic<bool> 同步等待（避免 deadlock）"
  - "AsioTCPListenSocket 的 accept() 使用 m_pendingSocket 暂存已接受连接"

patterns-established:
  - "Asio async 操作模式: strand 序列化 + shared_from_this 生命周期 + EventQueue 桥接"
  - "构建集成模式: vendor 本地源码优先，FetchContent 网络下载作为备选"
  - "Deskflow SPDX 文件头: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception"

requirements-completed: [NET-01, NET-05, NET-06]

# 指标
duration: 41min
completed: 2026-05-12
---

# Phase 1 Plan 01: Asio Socket Infrastructure Summary

**Standalone Asio 1.38.0 异步 TCP socket 基础设施 -- 每连接独立 io_context 线程，EventQueue 桥接，零旧代码修改**

## Performance

- **Duration:** 41 min
- **Started:** 2026-05-12T14:34:16Z
- **Completed:** 2026-05-12T15:15:20Z
- **Tasks:** 2
- **Files modified:** 9 (7 created, 2 modified)

## Accomplishments
- Standalone Asio 1.38.0 header-only 库成功集成到 CMake 构建系统
- AsioTCPSocket 实现完整的异步 connect/read/write/flush，每连接独立 io_context 线程
- AsioTCPListenSocket 实现异步 accept，新连接创建独立 AsioTCPSocket
- AsioTCPSocketFactory 创建 Asio 版 socket 实例，无需 SocketMultiplexer
- 构建零错误通过（net library target）
- 所有新类与现有 TCPSocket/SocketMultiplexer 并行共存，零修改旧文件

## Task Commits

Each task was committed atomically:

1. **Task 1: Add Standalone Asio via FetchContent and create class skeletons** - `20157e9` (feat)
2. **Task 2: Implement AsioTCPSocket, AsioTCPListenSocket, AsioTCPSocketFactory** - `6cfd2ae` (feat)

## Files Created/Modified
- `cmake/FetchAsio.cmake` - Standalone Asio 构建集成（vendor 本地优先，FetchContent 备选）
- `CMakeLists.txt` - 添加 `include(cmake/FetchAsio.cmake)`
- `src/lib/net/CMakeLists.txt` - 添加 6 个新源文件和 asio 链接依赖
- `src/lib/net/AsioTCPSocket.h` - Asio TCP 数据 socket 头文件（io_context/strand/work_guard）
- `src/lib/net/AsioTCPSocket.cpp` - 异步 connect/read/write/flush 完整实现
- `src/lib/net/AsioTCPListenSocket.h` - Asio TCP 监听 socket 头文件（acceptor）
- `src/lib/net/AsioTCPListenSocket.cpp` - 异步 accept 实现
- `src/lib/net/AsioTCPSocketFactory.h` - socket 工厂头文件
- `src/lib/net/AsioTCPSocketFactory.cpp` - 工厂实现，创建 Asio socket 实例

## Decisions Made
- **vendor 目录本地源码优先**: GitHub 网络不可靠（中国大陆），改用 vendor 目录存放预下载的 Asio 源码，FetchContent 网络下载作为备选
- **enable_shared_from_this**: AsioTCPSocket 继承 enable_shared_from_this 以安全地在异步回调中持有 this 指针
- **condition_variable for flush()**: flush() 使用 condition_variable + atomic<bool> 替代 CondVar<bool>（不依赖 mt 库），带 30 秒超时防死锁
- **pendingSocket 模式**: AsioTCPListenSocket 使用 m_pendingSocket 暂存 async_accept 的结果，accept() 方法同步返回

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] GitHub 网络不可达导致 FetchContent 失败**
- **Found during:** Task 2 (构建验证)
- **Issue:** GitHub 克隆超时（中国大陆网络），FetchContent 无法下载 Asio 源码
- **Fix:** 通过 ghproxy 镜像下载 Asio 源码到 vendor 目录，修改 FetchAsio.cmake 优先使用本地源码
- **Files modified:** cmake/FetchAsio.cmake, vendor/asio-asio-1-38-0/
- **Verification:** cmake configure 成功显示 "使用本地 Asio 源码"
- **Committed in:** 6cfd2ae (Task 2 commit)

**2. [Rule 1 - Bug] AsioTCPSocket.h 缺少 Event.h include**
- **Found during:** Task 2 (实现)
- **Issue:** sendEvent() 使用 EventTypes 类型但头文件未包含 Event.h，会导致编译错误
- **Fix:** 在 AsioTCPSocket.h 中添加 `#include "base/Event.h"`
- **Files modified:** src/lib/net/AsioTCPSocket.h
- **Verification:** net library 编译零错误
- **Committed in:** 6cfd2ae (Task 2 commit)

**3. [Rule 1 - Bug] AsioTCPSocketFactory.cpp 缺少 Log.h include**
- **Found during:** Task 2 (构建)
- **Issue:** 使用 LOG_DEBUG 但未包含 "base/Log.h"
- **Fix:** 添加 `#include "base/Log.h"`
- **Files modified:** src/lib/net/AsioTCPSocketFactory.cpp
- **Verification:** 编译零错误
- **Committed in:** 6cfd2ae (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (1 blocking, 2 bug)
**Impact on plan:** 所有偏差都是必要的修复。vendor 目录方案提供了更好的离线构建支持。无范围蔓延。

## Issues Encountered
- deskflow-core 链接失败（AGL framework not found）-- 这是预存在问题，与本计划修改无关
- Qt6 需要手动安装（brew install qt@6）-- 开发环境配置问题

## User Setup Required
None - 无需外部服务配置。

## Next Phase Readiness
- AsioTCPSocket 基础设施就绪，Plan 02 可以上层代码（Client/Server）切换到使用 AsioTCPSocketFactory
- 需要注意：AsioTCPSocket 的 accepted socket 构造函数中，socket 从 acceptor 的 io_context 移动到新 socket 的 io_context，可能需要验证跨 io_context 移动的正确性
- vendor/asio-asio-1-38-0/ 目录已提交，团队其他成员无需网络下载

---
*Phase: 01-network-pipeline-rewrite*
*Completed: 2026-05-12*

## Self-Check: PASSED

All 7 created files verified on disk. Both commit hashes (20157e9, 6cfd2ae) present in git log. All 3 Asio object files compiled successfully.
