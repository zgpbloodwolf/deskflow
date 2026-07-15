# 待办事项（Backlog）

## 蓝牙

### macOS 蓝牙客户端收不到服务端数据（IOBluetooth 数据通知不派发）

**状态**：待攻关（IOBluetooth 专项，独立跟进）

**现象**：Mac 作为蓝牙客户端连上 Windows 服务端后（蓝牙链路层「已连接」），deskflow 握手 2 秒超时。Windows 服务端确认已发送 hello（15 字节、写入成功），但 Mac 端 `rfcommChannelData:` delegate 数据回调**从未派发**（关闭回调 `rfcommChannelClosed:` 能派发）。

**已排除**：
- Mac 蓝牙后端崩溃（已修复，见 `CHANGELOG.md` [Unreleased] Fixed）
- RunLoop 未持续运行（已修复：保活源让 `CFRunLoopRun` 持续运行 2 秒直到 close）
- delegate 未设置（channel `isOpen=1`、delegate 已设）
- Windows 未发送数据（Windows 日志确认已写 hello）

**根因方向**：
- IOBluetooth channel 的 delegate 数据通知默认派发在 main runloop，而 deskflow main thread 运行的是 `EventQueue::loop()`（自定义事件循环，不驱动 NSRunLoop/CFRunLoop 源），导致数据回调永不触发。
- 或 outgoing RFCOMM channel 的 incoming data notification 需额外激活。

**候选方案**（均需较大改动 + IOBluetooth 专项验证）：
1. 用 `IOBluetoothRFCOMMChannel` 主动读取（如 `readAsync`）替代 delegate 数据回调，绕过数据通知机制。
2. 让 main thread 接入/驱动 NSRunLoop，使 IOBluetooth 数据源派发（改动 deskflow 事件循环，风险大）。
3. IOBluetooth 数据包层面确认 Windows 数据是否到达 Mac 蓝牙栈（区分「数据没到」vs「到了但通知没派发」）。

**关联**：本次已修复 Mac 端崩溃与 RunLoop 保活（见 `CHANGELOG.md`），数据接收作为独立后续任务跟进。
