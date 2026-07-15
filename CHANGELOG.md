# 变更日志（Changelog）

本项目所有重要变更记录于此文件。

格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### Added

- 蓝牙连接错误分类：新增 `BtErrorCategory`（`PairingFailed`/`StackUnavailable`/`Retryable`/`Unknown`）与 `classifyBtError`，把 Windows WSA 错误码归类，供上层按类别决定重连策略（`net/BtError.h`）。
- 蓝牙连接失败 GUI 友好提示：`ConnectionRefusal` 枚举新增 `BluetoothPairingFailed`、`BluetoothUnavailable`，经既有 IPC 通道触发中文弹窗（提示重新配对 / 检查蓝牙开关）。
- `classifyBtError` 单元测试（`unittests/net/BtErrorTests`，Windows）。

### Changed

- 蓝牙客户端重连按错误类别决策：配对/认证类失败（如 `10064`）**停止自动重试**并提示用户重新配对；蓝牙栈/无线电不可用（如 `10051`）改为 **30s 长退避**继续重试；其余错误保持原有阶梯退避。错误码经 `BtBackend → BtDataSocket → Client → ClientApp` 全链路传递。

### Fixed

- 修复 Windows 蓝牙连接失败时高频重试（前 300 次一律 1s）打爆已故障蓝牙栈的问题——此前 `10064`（配对失败）与 `10051`（蓝牙栈不可用）等错误码不区分，均以约 1s 间隔反复重连。
- 修复 Windows core 进程中文日志在 GUI 日志面板乱码的问题：`ConsoleLogOutputter` 统一输出 UTF-8（原用 `qPrintable`/`toLocal8Bit` 在中文 Windows 输出 GBK，与 GUI 读取侧 UTF-8 解码不一致）。
- 修复 `BtDataSocket` 重连退避 `1 << (n-1)` 的有符号整数移位隐患（改用乘法推导），并加入 ±20% 随机抖动避免多客户端同步重连（惊群）。
- 修复 macOS 蓝牙后端 `BtBackendMacOS::close()` 的 `CFRunLoopStop(m_runLoop)` 悬空崩溃：RunLoop 线程退出会释放 `CFRunLoopGetCurrent()` 拿到的 runloop，导致 close 操作野指针。改为对 runloop `CFRetain`/`CFRelease` 显式管理生命周期。
- 修复 macOS 蓝牙后端 `close()` 的 `[impl closeConnection]` 野指针崩溃：原顺序先停 RunLoop 线程（其 `@autoreleasepool` drain 会释放 connectToDevice 创建的 IOBluetooth 对象）再关闭连接，改为先 `closeConnection`（对象仍有效）再停 RunLoop。
- macOS 蓝牙后端 connect 的 RunLoop 线程加入 `CFRunLoopSource` 保活源，防止 `CFRunLoopRun()` 因 RunLoop 无源立即退出（使 IOBluetooth 回调得以派发）。
