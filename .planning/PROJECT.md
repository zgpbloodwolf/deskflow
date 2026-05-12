# Deskflow (个人精简版)

## What This Is

基于 Deskflow 分支的键盘鼠标共享工具，精简为 macOS + Windows 双平台个人高效使用。移除不必要的平台和功能，修复局域网场景下的网络卡顿问题，提升响应速度和稳定性。

## Core Value

**流畅的局域网键盘鼠标共享体验。** 网络波动时不能卡顿，鼠标移动必须跟手。

## Requirements

### Validated

- ✓ 跨平台键盘鼠标共享（macOS ↔ Windows） — 现有代码
- ✓ 剪贴板文本/图片共享 — 现有代码
- ✓ 跨屏文件拖拽传输 — 现有代码
- ✓ Qt GUI 配置界面 — 现有代码
- ✓ 屏幕保护联动 — 现有代码
- ✓ CMake 构建系统 — 现有代码
- ✓ 客户端/服务器模式（一台主机控制其他电脑） — 现有代码

### Active

- [ ] 修复网络波动时的持续卡顿/掉帧问题
- [ ] 移除 Linux 平台支持（X11/Wayland/EI）
- [ ] 移除 TLS 加密层（局域网不需要加密）
- [ ] 移除 deskflow-daemon（Windows 服务模式，非必需）
- [ ] 精简网络层，减少延迟
- [ ] 现代化内存管理（智能指针替代 new/delete）

### Out of Scope

- Linux 平台支持 — 只用 macOS + Windows
- TLS/SSL 加密 — 局域网内不需要
- Wayland/EI 输入协议 — Linux 相关，不需要
- 多语言翻译维护 — 暂时冻结
- 商业化功能（许可证、激活） — 个人开源项目

## Context

**技术栈：** C++20, Qt 6, CMake 3.24+, 跨平台（macOS/Windows/Linux）

**架构：** 分层架构 — GUI 可执行文件 → 应用层 → 客户端/服务器/平台层 → 基础设施层（net/io/mt/base/arch）

**关键代码位置：**
- 网络层：`src/lib/net/` — SocketMultiplexer 阻塞 I/O，可能是卡顿根源
- 平台层：`src/lib/platform/` — 各平台屏幕/输入实现
- GUI：`src/lib/gui/` — Qt Widgets 界面
- 服务器：`src/lib/server/` — 服务端逻辑（Config.cpp 2074 行，Server.cpp 2071 行）
- 客户端：`src/lib/client/` — 客户端逻辑

**已知问题：**
- SocketMultiplexer 使用手动 new/delete，有内存泄漏和 use-after-free 风险
- 阻塞 I/O 模型导致网络波动时输入处理线程被阻塞
- 92 个 TODO/FIXME 标记散布在代码中
- 多个巨型单文件（2000+ 行），混合多种职责
- TLS 证书验证被绕过（即将移除整个 TLS 层）
- `exit(0)` 直接调用导致资源泄漏
- 旧版测试框架遗留

## Constraints

- **平台**: 仅 macOS + Windows — 移除所有 Linux/X11/Wayland 代码
- **语言**: C++20 — 使用现代 C++ 特性（智能指针、std::format 等）
- **构建**: CMake — 保持现有构建系统
- **GUI**: Qt 6 — 保持现有 GUI 框架
- **网络**: 仅局域网 — 不需要加密，需要低延迟和高抗抖动
- **开源**: MIT 协议 — 保持现有许可证

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| 移除 TLS 加密层 | 仅局域网使用，不需要加密；减少约 3000+ 行代码 | — Pending |
| 移除 Linux 平台 | 只用 macOS + Windows；减少约 5000+ 行平台代码 | — Pending |
| 保留 GUI | 需要图形配置界面 | — Pending |
| 保留剪贴板和文件拖拽 | 这些是高频使用功能 | — Pending |
| 修复网络卡顿为最高优先级 | Core Value 是流畅体验，卡顿问题直接影响使用 | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-12 after initialization*
