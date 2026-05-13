# Phase 3: Platform & Feature Cleanup - Research

**Researched:** 2026-05-13
**Domain:** C++ 平台层清理、条件编译移除、CMake 构建系统重构
**Confidence:** HIGH

## Summary

Phase 3 需要在三个维度上做大规模代码删除：(1) 删除 44 个 Linux 平台文件（X11/Wayland/EI/Portal/XDG），以及 7 个 unix 通用文件（arch/unix 8 个、deskflow/unix 7 个），加上 6 个 Linux 测试文件；(2) 完全移除屏幕保护联动功能，涉及跨 15+ 文件的接口方法、协议消息、事件类型和平台实现；(3) 清理所有 CMake 条件编译块和 GUI 中的 Q_OS_LINUX 守卫。这三项工作之间存在交叉依赖：删除 Linux 平台文件会带走 XWindowsScreenSaver，但 MSWindowsScreenSaver 和 OSXScreenSaver 需要单独删除；删除 screensaver 功能后，Server.cpp 中的 onScreensaver/switchScreen 的 forScreensaver 参数链需要全部重写。

**Primary recommendation:** 按 "文件删除 -> CMake 清理 -> 接口重构 -> 编译验证" 的顺序执行，先物理删除文件再处理编译错误，避免中间态遗漏。

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** 完全移除所有屏幕保护代码 -- IScreenSaver 接口、kMsgCScreenSaver 协议消息、Server 事件处理（PrimaryScreenSaverActivated/Deactivated）、平台实现类（OSXScreenSaver、MSWindowsScreenSaver）、EventTypes 中的屏幕保护事件类型
- **D-02:** 完全删除 kMsgCScreenSaver 消息常量、ClientProxy1_0 发送端、ServerProxy 接收端分支。不保留上游 Barrier/Deskflow 兼容性（个人精简版不需要）
- **D-03:** 仅删除 Linux 相关的 CMake 块（UNIX AND NOT APPLE, elseif(UNIX), BUILD_X11_SUPPORT, LIBEI_FOUND 等），不重构现有的 WIN32/Apple if/elseif 结构。9 个 CMakeLists.txt 文件需要修改
- **D-04:** 删除 IClient::enter() 的 screensaver bool 参数、Server::switchScreen() 的 forScreensaver 参数、以及 Server.cpp 中的 m_activeSaver/m_xSaver/m_ySaver 状态变量。所有调用点简化

### Claude's Discretion
- Linux 文件的具体删除顺序（可按文件类型批量删除）
- #ifdef 守卫清理的具体范围判定
- 删除后 include 清理的具体方式
- 单元测试中 Linux 相关测试用例的处理

### Deferred Ideas (OUT OF SCOPE)
- `deskflow-daemon` 目录的处理 -- PROJECT.md 标注 "保留 Windows 服务模式（可能有用）"，需确认是否移除 daemon 相关代码
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CLEAN-02 | 移除 Linux 平台支持（X11/Wayland/EI，44 个文件） | 文件清单已验证（44 个 Linux + 8 个 arch/unix + 7 个 deskflow/unix + 6 个测试 = 65 文件）；CMake 块已定位 |
| CLEAN-03 | 移除屏幕保护联动功能 | 接口链已完整追踪：IScreenSaver -> IPlatformScreen -> PlatformScreen -> OSXScreen/MSWindowsScreen -> Server/ClientProxy/Client；协议消息 CSEC 已定位 |
| CLEAN-04 | 清理移除功能后的残留代码和 #ifdef 分支 | 所有 #ifdef 位置已编目：WINAPI_XWINDOWS(13 处)、WINAPI_LIBEI(10 处)、Q_OS_LINUX(4 处)、UNIX AND NOT APPLE(多处) |
| CLEAN-05 | 所有 CMakeLists.txt 更新，不再编译已移除的模块 | 9+ 个 CMakeLists.txt 已读取分析，精确块范围已确定 |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Linux 平台文件删除 | 文件系统 | -- | 纯文件删除操作 |
| CMake 构建配置 | 构建系统 | -- | 条件编译块删除、依赖列表更新 |
| 屏幕保护接口移除 | API 层 (接口继承链) | 服务层 (Server/Client) | IClient->BaseClientProxy->ClientProxy->ClientProxy1_0 继承链贯穿多个模块 |
| 协议消息移除 | 网络协议层 | 客户端/服务端 | kMsgCScreenSaver 消息定义和发送/接收两端 |
| #ifdef 守卫清理 | 预处理器 | GUI (Q_OS_LINUX) | 编译时宏和运行时条件编译 |
| macOS/Windows screensaver 实现 | 平台层 | -- | 平台特定类的删除和方法移除 |

