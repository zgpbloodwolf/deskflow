# Phase 3: Platform & Feature Cleanup - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-13
**Phase:** 3-Platform & Feature Cleanup
**Areas discussed:** 屏幕保护移除深度, 协议消息 kMsgCScreenSaver 处理, CMake 重构范围, Server::enter() screensaver 参数

---

## 屏幕保护移除深度

| Option | Description | Selected |
|--------|-------------|----------|
| 完全移除 | 删除 IScreenSaver 接口、协议消息、Server 事件处理、平台实现类、EventTypes 事件 | ✓ |
| 仅移除同步逻辑 | 删除协议和 Server 调用，保留 IScreenSaver 接口和平台类 | |

**User's choice:** 完全移除
**Notes:** 成功标准明确要求"完全移除无残留"

---

## 协议消息 kMsgCScreenSaver 处理

| Option | Description | Selected |
|--------|-------------|----------|
| 完全删除 | 删除消息常量、发送端、接收端分支 | ✓ |
| 接收端 no-op | 保留消息常量和接收端，但忽略 | |
| 两端 no-op | 保留消息常量和两端代码，不再触发 | |

**User's choice:** 完全删除
**Notes:** 个人精简版不需要与旧版 Barrier/Deskflow 互操作。会破坏上游协议兼容性，但个人使用无需顾虑。

---

## CMake 重构范围

| Option | Description | Selected |
|--------|-------------|----------|
| 仅删除 Linux 块 | 删除 UNIX AND NOT APPLE / elseif(UNIX) 块，保留 WIN32/Apple 结构 | ✓ |
| 重构为双平台结构 | 删除 Linux 块后重构为更简洁的双平台结构 | |

**User's choice:** 仅删除 Linux 块
**Notes:** 9 个 CMakeLists.txt 文件有 Linux 条件块，最小改动最安全。

---

## Server::enter() screensaver 参数

| Option | Description | Selected |
|--------|-------------|----------|
| 删除参数和状态 | 删除 screensaver/forScreensaver 参数及相关状态变量 | ✓ |
| 保留参数硬编码 false | 接口不变但参数固定为 false | |

**User's choice:** 删除参数和状态
**Notes:** screensaver=true 仅在屏幕保护激活跳转主屏时传入，现在该场景完全不存在。影响 IClient::enter()、Server::switchScreen()、约 10 个调用点。

---

## Claude's Discretion

- Linux 文件删除顺序和分组方式
- #ifdef 守卫清理的边界判定
- 删除后的 include 清理方式
- 单元测试中 Linux 相关测试用例处理

## Deferred Ideas

- `deskflow-daemon` 目录处理 — PROJECT.md 标注"保留 Windows 服务模式（可能有用）"，需进一步确认
