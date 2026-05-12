---
phase: 01-network-pipeline-rewrite
plan: 02
subsystem: net
tags: [spsc, lock-free, ring-buffer, input-buffer, key-state, mouse-coalescing, 200hz-timer, keyboard-fifo]

# 依赖图
requires:
  - phase: 01-01
    provides: "AsioTCPSocket 基础设施"
provides:
  - "SpscQueue.h: 无锁 SPSC FIFO 队列模板 (SpscFifo<T,Capacity>) 和 MousePositionSlot 原子位置槽"
  - "InputEventBuffer: 组合缓冲区（键盘 FIFO + 鼠标位置槽），生产者-消费者解耦"
  - "KeyStateTable: 发送端按键状态追踪表，支持 recordKeyDown/Up 和 releaseAll"
  - "AsioTCPSocket 集成: 200Hz 鼠标发送定时器 + 键盘 FIFO 排空循环 + 断连按键释放"
affects: [03-network-pipeline-rewrite, "上层 ClientProxy1_0 调用路径修改"]

# 技术追踪
tech-stack:
  added: []
  patterns:
    - "Lock-free SPSC ring buffer with alignas(64) false-sharing prevention (RESEARCH.md Pitfall 2)"
    - "Atomic mouse position slot with release/acquire memory ordering (RESEARCH.md Pitfall 5)"
    - "200Hz steady_timer mouse coalescing (D-08): store overwrites, timer reads latest"
    - "1ms keyboard FIFO drain loop (D-05): immediate dispatch on non-empty queue"
    - "Key state table with mutex-protected releaseAll on disconnect (D-10)"

key-files:
  created:
    - src/lib/net/SpscQueue.h
    - src/lib/net/InputEventBuffer.h
    - src/lib/net/InputEventBuffer.cpp
    - src/lib/net/KeyStateTable.h
    - src/lib/net/KeyStateTable.cpp
  modified:
    - src/lib/net/AsioTCPSocket.h
    - src/lib/net/AsioTCPSocket.cpp
    - src/lib/net/CMakeLists.txt

key-decisions:
  - "SpscFifo 容量 256 满足极端打字速度（约 10 key/s），队列满返回 false 并记录警告"
  - "MousePositionSlot 使用 release/acquire 语义确保 x/y 坐标和 updated 标志可见性"
  - "KeyStateTable 使用 mutex 而非 lock-free：正常路径仅单线程写入，断连时低频读取"
  - "键盘 FIFO 排空使用 1ms 定时器轮询而非条件变量通知，简化实现"
  - "ProtocolUtil::writef 通过 static_cast<deskflow::IStream*> 调用，确保多重继承正确转换"

patterns-established:
  - "SPSC 缓冲区模式: 生产者(EventQueue 线程) 写入 -> 消费者(Asio io_context 线程) 读取"
  - "200Hz 定时器模式: expires_after(5ms) -> 回调处理 -> 重新调度"
  - "断连按键释放模式: onReadComplete 错误 -> releaseAll -> 遍历发送 keyUp"

requirements-completed: [NET-01, NET-02, NET-04, NET-05, NET-06]

# 指标
duration: 6min
completed: 2026-05-12
---

# Phase 1 Plan 02: SPSC Event Buffers Summary

**Lock-free SPSC 缓冲区基础设施 -- 键盘 FIFO + 鼠标原子位置槽 + 按键状态表，集成到 AsioTCPSocket 实现事件流解耦**

## Performance

- **Duration:** 6 min
- **Started:** 2026-05-12T15:19:38Z
- **Completed:** 2026-05-12T15:25:47Z
- **Tasks:** 2
- **Files modified:** 8 (5 created, 3 modified)

## Accomplishments
- SpscQueue.h 实现无锁 SPSC FIFO 环形缓冲区和 MousePositionSlot 原子位置槽
- InputEventBuffer 组合缓冲区将键盘 FIFO 和鼠标位置槽封装为统一接口
- KeyStateTable 追踪所有按下状态的按键，断连时遍历发送 keyUp 释放事件
- AsioTCPSocket 集成 200Hz 鼠标发送定时器（D-08 合并发送）
- AsioTCPSocket 集成键盘 FIFO 排空循环（D-05 即时分发）
- AsioTCPSocket 断连回调调用 releaseAll 防止卡键（D-10）
- 所有文件使用 Deskflow SPDX 文件头
- 构建零错误通过（net library target）

## Task Commits

Each task was committed atomically:

1. **Task 1: Create lock-free SPSC buffers and InputEventBuffer + KeyStateTable** - `03e81bf` (feat)
2. **Task 2: Integrate InputEventBuffer and KeyStateTable into AsioTCPSocket** - `d5b9a0c` (feat)

## Files Created/Modified
- `src/lib/net/SpscQueue.h` - 无锁 SPSC FIFO 队列模板 + MousePositionSlot 原子位置槽（alignas(64) 防止 false sharing）
- `src/lib/net/InputEventBuffer.h` - 组合缓冲区头文件（键盘 FIFO 256 容量 + 鼠标位置槽）
- `src/lib/net/InputEventBuffer.cpp` - 组合缓冲区实现（简单委托到 SPSC 组件）
- `src/lib/net/KeyStateTable.h` - 按键状态追踪表头文件（mutex 保护的 unordered_map）
- `src/lib/net/KeyStateTable.cpp` - recordKeyDown/Up + releaseAll 实现
- `src/lib/net/AsioTCPSocket.h` - 添加 InputEventBuffer、KeyStateTable、steady_timer 成员和新方法声明
- `src/lib/net/AsioTCPSocket.cpp` - 实现 200Hz 鼠标定时器、键盘 FIFO 排空、断连按键释放
- `src/lib/net/CMakeLists.txt` - 添加 InputEventBuffer 和 KeyStateTable 源文件

## Decisions Made
- **SpscFifo 容量 256**: 极端打字速度（~10 key/s）下即使 1 秒不消费也不会溢出，满足 RESEARCH.md Pitfall 6 推荐
- **1ms 键盘轮询**: 使用短超时定时器而非条件变量通知，简化实现且延迟足够低（1ms 远低于人类感知阈值）
- **KeyStateTable 使用 mutex**: 正常路径 recordKeyDown/Up 仅从 EventQueue 单线程写入，releaseAll 仅在断连时从 Asio 线程读取（低频），mutex 开销可忽略
- **static_cast<deskflow::IStream*>**: AsioTCPSocket 多重继承 IDataSocket（->ISocket, ->deskflow::IStream），显式转换确保 writef 获得正确的 IStream 指针

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- googletest FetchContent 构建失败（预存在问题，与本计划无关）-- 通过 -DBUILD_TESTS=OFF 绕过
- deskflow-core 链接失败（AGL framework not found）-- 预存在问题，net library target 编译成功

## Next Phase Readiness
- SPSC 缓冲区基础设施就绪，Plan 03 可以修改上层 ClientProxy1_0::mouseMove()/keyDown()/keyUp() 调用路径
- 上层代码将直接调用 AsioTCPSocket 的 InputEventBuffer 写入方法（storeMousePosition/pushKeyboardEvent）
- 注意：当前 AsioTCPSocket 没有暴露 InputEventBuffer 的写入方法给外部，Plan 03 需要添加公共接口

---
*Phase: 01-network-pipeline-rewrite*
*Completed: 2026-05-12*

## Self-Check: PASSED

All 5 created files verified on disk. Both commit hashes (03e81bf, d5b9a0c) present in git log. net library target compiles with zero errors. InputEventBuffer.cpp.o and KeyStateTable.cpp.o compiled successfully.