## Standard Stack

### Core
本阶段不引入新库，纯粹是代码删除和接口简化。

### Build Tools
| Tool | Version | Purpose |
|------|---------|---------|
| CMake | 3.x | 构建系统，需修改多个 CMakeLists.txt |
| Qt6 | 6.7+ | GUI 框架，Q_OS_LINUX 宏清理 |
| Clang/MSVC | 现有版本 | 编译验证 |

## Architecture Patterns

### 系统架构图

```
接口删除链 (Screensaver Feature):

  IClient::enter(..., forScreensaver)
       |
       +-> BaseClientProxy::enter(..., forScreensaver)  [纯虚接口]
       |     |
       |     +-> ClientProxy::enter(..., forScreensaver)  [纯虚接口]
       |     |     |
       |     |     +-> ClientProxy1_0::enter(..., forScreensaver)  [实现: 发送 CINN 协议]
       |     |
       |     +-> PrimaryClient::enter(..., forScreensaver)  [实现: 条件 warpCursor]
       |
       +-> Client::enter(..., forScreensaver)  [实现: 忽略参数]
             |
             +-> Screen::enter(mask)  [委托到平台]

  IClient::screensaver(bool)
       |
       +-> BaseClientProxy::screensaver(bool)  [纯虚接口]
       |     |
       |     +-> ClientProxy1_0::screensaver(bool)  [发送 CSEC 协议]
       |     +-> PrimaryClient::screensaver(bool)   [忽略]
       |
       +-> Client::screensaver(bool)  [委托到 Screen]

  Server.cpp 事件监听:
       PrimaryScreenSaverActivated -> onScreensaver(true) -> switchScreen(primary, 0, 0, true)
       PrimaryScreenSaverDeactivated -> onScreensaver(false) -> switchScreen(saved, x, y, false)

  协议消息流:
       Server -> ClientProxy1_0::screensaver() -> ProtocolUtil::writef(kMsgCScreenSaver)
       Client -> ServerProxy::parseMessage() -> CSEC 分支 -> ServerProxy::screensaver()
```

### Recommended Project Structure (Phase 后)
```
src/lib/platform/          # 仅保留 MSWindows* 和 OSX* 文件
src/lib/arch/              # 仅保留 win32/ 子目录
src/lib/deskflow/          # 无 unix/ 子目录，无 IScreenSaver.h
src/unittests/             # 无 XWindows/WlClipboard/X11 测试文件
```

### Pattern 1: 平台条件编译三段式
**What:** CMakeLists.txt 中的 `if(WIN32)/elseif(APPLE)/elseif(UNIX)` 三段式平台选择
**When to use:** 需要从三段式变为二段式 `if(WIN32)/elseif(APPLE)`（或 `else()`）
**Example:**
```cmake
# 删除前:
if(WIN32)
  set(PLATFORM_CODE ...)
elseif(APPLE)
  set(PLATFORM_CODE ...)
elseif(UNIX)
  set(PLATFORM_CODE ...)   # <-- 删除整个 elseif(UNIX) 块
endif()

# 删除后:
if(WIN32)
  set(PLATFORM_CODE ...)
elseif(APPLE)
  set(PLATFORM_CODE ...)
endif()
```

### Pattern 2: 接口参数删除的瀑布式传播
**What:** 删除 IClient::enter() 的 forScreensaver 参数需要更新整条继承链
**When to use:** 删除任何贯穿接口继承链的参数时
**Example:**
```cpp
// 删除前:
virtual void enter(int32_t x, int32_t y, uint32_t seq, KeyModifierMask mask, bool forScreensaver) = 0;

// 删除后:
virtual void enter(int32_t x, int32_t y, uint32_t seq, KeyModifierMask mask) = 0;
// 所有子类 override 同步删除参数
// 所有调用点删除最后一个实参
```

