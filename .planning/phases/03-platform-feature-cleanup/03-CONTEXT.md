# Phase 3: Platform & Feature Cleanup - Context

**Gathered:** 2026-05-13
**Status:** Ready for planning

<domain>
## Phase Boundary

移除 Linux 平台支持（X11/Wayland/EI，44 个文件）、屏幕保护联动功能（接口+协议+平台实现）、以及所有残留的 #ifdef 分支和死代码路径。使代码库只包含 macOS 和 Windows 平台代码。

**包含：**
- 删除 44 个 Linux 平台源文件（X11: 24, Wayland/Wl: 4, EI: 6, Portal: 4, XDG: 4, 其他: 2）
- 完全移除屏幕保护联动功能（IScreenSaver 接口、协议消息 kMsgCScreenSaver、平台实现类、Server 事件处理、EventTypes 事件）
- 清理所有 `#ifdef` Linux 相关守卫（Q_OS_LINUX, UNIX AND NOT APPLE 等）
- 更新所有 CMakeLists.txt，移除 Linux 编译块和依赖（libei, libportal, xkbcommon）
- 清理 IClient::enter() 的 screensaver 参数和相关状态变量

**不包含：**
- C++ 现代化（智能指针、jthread，Phase 4）
- Windows/macOS 平台代码修改（除非移除 screensaver 引用）
- GUI 功能修改（仅清理 Linux 条件编译）
- 协议格式修改（帧格式不变，仅删除 CSEC 消息类型）

</domain>

<decisions>
## Implementation Decisions

### 屏幕保护移除策略
- **D-01:** 完全移除所有屏幕保护代码 — IScreenSaver 接口、kMsgCScreenSaver 协议消息、Server 事件处理（PrimaryScreenSaverActivated/Deactivated）、平台实现类（OSXScreenSaver、MSWindowsScreenSaver）、EventTypes 中的屏幕保护事件类型

### 协议兼容性
- **D-02:** 完全删除 kMsgCScreenSaver 消息常量、ClientProxy1_0 发送端、ServerProxy 接收端分支。不保留上游 Barrier/Deskflow 兼容性（个人精简版不需要）

### CMake 重构范围
- **D-03:** 仅删除 Linux 相关的 CMake 块（UNIX AND NOT APPLE, elseif(UNIX), BUILD_X11_SUPPORT, LIBEI_FOUND 等），不重构现有的 WIN32/Apple if/elseif 结构。9 个 CMakeLists.txt 文件需要修改

### 接口变更
- **D-04:** 删除 IClient::enter() 的 screensaver bool 参数、Server::switchScreen() 的 forScreensaver 参数、以及 Server.cpp 中的 m_activeSaver/m_xSaver/m_ySaver 状态变量。所有调用点简化

### Claude's Discretion
- Linux 文件的具体删除顺序（可按文件类型批量删除）
- #ifdef 守卫清理的具体范围判定
- 删除后 include 清理的具体方式
- 单元测试中 Linux 相关测试用例的处理

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 平台层代码
- `src/lib/platform/CMakeLists.txt` — 三层 if/elseif 平台选择结构，需移除 elseif(UNIX) 整个块及顶部 libei/libportal pkg_check_modules
- `src/lib/platform/` — 44 个 Linux 文件需删除（XWindows*, Ei*, Wl*, Portal*, XDG*）

### 屏幕保护代码（全部移除）
- `src/lib/deskflow/IScreenSaver.h` — 屏幕保护接口定义
- `src/lib/deskflow/ProtocolTypes.h:434` — kMsgCScreenSaver 消息声明
- `src/lib/deskflow/ProtocolTypes.cpp:21` — kMsgCScreenSaver 消息定义
- `src/lib/deskflow/IPlatformScreen.h:110` — screensaver() 纯虚方法
- `src/lib/deskflow/PlatformScreen.h:75` — screensaver() override
- `src/lib/deskflow/Screen.h:105` — Screen::screensaver() 方法
- `src/lib/deskflow/Screen.cpp:217` — Screen::screensaver() 实现
- `src/lib/deskflow/IClient.h:131` — IClient::screensaver() 接口
- `src/lib/server/Server.cpp:94,98,156,157,259,1538` — 屏幕保护事件监听和调用
- `src/lib/server/ClientProxy.h:72` — ClientProxy::screensaver() 接口
- `src/lib/server/ClientProxy1_0.h:50` — ClientProxy1_0::screensaver() 声明
- `src/lib/server/ClientProxy1_0.cpp:376-379` — 发送 CSEC 消息
- `src/lib/server/BaseClientProxy.h:76` — BaseClientProxy::screensaver() 接口
- `src/lib/server/PrimaryClient.cpp:102-112` — enter() screensaver 参数使用
- `src/lib/client/ServerProxy.cpp:283-289` — CSEC 消息接收分支
- `src/lib/base/EventTypes.h:184,187` — PrimaryScreenSaverActivated/Deactivated
- `src/lib/platform/OSXScreenSaver.h/cpp` — macOS 屏幕保护实现（删除）
- `src/lib/platform/MSWindowsScreenSaver.h/cpp` — Windows 屏幕保护实现（删除）
- `src/lib/platform/OSXScreenSaverControl.h` — macOS 屏幕保护辅助（删除）
- `src/lib/platform/OSXScreenSaverUtil.h` — macOS 屏幕保护工具（删除）

