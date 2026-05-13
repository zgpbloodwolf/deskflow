# Phase 5: Build Validation & Regression - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-13
**Phase:** 5-Build Validation & Regression
**Areas discussed:** Windows 构建环境, 测试清理策略, 冒烟测试方式, 警告处理策略

---

## Windows 构建环境

| Option | Description | Selected |
|--------|-------------|----------|
| 本地 Windows 机器 | 物理机可用，本地构建速度快，可直接运行 GUI/剪贴板/拖拽测试 | ✓ |
| Mac 上的虚拟机 | Parallels/UTM/VMware，性能受限，剪贴板/拖拽可能受虚拟化影响 | |
| 仅 CI/CD 自动化 | GitHub Actions 等 CI，手动测试仍需本地环境 | |
| 暂无 Windows 环境 | 先只验证 macOS，Windows 推迟 | |

**User's choice:** 本地 Windows 机器

| Option | Description | Selected |
|--------|-------------|----------|
| 已配置完成 | MSVC + CMake + vcpkg 已就绪 | |
| 部分配置 | 有 Visual Studio 但缺 CMake/vcpkg | |
| 未配置 | 需要从零开始安装 | |
| 你来决定 | Claude 决定构建配置细节 | ✓ |

**User's choice:** 你来决定（Claude's Discretion）

| Option | Description | Selected |
|--------|-------------|----------|
| Git 同步 | 两台机器 clone 同一仓库，通过 git push/pull 同步 | ✓ |
| 共享文件夹 | SMB/NFS 挂载同一目录 | |
| 网络直连测试 | 通过 Deskflow 本身进行 KVM 测试 | |

**User's choice:** Git 同步

| Option | Description | Selected |
|--------|-------------|----------|
| 手动同步+构建 | Mac push → Windows pull → 本地构建 | ✓ |
| CI 自动构建+手动测试 | GitHub Actions 自动构建，冒烟测试手动 | |
| 你来决定 | Claude 决定工作流程 | |

**User's choice:** 手动同步+构建

---

## 测试清理策略

| Option | Description | Selected |
|--------|-------------|----------|
| 随 Phase 3 一起删除 | Linux 测试文件随 Linux 平台源码一起移除，最干净 | ✓ |
| 保留但依赖条件编译 | 保留文件，依赖 CMake if(UNIX) 守卫跳过 | |
| 你来决定 | Claude 决定处理方式 | |

**User's choice:** 随 Phase 3 一起删除

| Option | Description | Selected |
|--------|-------------|----------|
| 需要更新以匹配新 API | Legacy tests 中的 Thread 引用需适配 Phase 4 的 std::jthread | ✓ |
| 先验证，按需修复 | 先运行测试，有问题再修 | |
| 你来决定 | Claude 决定处理方式 | |

**User's choice:** 需要更新以匹配新 API

| Option | Description | Selected |
|--------|-------------|----------|
| 统一在 Phase 5 修复 | 所有测试修复集中在此阶段处理 | ✓ |
| 每个 Phase 自行负责 | 每个 Phase 完成时确保测试通过 | |
| 你来决定 | Claude 决定处理方式 | |

**User's choice:** 统一在 Phase 5 修复

---

## 冒烟测试方式

| Option | Description | Selected |
|--------|-------------|----------|
| 双机实景测试 | Mac 作服务器，Windows 作客户端（或反过来），实际验证鼠标穿越/剪贴板/拖拽 | ✓ |
| 单机验证+双机测网 | 单机先验证 GUI，网络功能单独测试 | |
| 你来决定 | Claude 决定测试方式 | |

**User's choice:** 双机实景测试

| Option | Description | Selected |
|--------|-------------|----------|
| 检查清单式 | 完整的检查清单，逐步验证每个操作 | |
| 仅验证成功标准 | 只验证 5 条 Success Criteria | |
| 你来决定 | Claude 决定测试详细程度 | ✓ |

**User's choice:** 你来决定（Claude's Discretion）

| Option | Description | Selected |
|--------|-------------|----------|
| 发现即修复 | 冒烟测试发现问题直接修复后重新测试 | |
| 仅记录问题 | 只记录，不修复 | |
| 你来决定 | Claude 决定处理方式 | ✓ |

**User's choice:** 你来决定（Claude's Discretion）

---

## 警告处理策略

| Option | Description | Selected |
|--------|-------------|----------|
| 零警告但不开启 -Werror | 项目代码零警告，第三方库用 SYSTEM include 压制 | |
| 开启 -Werror | 所有警告视为错误 | |
| 你来决定 | Claude 决定处理方式 | ✓ |

**User's choice:** 你来决定（Claude's Discretion → 建议零警告但不开启 -Werror）

| Option | Description | Selected |
|--------|-------------|----------|
| 仅 Release | 只验证 Release 配置 | |
| Debug + Release | 两种配置都验证 | |
| 你来决定 | Claude 决定配置范围 | ✓ |

**User's choice:** 你来决定（Claude's Discretion → 建议 Release 优先）

---

## Claude's Discretion

- Windows 构建环境的具体配置步骤（MSVC、CMake、vcpkg、Qt 6 安装）
- 冒烟测试检查清单的具体内容
- 冒烟测试发现问题的修复流程
- 构建配置范围（Release 优先，Debug 可选）
- 警告处理的具体方式（零警告目标，SYSTEM include 压制第三方，不开 -Werror）

## Deferred Ideas

None — discussion stayed within phase scope