### Anti-Patterns to Avoid
- **逐个文件删除再编译验证:** 会导致大量中间态编译错误。应批量删除同类文件后统一编译。
- **遗漏 unix 通用文件:** CONTEXT.md 只列了 44 个 Linux 平台文件，但还有 arch/unix/（8 个）、deskflow/unix/（7 个）文件也是 Linux 专属。
- **忘记删除 OSXScreenSaverUtil.m:** 此 .m 文件在 CMakeLists.txt 的 APPLE 块中列出，需同步删除行。

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| 条件编译清理 | 手动逐行搜索 #ifdef | 批量 grep + 编译验证 | grep 漏检率高，编译器会报告未解析符号 |
| 接口参数删除 | 只改声明不改定义 | 声明+定义+所有调用点一起改 | 参数不匹配会导致链接错误而非编译错误 |

## Runtime State Inventory

> 本阶段涉及文件删除和代码重构，涉及构建状态。

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | 无数据库存储 -- 纯代码变更 | -- |
| Live service config | `cmake/Libraries.cmake` 中 `find_library(lib_ScreenSaver ScreenSaver)` 在 APPLE 分支中引用 macOS ScreenSaver 框架 | 删除屏幕保护后需同步移除此 find_library 调用 |
| OS-registered state | 无 OS 注册状态 | -- |
| Secrets/env vars | 无密钥或环境变量变更 | -- |
| Build artifacts | 删除文件后 CMake 缓存将过时，需重新运行 CMake configure | 删除文件后执行 clean rebuild |

## Common Pitfalls

### Pitfall 1: 遗漏 deskflow/unix 和 arch/unix 目录
**What goes wrong:** CONTEXT.md 标注 "44 个 Linux 文件" 指的仅是 `src/lib/platform/` 下的文件。`src/lib/deskflow/unix/`（7 个文件）和 `src/lib/arch/unix/`（8 个文件）也是 Linux 专属，必须一并删除。
**Why it happens:** CONTEXT.md 聚焦于 platform 目录，未展开其他子目录中的 unix 块。
**How to avoid:** 完整文件清单应包含三类：(a) platform/ 下 44 个文件，(b) deskflow/unix/ 下 7 个文件，(c) arch/unix/ 下 8 个文件，加上 6 个 Linux 测试文件。总计 65 个文件。
**Warning signs:** 编译时如果仍有 `elseif(UNIX)` 块引用这些文件，会因文件不存在而报错。

### Pitfall 2: MSWindowsHook 的 screensaver 函数被其他代码调用
**What goes wrong:** MSWindowsHook::installScreenSaver()/uninstallScreenSaver() 被 MSWindowsDesks 调用。删除 MSWindowsScreenSaver 后，MSWindowsDesks 仍依赖 IScreenSaver 指针来检查 isActive()。需要重构 MSWindowsDesks 以移除 screensaver 依赖。
**Why it happens:** MSWindowsDesks 的构造函数接收 `const IScreenSaver *screensaver` 参数，用 `m_screensaver->isActive()` 判断是否在屏幕保护激活时阻止桌面切换。
**How to avoid:** 删除 IScreenSaver 后，MSWindowsDesks 构造函数需要移除 screensaver 参数，并将 `m_screensaver->isActive()` 调用替换为直接返回 false（不再阻止桌面切换）。
**Warning signs:** MSWindowsDesks.cpp 编译错误 -- 未解析的 IScreenSaver 符号。

### Pitfall 3: ScreenSaver macOS 框架链接
**What goes wrong:** `cmake/Libraries.cmake` 在 APPLE 分支中有 `find_library(lib_ScreenSaver ScreenSaver)`，链接到 macOS ScreenSaver 框架。删除 OSXScreenSaver 后如果不删除此行，链接不会出错（框架仍然存在），但会引入不必要的依赖。
**Why it happens:** OSXScreenSaver.cpp 使用 ScreenSaver.framework 的私有 API（OSXScreenSaverControl.h）。
**How to avoid:** 从 cmake/Libraries.cmake 的 APPLE 块中移除 `find_library(lib_ScreenSaver ScreenSaver)` 和 `${lib_ScreenSaver}`。

### Pitfall 4: GUI 中 wl-clipboard 相关 UI 和设置
**What goes wrong:** SettingsDialog.ui 中有 `widgetWlClipboard` 和 `cbUseWlClipboard` 控件，Settings.h 中有 `UseWlClipboard` 设置项。这些是 Linux Wayland 专属功能，需要清理。但 SettingsDialog.cpp 中已有 `if (isWayland())` 条件检查，在非 Linux 上这些控件会被隐藏。
**Why it happens:** 这些 UI 元素虽然运行时被隐藏，但源代码中仍存在。
**How to avoid:** 作为 Claude's Discretion 区域，可选择：(a) 完全删除 wl-clipboard UI 和设置项，或 (b) 保留但确保在 macOS/Windows 上不显示。推荐 (a)，因为 USE_WL_CLIPBOARD 设置只在 WlClipboard.cpp 中使用（也被删除）。
**Warning signs:** 编译时 WlClipboard 引用错误。