### CMake 文件（需移除 Linux 块）
- `src/lib/platform/CMakeLists.txt` — 主要平台编译配置
- `src/lib/gui/CMakeLists.txt` — GUI Linux 条件
- `src/lib/common/CMakeLists.txt` — 公共代码 Linux 条件
- `src/lib/arch/CMakeLists.txt` — 架构层 Linux 条件
- `src/apps/CMakeLists.txt` — 应用入口 Linux 条件
- `src/unittests/CMakeLists.txt` — 测试 Linux 条件
- `src/unittests/platform/CMakeLists.txt` — 平台测试 Linux 条件
- `src/unittests/deskflow/CMakeLists.txt` — Deskflow 测试 BUILD_X11_SUPPORT
- `src/unittests/server/CMakeLists.txt` — 服务器测试 Linux 条件

### GUI 代码中的 Linux 守卫
- `src/lib/gui/Messages.cpp:119` — Q_OS_LINUX 条件
- `src/lib/gui/core/CoreProcess.cpp:18,227` — Q_OS_LINUX 条件
- `src/lib/gui/dialogs/AboutDialog.cpp:53` — Q_OS_LINUX 条件
- `src/lib/common/UrlConstants.h:23` — Q_OS_LINUX 条件

### 先前阶段决策（必须遵循）
- `.planning/phases/01-network-pipeline-rewrite/01-CONTEXT.md` — Phase 1 D-07: 保持 ProtocolUtil::writef 帧格式不变

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ProtocolUtil::writef` / `ProtocolUtil::readf` — 协议帧读写工具，删除 CSEC 消息不影响帧格式
- `IPlatformScreen` / `PlatformScreen` — 平台屏幕接口，删除 screensaver() 方法后接口更简洁
- `ISocketFactory` / 工厂模式 — Phase 1 已用 Asio 重写，不涉及本次清理

### Established Patterns
- 平台条件编译: CMakeLists.txt 用 `if(WIN32)/elseif(APPLE)/elseif(UNIX)` 三段式，GUI 用 `Q_OS_LINUX` 宏
- 接口继承链: IClient → BaseClientProxy → ClientProxy → ClientProxy1_0，screensaver 方法贯穿整条链
- 事件系统: EventQueue + EventTypes，屏幕保护事件通过 addHandler 注册

### Integration Points
- `Server::switchScreen()` — 核心屏幕切换逻辑，删除 forScreensaver 参数影响约 10 个调用点
- `PrimaryClient::enter()` — 唯一使用 screensaver 参数的地方（控制是否 warp cursor）
- `ClientProxy1_0::screensaver()` — 通过 ProtocolUtil::writef 发送 CSEC 消息
- `ServerProxy::parseMessage()` — 消息分发，删除 CSEC 分支后分发链更短

</code_context>

<specifics>
## Specific Ideas

- 删除 screensaver 后 `PrimaryClient::enter()` 将始终执行 `m_screen->warpCursor(xAbs, yAbs)`，逻辑更清晰
- GUI 代码中 Q_OS_LINUX 守卫需检查是否只是简单删除还是需要替换为其他逻辑
- `src/apps/deskflow-daemon/CMakeLists.txt` 存在但 PROJECT.md 标注 "保留 Windows 服务模式"，daemon 目录是否删除需确认

</specifics>

<deferred>
## Deferred Ideas

- `deskflow-daemon` 目录的处理 — PROJECT.md 标注 "保留 Windows 服务模式（可能有用）"，需确认是否移除 daemon 相关代码

</deferred>

---

*Phase: 3-Platform & Feature Cleanup*
*Context gathered: 2026-05-13*
