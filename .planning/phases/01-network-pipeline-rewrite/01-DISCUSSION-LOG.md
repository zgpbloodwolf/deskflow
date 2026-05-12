# Phase 1: Network Pipeline Rewrite - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-12
**Phase:** 01-Network Pipeline Rewrite
**Areas discussed:** I/O 模型选择, 线程架构设计, 协议兼容性, 断连检测与恢复

---

## I/O 模型选择

### Q1: 网络 I/O 的底层实现选择哪种方案？

| Option | Description | Selected |
|--------|-------------|----------|
| Standalone Asio | header-only，跨平台 proactor 模式，C++20 协程友好，约 1.5MB | ✓ |
| 直接 OS API 封装 | non-blocking socket + select/poll/WSAPoll，无外部依赖但需处理跨平台差异 | |
| Qt 网络层 | QTcpSocket，已有依赖但事件循环冲突且信号槽开销大 | |

**User's choice:** Standalone Asio

### Q2: Asio 与现有 EventQueue 的关系怎么处理？

| Option | Description | Selected |
|--------|-------------|----------|
| Asio 仅处理 I/O | Asio io_context 独立线程，上层仍通过 EventQueue 调度，最小侵入 | ✓ |
| Asio 替代 EventQueue | 统一事件循环，改动大但长期简洁 | |
| 渐进式，先 I/O 后合并 | 先仅处理 I/O，Phase 4 再考虑合并 | |

**User's choice:** Asio 仅处理 I/O

### Q3: 有界 poll 超时和心跳定时用 Asio steady_timer 还是现有机制？

| Option | Description | Selected |
|--------|-------------|----------|
| Asio 定时器 | 与 I/O 线程同线程，避免额外调度 | ✓ |
| 现有定时机制 | 保持现有模式但多了一层间接调用 | |

**User's choice:** Asio steady_timer

---

## 线程架构设计

### Q1: 网络 I/O 线程模型怎么设计？

| Option | Description | Selected |
|--------|-------------|----------|
| 每连接单线程 | 一个 Asio io_context 线程处理发送和接收，SPSC 用于跨线程传递，简单高效 | ✓ |
| 共享线程池 | 共享 io_context + 线程池，灵活但复杂 | |
| 分离收发线程 | 发送和接收独立线程，隔离强但调度开销大 | |

**User's choice:** 每连接单线程

### Q2: Lock-free SPSC 缓冲区如何组织？

| Option | Description | Selected |
|--------|-------------|----------|
| 分离队列 | 键盘 FIFO + 鼠标 atomic 最新位置槽，简单直接 | ✓ |
| 统一队列 + 合并逻辑 | 所有事件走一个队列，鼠标通过标记覆盖合并，处理逻辑复杂 | |

**User's choice:** 分离队列

### Q3: Asio I/O 完成后如何通知上层？

| Option | Description | Selected |
|--------|-------------|----------|
| 桥接到 EventQueue | Asio 完成后将数据投递到 EventQueue，上层处理逻辑不变 | ✓ |
| 直接回调 | Asio 直接调用 Server/Client 方法，延迟低但耦合大 | |

**User's choice:** 桥接到 EventQueue

---

## 协议兼容性

### Q1: 是否保持与现有 Deskflow/Barrier 二进制协议的兼容？

| Option | Description | Selected |
|--------|-------------|----------|
| 完全兼容 | 保持 ProtocolUtil::writef 帧格式不变，零风险 | ✓ |
| 扩展协议 | 添加扩展字段，需要版本协商 | |
| 全新协议 | 重新设计帧格式，性能最优但不兼容 | |

**User's choice:** 完全兼容

### Q2: 鼠标移动合并发生在哪一层？

| Option | Description | Selected |
|--------|-------------|----------|
| 缓冲区层合并 | SPSC 缓冲区覆盖旧位置，每次只发最新 DMOVM，协议不变 | ✓ |
| 协议层批量发送 | 添加批量消息类型，需修改协议 | |

**User's choice:** 缓冲区层合并

---

## 断连检测与恢复

### Q1: 连接断开如何检测？

| Option | Description | Selected |
|--------|-------------|----------|
| 读写失败检测 | 依赖 Asio 异步读写错误 + 超时备份，零额外流量 | ✓ |
| 心跳检测 | 定期发送心跳包，检测快但增加流量 | |
| 混合检测 | 两者结合，最可靠但最复杂 | |

**User's choice:** 读写失败检测

### Q2: 按键释放机制怎么实现？

| Option | Description | Selected |
|--------|-------------|----------|
| 发送端状态表 | 维护按下状态表，断连时遍历释放，简单可靠 | ✓ |
| 接收端超时保护 | 定时器检测未收到的 keyup 并自动释放 | |
| 双重保护 | 两者结合，最安全但复杂度高 | |

**User's choice:** 发送端状态表

### Q3: 断连后的恢复策略？

| Option | Description | Selected |
|--------|-------------|----------|
| 不自动重连 | 通知 GUI，用户手动操作 | |
| 自动重连 | 指数退避 + 自动恢复输入流 | ✓ |

**User's choice:** 自动重连

### Q4: 自动重连的具体策略？

| Option | Description | Selected |
|--------|-------------|----------|
| 指数退避 + 自动恢复 | 立即重试一次，之后 1s→2s→4s→最大 30s | ✓ |
| 固定间隔重试 | 每 5 秒重试，简单但可能加重负担 | |

**User's choice:** 指数退避 + 自动恢复

---

## Claude's Discretion

- SPSC 队列的具体容量大小
- Asio 引入方式（FetchContent vs 系统安装）
- 桥接层的具体实现细节

## Deferred Ideas

None — discussion stayed within phase scope