### Pitfall 5: PlatformInfo.h 中 isWayland/isFlatpak/isSnap/isSandboxed 函数
**What goes wrong:** PlatformInfo.h 包含 `isWayland()`、`isFlatpak()`、`isSnap()`、`isSandboxed()` 等 Linux 专属函数。删除 Linux 平台后这些函数不应再被调用。
**Why it happens:** 这些函数在 ServerApp.cpp、ClientApp.cpp、SettingsDialog.cpp 中被调用，均在 `#else`（非 WIN32、非 APPLE）分支内。
**How to avoid:** 删除 Linux 代码路径后这些调用自然消失。PlatformInfo.h 中可以保留函数定义（它们是 inline 函数，不增加二进制大小），也可以删除以保持代码整洁。推荐删除。
**Warning signs:** 未使用的函数警告（如编译器开启了 -Wunused-function）。

### Pitfall 6: switchScreen forScreensaver 参数的调用点散布
**What goes wrong:** Server.cpp 中 `switchScreen` 有约 8 个调用点，其中仅 2 个使用 `true`（来自 onScreensaver），其余 6 个使用 `false`。删除参数时需要更新所有调用点。
**Why it happens:** forScreensaver 参数控制是否在 PrimaryClient::enter() 中跳过 warpCursor。
**How to avoid:** 删除参数后，PrimaryClient::enter() 将始终执行 warpCursor（即 forScreensaver=false 的行为），这是正确的简化。
**Warning signs:** 编译错误 "too many arguments to function"。

## Code Examples

### 示例 1: IClient::enter() 参数简化

```cpp
// 删除前 (IClient.h:37):
virtual void enter(int32_t xAbs, int32_t yAbs, uint32_t seqNum,
                   KeyModifierMask mask, bool forScreensaver) = 0;

// 删除后:
virtual void enter(int32_t xAbs, int32_t yAbs, uint32_t seqNum,
                   KeyModifierMask mask) = 0;
```

### 示例 2: Server::switchScreen 参数简化

```cpp
// 删除前 (Server.h:224):
void switchScreen(BaseClientProxy *, int32_t x, int32_t y, bool forScreenSaver);

// 删除后:
void switchScreen(BaseClientProxy *, int32_t x, int32_t y);
```

### 示例 3: Server.cpp onScreensaver 完全删除

```cpp
// 删除前 (Server.cpp:93-100): 构造函数中的事件监听
m_events->addHandler(
    EventTypes::PrimaryScreenSaverActivated, m_primaryClient->getEventTarget(),
    [this](const auto &) { onScreensaver(true); }
);
m_events->addHandler(
    EventTypes::PrimaryScreenSaverDeactivated, m_primaryClient->getEventTarget(),
    [this](const auto &) { onScreensaver(false); }
);

// 删除前 (Server.cpp:257-260): adoptClient 中的 screensaver 调用
if (m_activeSaver != nullptr) {
    client->screensaver(true);
}

// 删除前 (Server.cpp:1489-1540): onScreensaver 方法整个删除

// 删除前 (Server.h): 状态变量
BaseClientProxy *m_activeSaver = nullptr;
int32_t m_xSaver;
int32_t m_ySaver;
```

### 示例 4: IPlatformScreen 接口简化

```cpp
// 删除前 (IPlatformScreen.h:93-110):
virtual void openScreensaver(bool notify) = 0;
virtual void closeScreensaver() = 0;
virtual void screensaver(bool activate) = 0;

// 删除后: 三个方法全部移除
// PlatformScreen.h 中对应的 override 声明也同步删除
// OSXScreen.h 和 MSWindowsScreen.h 中的 override 声明删除
// OSXScreen.mm 和 MSWindowsScreen.cpp 中的实现删除
```

### 示例 5: CMakeLists.txt platform 清理

