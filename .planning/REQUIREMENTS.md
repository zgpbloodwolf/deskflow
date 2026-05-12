# Requirements: Deskflow (个人精简版)

**Defined:** 2026-05-12
**Core Value:** 流畅的局域网键盘鼠标共享体验 — 网络波动时不能卡顿，鼠标移动必须跟手

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Network Performance (核心优先级)

- [ ] **NET-01**: 鼠标移动在网络波动时保持流畅，不出现持续卡顿或掉帧
- [ ] **NET-02**: 键盘事件可靠送达，不丢失按键事件
- [ ] **NET-03**: 网络断开时自动释放所有按键（防卡键）
- [ ] **NET-04**: 鼠标移动事件合并发送（1000Hz 输入 → 200Hz 发送，只发最新位置）
- [ ] **NET-05**: 输入处理线程不被网络 I/O 阻塞（解耦架构）
- [ ] **NET-06**: 端到端延迟降至 15ms 以下（当前约 45ms）

### Codebase Cleanup (代码精简)

- [ ] **CLEAN-01**: 移除 TLS/SSL 加密层及所有 SecurityLevel 相关代码（~3000 行）
- [ ] **CLEAN-02**: 移除 Linux 平台支持（X11/Wayland/EI，44 个文件）
- [ ] **CLEAN-03**: 移除屏幕保护联动功能
- [ ] **CLEAN-04**: 清理移除功能后的残留代码和 #ifdef 分支
- [ ] **CLEAN-05**: 所有 CMakeLists.txt 更新，不再编译已移除的模块

### Modernization (现代化)

- [ ] **MOD-01**: SocketMultiplexer 中的手动 new/delete 替换为智能指针
- [ ] **MOD-02**: 自定义 Thread 类替换为 std::jthread + std::stop_token
- [ ] **MOD-03**: reinterpret_cast 游标标记模式替换为安全的迭代方式
- [ ] **MOD-04**: exit(0) 调用替换为优雅的事件队列退出

### Build & Quality (构建和质量)

- [ ] **BQ-01**: macOS 和 Windows 平台均可正常编译通过
- [ ] **BQ-02**: 现有测试在新架构下全部通过
- [ ] **BQ-03**: GUI 配置界面正常工作，无回归
- [ ] **BQ-04**: 剪贴板共享功能正常工作
- [ ] **BQ-05**: 文件拖拽传输功能正常工作

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Advanced Features

- **ADV-01**: 多语言翻译更新（当前冻结）
- **ADV-02**: Wayland 支持重新评估（如果未来有 Linux 需求）
- **ADV-03**: UDP 传输模式（如果 TCP+合并仍不够）
- **ADV-04**: C++20 协程集成 Asio（Phase 2 优化）

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| TLS/SSL 加密 | 局域网使用不需要，减少 ~3000 行代码 |
| Linux 平台 (X11/Wayland/EI) | 只用 macOS + Windows |
| 屏幕保护联动 | 已知 bug 多，日常使用价值低 |
| 多语言翻译维护 | 个人项目优先级低 |
| 商业化功能（许可证、激活） | 个人开源项目 |
| Daemon 模式移除 | 保留 Windows 服务模式（可能有用） |
| 协议格式重设计 | 保持与上游 Deskflow/Barrier 兼容 |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| NET-01 | — | Pending |
| NET-02 | — | Pending |
| NET-03 | — | Pending |
| NET-04 | — | Pending |
| NET-05 | — | Pending |
| NET-06 | — | Pending |
| CLEAN-01 | — | Pending |
| CLEAN-02 | — | Pending |
| CLEAN-03 | — | Pending |
| CLEAN-04 | — | Pending |
| CLEAN-05 | — | Pending |
| MOD-01 | — | Pending |
| MOD-02 | — | Pending |
| MOD-03 | — | Pending |
| MOD-04 | — | Pending |
| BQ-01 | — | Pending |
| BQ-02 | — | Pending |
| BQ-03 | — | Pending |
| BQ-04 | — | Pending |
| BQ-05 | — | Pending |

**Coverage:**
- v1 requirements: 20 total
- Mapped to phases: 0
- Unmapped: 20

---
*Requirements defined: 2026-05-12*
*Last updated: 2026-05-12 after initial definition*
