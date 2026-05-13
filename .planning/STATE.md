# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-05-12)

**Core value:** 流畅的局域网键盘鼠标共享体验 -- 网络波动时不能卡顿，鼠标移动必须跟手
**Current focus:** Phase 5: Build Validation & Regression

## Current Position

Phase: 5 of 5 (Build Validation & Regression)
Plan: 1 of 3 in current phase
Status: Plan 05-01 complete
Last activity: 2026-05-13 -- Plan 05-01 complete (macOS build + tests)

Progress: [██████████] 67%

## Performance Metrics

**Velocity:**
- Total plans completed: 10
- Average duration: 12 min
- Total execution time: 1.9 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 3 | 54min | 18min |
| 02 | 3 | ~15min | ~5min |
| 03 | 3 | ~37min | ~12min |
| 05 | 1 | 22min | 22min |

**Recent Trend:**
- Last 5 plans: 05-01 (22min), 04-04 (4min), 04-03 (5min), 04-02 (7min), 04-01 (4min)
- Trend: Build validation plans take longer due to full rebuild + test cycles

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: Network pipeline rewrite is Phase 1 (core value, highest priority)
- [Roadmap]: TLS removal before Linux removal (TLS touches network path, simpler to test)
- [Roadmap]: Modernization last (smart pointer migration of SocketMultiplexer is dangerous during architecture changes)
- [Roadmap]: BQ requirements in dedicated validation phase (cross-cutting, not assignable to single change)
- [01-01]: vendor 目录本地源码优先于 FetchContent 网络下载（中国网络环境）
- [01-01]: AsioTCPSocket 使用 enable_shared_from_this 管理异步回调生命周期
- [01-01]: flush() 使用 condition_variable + atomic 同步等待
- [01-02]: SpscFifo 容量 256 满足极端打字速度，队列满返回 false 不丢弃
- [01-02]: KeyStateTable 使用 mutex（正常路径单线程写，断连低频读）
- [01-02]: 键盘 FIFO 排空使用 1ms 定时器轮询简化实现
- [01-03]: dynamic_cast 回退模式确保过渡期 Asio/旧 socket 兼容性
- [01-03]: asio 依赖从 PRIVATE 改为 PUBLIC（公共头文件 include asio.hpp）
- [01-03]: 指数退避重连立即重试一次，之后 1s->2s->4s->...->30s max

### Pending Todos

None yet.

### Blockers/Concerns

- [Phase 1 RESOLVED]: Full Asio adoption selected -- AsioTCPSocket/ListenSocket/Factory implemented
- [Phase 1 RESOLVED]: SPSC queue implemented with alignas(64), MSVC validation pending integration testing
- [Phase 2 RESOLVED]: SecurityLevel dispatch graph fully traced — 6+ dispatch points, 22 files deleted, 30+ call sites modified
- [Phase 3 RESOLVED]: 65 Linux files deleted, screensaver sync fully removed, all #ifdef dead code paths cleaned
- [Phase 3 BUG]: arch/unix and deskflow/unix wrongly deleted -- macOS needs POSIX/BSD impls, restored in 05-01
- [05-01]: FetchContent googletest changed to vendor-local SOURCE_DIR (Chinese network)

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-05-13
Stopped at: Plan 05-01 complete (macOS build + tests pass)
Resume file: None