```cmake
# 删除前 (src/lib/platform/CMakeLists.txt):
if(UNIX AND NOT APPLE)
  pkg_check_modules(LIBEI ...)    # 行 6-21: 整个块删除
  ...
endif()

if(WIN32)
  set(PLATFORM_SOURCES ...)        # 保留
elseif(APPLE)
  set(PLATFORM_SOURCES ...)        # 保留
elseif(UNIX)
  set(PLATFORM_SOURCES ...)        # 行 104-166: 整个块删除
endif()

# 后半部分 UNIX 块 (行 173-201):
if(UNIX)
  target_link_libraries(...)       # 保留 APPLE 子块，删除 else() 部分
  if(APPLE)
    ...                            # 保留
  else()
    ...                            # 删除 (LIBEKBOMMON, LIBEI, LIBPORTAL 等)
  endif()
endif()
```

## Complete File Deletion Inventory

### Linux Platform Files (44 files in src/lib/platform/)
[VERIFIED: filesystem grep]

XWindows (25 files):
- XWindowsClipboard.cpp/.h
- XWindowsClipboardAnyBitmapConverter.cpp/.h
- XWindowsClipboardBMPConverter.cpp/.h
- XWindowsClipboardHTMLConverter.cpp/.h
- XWindowsClipboardTextConverter.cpp/.h
- XWindowsClipboardUCS2Converter.cpp/.h
- XWindowsClipboardUTF8Converter.cpp/.h
- XWindowsConfig.h.in
- XWindowsEventQueueBuffer.cpp/.h
- XWindowsKeyState.cpp/.h
- XWindowsScreen.cpp/.h
- XWindowsScreenSaver.cpp/.h
- XWindowsUtil.cpp/.h

EI (6 files):
- EiEventQueueBuffer.cpp/.h
- EiKeyState.cpp/.h
- EiScreen.cpp/.h

Wl (4 files):
- WlClipboard.cpp/.h
- WlClipboardCollection.cpp/.h

Portal (4 files):
- PortalInputCapture.cpp/.h
- PortalRemoteDesktop.cpp/.h

XDG (5 files):
- XDGKeyUtil.cpp/.h
- XDGPortalRegistry.h
- XDGPowerManager.cpp/.h

### Screensaver Files to Delete (8 files)
[VERIFIED: filesystem grep]

- src/lib/deskflow/IScreenSaver.h (接口定义)
- src/lib/platform/MSWindowsScreenSaver.cpp/.h (Windows 实现)
- src/lib/platform/OSXScreenSaver.cpp/.h (macOS 实现)
- src/lib/platform/OSXScreenSaverControl.h (macOS 私有 API 头文件)
- src/lib/platform/OSXScreenSaverUtil.h (macOS 辅助)
- src/lib/platform/OSXScreenSaverUtil.m (macOS Objective-C 实现)

### Unix Common Files to Delete (15 files)
[VERIFIED: filesystem grep]

arch/unix (8 files):
- src/lib/arch/unix/ArchLogUnix.cpp/.h
- src/lib/arch/unix/ArchMultithreadPosix.cpp/.h
- src/lib/arch/unix/ArchNetworkBSD.cpp/.h
- src/lib/arch/unix/XArchUnix.cpp/.h

deskflow/unix (7 files):
- src/lib/deskflow/unix/AppUtilUnix.cpp/.h
- src/lib/deskflow/unix/DeskflowXkbKeyboard.cpp/.h
- src/lib/deskflow/unix/ISO639Table.h
- src/lib/deskflow/unix/X11LayoutsParser.cpp/.h

### Linux Test Files to Delete (6 files)
[VERIFIED: filesystem grep]

- src/unittests/platform/XWindowsClipboardTests.cpp/.h
- src/unittests/platform/WlClipboardTests.cpp/.h
- src/unittests/deskflow/X11LayoutParserTests.cpp/.h

**Total files to delete: 65 files + 2 unix directories (src/lib/arch/unix/, src/lib/deskflow/unix/)**

## CMake Modification Inventory

### 需要修改的 CMakeLists.txt 文件

