<!-- SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers -->
<!-- SPDX-License-Identifier: MIT -->

# Deskflow

一套键盘鼠标，无缝控制多台电脑。

Deskflow 是一个免费开源的键鼠共享软件，让你可以用一套键盘和鼠标无缝控制多台计算机，就像在使用同一台电脑一样。将鼠标移到屏幕边缘，光标就会自动跳转到另一台电脑的屏幕上，键盘输入也会跟随切换。

> ⚠️ **本项目是基于 [deskflow/deskflow](https://github.com/deskflow/deskflow) 的二次开发版本**，在原项目基础上进行了中文化、网络层重构、界面优化等定制。如需了解上游原版项目，请访问 [deskflow/deskflow](https://github.com/deskflow/deskflow)。

## 与上游的主要差异

- **界面全面中文化** — 所有 GUI 文本直接使用中文，无需加载翻译文件
- **深色/浅色主题自动切换** — 根据系统配色方案自动应用对应主题和图标
- **网络层重构** — 基于 Asio 重写了 TCP 网络通信层，支持指数退避自动重连
- **精简平台支持** — 仅保留 Windows 和 macOS，移除了 Linux 平台代码
- **离线构建优化** — Asio 和 GoogleTest 等依赖已内置于 `vendor/` 目录
- **修饰键预设** — 屏幕设置中新增修饰键快捷预设功能
- **移除 TLS 加密** — 简化了网络通信模型

## 特性

- 跨平台支持 Windows、macOS
- Client-Server 架构，一台主机共享键鼠给多台客户端
- 剪贴板共享
- 可视化屏幕布局配置
- 热键和动作自定义
- 系统托盘图标，最小化到托盘运行
- 自适应深色/浅色主题

## 系统要求

### 通用

- CMake >= 3.24
- C++20 编译器
- Qt >= 6.7.0

### macOS

- macOS 12+
- Xcode Command Line Tools

### Windows

- Visual Studio 2022（MSVC v143）
- MSVC Runtime 14.x

## 构建

```bash
# 配置
cmake -B build

# 构建
cmake --build build -j$(nproc)

# 运行测试
ctest --test-dir build
```

### 构建选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `BUILD_TESTS` | 构建单元测试 | ON |
| `BUILD_INSTALLER` | 构建安装包 | ON |
| `BUILD_OSX_BUNDLE` | 构建 macOS .app 包 | ON（仅 macOS） |
| `ENABLE_COVERAGE` | 启用测试覆盖率（gcov） | OFF |
| `VCPKG_QT` | 使用 vcpkg 安装的 Qt（仅 Windows） | OFF |

### 快速运行

```bash
# 构建并启动图形界面
cmake --build build --target run-gui

# 构建并以调试模式启动命令行服务端
cmake --build build --target run
```

## 使用

Deskflow 提供三个可执行程序：

| 程序 | 说明 | 平台 |
|------|------|------|
| `deskflow` | Qt 图形界面（推荐） | 全平台 |
| `deskflow-core` | 命令行模式（服务端/客户端） | 全平台 |
| `deskflow-daemon` | 后台守护进程 | 仅 Windows |

### 基本用法

1. 在所有电脑上安装 Deskflow
2. 选择一台作为**服务端**（共享键鼠的主机），其余作为**客户端**
3. 在服务端配置屏幕布局，拖拽排列各屏幕的相对位置
4. 启动连接后，将鼠标移到屏幕边缘即可切换到另一台电脑

## 项目结构

```
src/
├── lib/
│   ├── deskflow/     # 核心库（协议、键鼠映射、屏幕、剪贴板、IPC）
│   ├── gui/          # Qt6 图形界面（主窗口、对话框、控件、配置）
│   ├── server/       # 服务端逻辑（客户端代理 v1.0~v1.8、配置、输入过滤）
│   ├── client/       # 客户端逻辑（连接、协议解析）
│   ├── platform/     # 平台抽象层（Windows / macOS）
│   ├── net/          # 网络层（基于 Asio 的 TCP 通信、SPSC 队列）
│   ├── io/           # I/O 抽象（流、缓冲区、过滤器）
│   ├── mt/           # 多线程工具（互斥锁、条件变量、线程）
│   ├── base/         # 基础设施（事件队列、日志、异常、字符串工具）
│   ├── common/       # 公共定义（常量、枚举、设置、国际化）
│   └── arch/         # 架构抽象（POSIX 线程、BSD 网络）
├── apps/             # 可执行程序入口
│   ├── deskflow-gui/     # 图形界面入口
│   ├── deskflow-core/    # 命令行入口
│   └── deskflow-daemon/  # 守护进程入口（仅 Windows）
└── unittests/        # 单元测试
```

### GUI 组件

| 组件 | 说明 |
|------|------|
| MainWindow | 主窗口，包含菜单栏、系统托盘、日志面板 |
| ServerConfigDialog | 可视化屏幕布局编辑器 |
| ScreenSettingsDialog | 单个屏幕设置（名称、别名、修饰键） |
| SettingsDialog | 应用全局设置 |
| ActionDialog | 热键动作配置 |
| HotkeyDialog | 热键绑定配置 |
| AboutDialog | 关于对话框 |

## 打包

使用 CPack 进行打包：

- **macOS**: DMG（DragNDrop）
- **Windows**: MSI (WiX v4) / 7Z 压缩包

## 技术栈

| 类别 | 技术 |
|------|------|
| 编程语言 | C++20 |
| 构建系统 | CMake >= 3.24 |
| GUI 框架 | Qt 6.7+（Widgets、Network） |
| 网络库 | Asio 1.38（header-only，内置） |
| 测试框架 | Google Test 1.15（内置）+ Qt Test |
| 协议端口 | TCP 24800 |

## 许可证

- 项目整体：GPL-2.0-only（含 OpenSSL 例外）
- 部分代码：MIT License
- KDE Breeze 图标：LGPL-2.1-only

## 致谢

- [Deskflow Developers](https://github.com/deskflow/deskflow) — 上游项目
- [Symless Ltd](https://github.com/symless) — Synergy 原始开发
- Nick Bolton — Synergy 创始人
