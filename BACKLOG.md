# 待办事项（Backlog）

## 蓝牙

### ~~macOS 蓝牙客户端收不到服务端数据~~ ✅ 已解决（2026-07-15）

**状态**：已解决。

**实际根因**（纠正了此前的 runloop 推测）：两个具体 bug，与 runloop 集成无关。
1. `rfcommChannelData:` delegate 方法签名与 `IOBluetoothRFCOMMChannelDelegate` 协议不匹配（误用 5 参数 `device:channelID:data:dataLength:`，协议是 3 参数 `data:length:`），IOBluetooth 永不调用，数据回调不派发。
2. `openRFCOMMChannelSync` 在 channel 实际建立时仍返回非成功码，原代码严格按返回码误判失败、不启动 ioThread，收到的数据无人读取，握手超时。

**修复**：见 `CHANGELOG.md` [Unreleased] Fixed——delegate 签名匹配协议 + 以 `newChannel` 是否建立为准。修复后 Mac↔Windows 蓝牙键鼠共享打通。