| File | 修改内容 | 复杂度 |
|------|----------|--------|
| `CMakeLists.txt` (root) | 删除 REQUIRED_LIBEI/REQUIRED_LIBPORTAL 版本变量，删除 `if(UNIX AND NOT APPLE)` 块中 BUILD_X11_SUPPORT option | 低 |
| `cmake/Libraries.cmake` | 删除 `else()` 分支中的 BUILD_X11_SUPPORT 调用、configure_xorg_libs 调用、xkbcommon/glib2/pkg-config 查找；删除 `find_library(lib_ScreenSaver ScreenSaver)` 和 `${lib_ScreenSaver}`；删除整个 `configure_xorg_libs()` 宏 | 高 |
| `src/lib/platform/CMakeLists.txt` | 删除顶部 `if(UNIX AND NOT APPLE)` 块（行 6-21）；删除 `elseif(UNIX)` 块（行 104-166）；删除底部 `if(UNIX)` 块中的 `else()` 部分（行 186-199）；删除 APPLE 块中的 screensaver 文件（OSXScreenSaver*、OSXScreenSaverUtil*）；删除 WIN32 块中的 MSWindowsScreenSaver 文件 | 高 |
| `src/lib/deskflow/CMakeLists.txt` | 删除 `elseif(UNIX)` 块（行 14-24）；将底部 `if(UNIX)` 块改为 `if(APPLE)`；删除 `IScreenSaver.h` 从文件列表；移除 `if(NOT APPLE)` Qt6::Xml 链接 | 中 |
| `src/lib/arch/CMakeLists.txt` | 删除 `elseif(UNIX)` 块（行 23-34） | 低 |
| `src/lib/client/CMakeLists.txt` | 将 `if(UNIX)` 块改为 `if(APPLE)`（macOS 也需要链接 app 和 io） | 低 |
| `src/lib/server/CMakeLists.txt` | 将 `if(UNIX)` 块改为 `if(APPLE)` | 低 |
| `src/lib/gui/CMakeLists.txt` | 无 Linux 特定块需要修改（仅有 APPLE 平台源文件和 WIN32 链接） | 无 |
| `src/apps/CMakeLists.txt` | 删除 `if(UNIX AND NOT APPLE)` 块（行 6-11）和 help2man 相关代码；删除 `generate_app_man` 函数；将底部 `else()` 块中的 man page 生成删除 | 中 |
| `src/apps/deskflow-gui/CMakeLists.txt` | 删除底部 `else()` 块中的 `generate_app_man` 调用（行 104-106） | 低 |
| `src/unittests/CMakeLists.txt` | 删除 `if(UNIX AND NOT APPLE)` 块（行 69-71） | 低 |
| `src/unittests/platform/CMakeLists.txt` | 删除 `elseif(UNIX)` 块及其子条件（行 27-48） | 低 |
| `src/unittests/deskflow/CMakeLists.txt` | 删除 `if(BUILD_X11_SUPPORT)` 块（行 56-64） | 低 |

## Interface Change Map (Screensaver Removal)

### 需要修改的接口和类

