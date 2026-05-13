<!-- SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers -->
<!-- SPDX-License-Identifier: MIT -->

# Deskflow

Share a single keyboard and mouse between multiple computers.

Deskflow 是一个免费开源的键鼠共享软件，让你可以用一套键盘和鼠标无缝控制多台计算机，就像在使用同一台电脑一样。

> 本项目基于 [deskflow/deskflow](https://github.com/deskflow/deskflow) 开发。

## 特性

- 跨平台支持 Windows、macOS、Linux
- 支持 X11 和 Wayland (libei/libportal)
- Client-Server 架构，一台主机共享键鼠给多台客户端
- 剪贴板共享
- 多显示器屏幕布局配置
- 热键和动作自定义
- 国际化支持（中文、英语、日语、韩语、俄语、西班牙语、意大利语）

## 系统要求

### 通用

- CMake >= 3.24
- C++20 编译器
- Qt >= 6.7.0

### Linux

- libei >= 1.3（Wayland 输入模拟）
- libportal >= 0.9.1（XDG Portal 输入捕获）
- xkbcommon
- 可选：X11（Xtst、Xrandr、Xinerama、Xi）

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

### Linux Wayland 构建

```bash
cmake -B build -DBUILD_X11_SUPPORT=OFF
cmake --build build -j$(nproc)
```

## 使用

Deskflow 提供三个可执行程序：

| 程序 | 说明 |
|------|------|
| `deskflow` | Qt 图形界面（推荐） |
| `deskflow-core` | 命令行模式（服务端/客户端） |
| `deskflow-daemon` | 后台守护进程 |

图形界面模式下，选择当前计算机作为服务端（共享键鼠）或客户端（接收控制），配置屏幕布局后即可使用。

## 项目结构

```
src/
├── lib/
│   ├── deskflow/     # 核心库（协议、键鼠映射、屏幕、剪贴板）
│   ├── gui/          # Qt6 图形界面
│   ├── server/       # 服务端逻辑（客户端代理、配置、输入过滤）
│   ├── client/       # 客户端逻辑
│   ├── platform/     # 平台抽象层（Win/macOS/Linux）
│   ├── net/          # 网络层（基于 Asio 的 TCP 通信）
│   ├── io/           # I/O 抽象
│   ├── mt/           # 多线程工具
│   ├── base/         # 基础设施（事件队列、日志、异常）
│   ├── common/       # 公共定义（常量、枚举、国际化）
│   └── arch/         # 架构抽象
├── apps/             # 可执行程序入口
└── unittests/        # 单元测试
```

## 打包

使用 CPack 进行打包：

- **Linux**: DEB / RPM / Flatpak / Arch PKGBUILD
- **macOS**: DMG
- **Windows**: MSI (WiX v4)

## 许可证

- 代码：MIT License
- 项目：GPL-2.0-only

Deskflow 是 [Synergy](https://github.com/symless/synergy-core) 的社区继任者。

## 致谢

- [Deskflow Developers](https://github.com/deskflow/deskflow)
- [Symless Ltd](https://github.com/symless)
- Nick Bolton