| File | Change | Impact |
|------|--------|--------|
| IClient.h:37 | 删除 `enter()` 的 `bool forScreensaver` 参数 | 8 个子类需同步 |
| IClient.h:131 | 删除 `virtual void screensaver(bool)` | 8 个子类需同步 |
| BaseClientProxy.h:63,76 | 删除 enter/screensaver 声明 | 纯虚，子类同步 |
| ClientProxy.h:59,72 | 删除 enter/screensaver 声明 | 纯虚，子类同步 |
| ClientProxy1_0.h:37,50 | 删除 enter/screensaver 声明 | 实现类，需删 cpp 定义 |
| ClientProxy1_0.cpp:228,376 | 删除 enter/screensaver 实现 | CINN 协议中不再传 forScreensaver |
| PrimaryClient.h:112,125 | 删除 enter/screensaver 声明 | 实现类 |
| PrimaryClient.cpp:102-108,194 | 删除 enter/screensaver 实现 | warpCursor 不再条件跳过 |
| Client.h:139,152 | 删除 enter/screensaver 声明 | 实现类 |
| Client.cpp:190,274 | 删除 enter/screensaver 实现 | 直接委托 |
| Server.h:224 | 删除 switchScreen 的 forScreensaver 参数 | -- |
| Server.h:328 | 删除 onScreensaver 声明 | -- |
| Server.h:388,441-442 | 删除 m_activeSaver/m_xSaver/m_ySaver | 状态变量 |
| Server.cpp:93-100 | 删除事件监听注册 | 构造函数 |
| Server.cpp:156-157 | 删除事件监听注销 | 析构函数 |
| Server.cpp:257-260 | 删除 adoptClient 中的 screensaver 调用 | -- |
| Server.cpp:379 | switchScreen 删除 forScreensaver 参数 | -- |
| Server.cpp:474 | enter 调用删除 forScreensaver 实参 | -- |
| Server.cpp:1489-1540 | 删除整个 onScreensaver 方法 | -- |
| IPlatformScreen.h:88-110 | 删除 openScreensaver/closeScreensaver/screensaver | 平台接口 |
| PlatformScreen.h:73-75 | 删除对应的 override 声明 | 基类 |
| Screen.h:100-105 | 删除 screensaver 方法声明 | -- |
| Screen.cpp:217-220 | 删除 screensaver 方法实现 | -- |
| Screen.cpp:407-413 | enablePrimary 中删除 openScreensaver(true) 调用 | -- |
| Screen.cpp:424-433 | disablePrimary/disableSecondary 中删除 closeScreensaver() | -- |
| EventTypes.h:183-187 | 删除 PrimaryScreenSaverActivated/Deactivated 枚举值 | 事件类型 |
| ProtocolTypes.h:408-434 | 删除 kMsgCScreenSaver 文档声明 | -- |
| ProtocolTypes.cpp:21 | 删除 kMsgCScreenSaver 定义 | -- |
| ServerProxy.cpp:283-284 | 删除 CSEC 消息分支 | 消息解析 |
| ServerProxy.cpp:740-748 | 删除 screensaver() 方法 | -- |
| ServerProxy.h:93 | 删除 screensaver 声明 | -- |
| MSWindowsScreen.h:24,116-118,297-299 | 删除 screensaver 相关声明和成员 | -- |
| MSWindowsScreen.cpp | 删除 openScreensaver/closeScreensaver/screensaver 实现，删除 m_screensaver 使用 | 大量修改 |
| MSWindowsDesks.h:26,51-57,116-118,261-262 | 删除 IScreenSaver 参数和相关逻辑 | -- |
| MSWindowsDesks.cpp:114-119,216-217,643-645,725-727,798-802 | 删除 screensaver 逻辑 | -- |
| MSWindowsHook.h:69,71 | 删除 installScreenSaver/uninstallScreenSaver | -- |
| MSWindowsHook.cpp:47,692-710 | 删除 screensaver hook 实现 | -- |
| OSXScreen.h:259-260 | 删除 m_screensaver 和 m_screensaverNotify 成员 | -- |
| OSXScreen.h:39,88-90 | 删除 screensaver 相关声明 | -- |
| OSXScreen.mm:89-90,118,168,214,826-846 | 删除 screensaver 相关代码 | -- |

### GUI/Linux 守卫清理

| File | Line | Content | Action |
|------|------|---------|--------|
| UrlConstants.h | 23-25 | `Q_OS_LINUX` 条件下 kUrlGnomeTrayFix | 删除 #if/#endif 块 |
| Messages.cpp | 119-126 | `Q_OS_LINUX` 条件下 GNOME 3 通知区域提示 | 删除 #if/#endif 块 |
| CoreProcess.cpp | 18-21 | `Q_OS_LINUX` 头文件 include (signal.h, sys/prctl.h) | 删除 #ifdef/#endif 块 |
| CoreProcess.cpp | 227-233 | `Q_OS_LINUX` 条件下 prctl(PR_SET_PDEATHSIG) | 删除 #ifdef/#endif 块 |
| AboutDialog.cpp | 53-56 | `Q_OS_LINUX` 条件下追加 XDG session 信息 | 删除 #ifdef/#endif 块 |
| SettingsDialog.cpp | 297-303 | `isWayland()` 条件下 wl-clipboard 控件可见性 | 删除整个 if 块（WlClipboard 已删除） |
| SettingsDialog.ui | 687-699 | widgetWlClipboard 和 cbUseWlClipboard 控件 | 删除 XML 元素 |
| Settings.h | 58 | UseWlClipboard 设置项 | 可选删除（无代码引用后为死代码） |

### 共享代码中的 WINAPI_XWINDOWS / WINAPI_LIBEI 守卫

| File | Lines | Content | Action |
|------|-------|---------|--------|
| App.cpp | 29-31 | `#if defined(WINAPI_XWINDOWS) or defined(WINAPI_LIBEI)` include XDGPortalRegistry | 删除 #if/#endif 块 |
| App.cpp | 49-51 | 同上 setAppId() 调用 | 删除 #if/#endif 块 |
| App.h | 144-152 | `#if !defined(WINAPI_LIBEI) && WINAPI_XWINDOWS` s_helpNoWayland | 删除 #if/#else/#endif 块 |
| ClientApp.cpp | 32-38 | `#if WINAPI_XWINDOWS` / `#if WINAPI_LIBEI` include | 删除 #if/#endif 块 |
| ClientApp.cpp | 112-130 | `#else` 下的 isWayland 分支 + WINAPI_LIBEI/XWINDOWS | 将 `#else` 保留为编译错误（不应到达） |
| ServerApp.cpp | 38-44 | `#if WINAPI_XWINDOWS` / `#if WINAPI_LIBEI` include | 删除 #if/#endif 块 |
| ServerApp.cpp | 395-415 | `#else` 下的 isWayland 分支 + WINAPI_LIBEI/XWINDOWS | 同 ClientApp 处理 |
| SettingsDialog.cpp | 297-303 | isWayland 检查 wl-clipboard 可见性 | 删除 |

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| 三平台支持 (Win/Mac/Linux) | 双平台支持 (Win/Mac) | Phase 3 | 大量 CMake 条件编译简化 |
| Screensaver 同步 | 无 screensaver 功能 | Phase 3 | 协议消息 CSEC 删除，接口参数简化 |
| X11/Wayland/EI 输入 | 仅 Win/Mac 原生输入 | Phase 3 | 44+15 文件删除 |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `deskflow-daemon` 目录保留（CONTEXT.md deferred），其 CMakeLists.txt 已有 `if(WIN32)` 保护，不需要修改 | CMake Modification | 低 -- 已确认 daemon 只在 WIN32 下编译 |
| A2 | `src/lib/client/CMakeLists.txt` 和 `src/lib/server/CMakeLists.txt` 中的 `if(UNIX)` 链接可以改为 `if(APPLE)` 因为 macOS 也需要这些链接 | CMake Modification | 中 -- 需要确认 macOS 构建是否依赖这些链接 |
| A3 | `Settings::Core::UseWlClipboard` 设置项可以从 Settings.h 中删除，因为唯一的消费者 WlClipboard.cpp 会被删除 | GUI 清理 | 低 -- 删除后不影响功能 |
| A4 | `cmake/Libraries.cmake` 中的 `find_library(lib_ScreenSaver ScreenSaver)` 应该与 OSXScreenSaver 一起删除 | CMake Modification | 低 -- 框架仍在系统中但不再需要链接 |

## Open Questions

1. **deskflow-daemon 是否完全保留？**
   - What we know: CONTEXT.md 标注 deferred，src/apps/CMakeLists.txt 中有 `add_subdirectory(deskflow-daemon)` 且有注释 "Only used on windows"，其 CMakeLists.txt 已有 `if(WIN32)` 保护。
   - Recommendation: 保留，不修改。daemon 的 CMakeLists.txt 已有 if(WIN32) 保护。

2. **src/lib/client/CMakeLists.txt 和 server/CMakeLists.txt 中 `if(UNIX)` 链接苹果是否需要？**
   - What we know: 当前 `if(UNIX)` 下 client 链接 `app io`，server 链接 `app`。macOS 是 UNIX 的子集，APPLE 宏是 UNIX 的子条件。
   - Recommendation: 将 `if(UNIX)` 改为 `if(APPLE)` 以保持 macOS 链接。

3. **Settings::Core::UseWlClipboard 是否删除？**
   - What we know: 该设置被 WlClipboard.cpp（将被删除）和 SettingsDialog.cpp（wl-clipboard UI 将被删除）使用。
   - Recommendation: 删除。标记为 Claude's Discretion 范围。

## Environment Availability

> 本阶段仅涉及代码删除和 CMake 修改，不需要额外工具。macOS 构建环境已就绪。

Step 2.6: SKIPPED (no external dependencies identified -- 纯代码/config变更)

## Sources

### Primary (HIGH confidence)
- 文件系统直接验证 -- 所有 65 个待删除文件存在性已确认
- CMakeLists.txt 直接读取 -- 13 个文件的内容和修改范围已分析
- 源代码直接读取 -- 所有 screensaver 引用链已追踪

### Secondary (MEDIUM confidence)
- CONTEXT.md 中的 canonical refs 与实际代码一致 -- 所有行号和引用点已交叉验证

## Metadata

**Confidence breakdown:**
- Linux 文件清单: HIGH - 文件系统 grep 验证，44+15+6=65 文件全部存在
- Screensaver 引用链: HIGH - 接口继承链和调用点完整追踪
- CMake 修改范围: HIGH - 所有 CMakeLists.txt 直接读取分析
- GUI 清理范围: HIGH - Q_OS_LINUX 和 WINAPI_* 守卫全部 grep 定位

**Research date:** 2026-05-13
**Valid until:** 2026-06-12 (30 days - 结构性代码变更频率低)
