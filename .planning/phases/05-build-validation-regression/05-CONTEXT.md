# Phase 5: Build Validation & Regression - Context

**Gathered:** 2026-05-13
**Status:** Ready for planning

<domain>
## Phase Boundary

验证经过 Phase 1-4 的代码重构后，应用在 macOS (clang) 和 Windows (MSVC) 双平台上能正确构建、运行且无功能回归。

**包含：**
- 修复 macOS 和 Windows 上的所有构建错误和警告
- 运行并修复所有现有自动化测试（Qt Test + Legacy Google Test）
- 手动冒烟测试 GUI 配置界面
- 双机实景测试剪贴板共享（文本+图片）
- 双机实景测试文件拖拽传输

**不包含：**
- 新功能开发
- 性能优化（已在 Phase 1 完成）
- 代码重构（已在 Phase 2-4 完成）
- CI/CD 流水线搭建

</domain>

<decisions>
## Implementation Decisions

### Windows 构建环境
- **D-01:** 使用本地 Windows 物理机构建和测试
- **D-02:** Windows 构建环境配置细节（MSVC + CMake + vcpkg）由 Claude 根据实际情况决定
- **D-03:** Mac 和 Windows 之间通过 Git 同步代码
- **D-04:** 手动同步+手动构建的工作流程——Mac 修改代码 → git push → Windows pull → 本地构建

### 测试清理策略
- **D-05:** Linux 相关测试文件（WlClipboardTests、XWindowsClipboardTests、X11LayoutParserTests）随 Phase 3 移除 Linux 支持时一起删除，Phase 5 时这些文件已不存在
- **D-06:** Legacy tests 中引用旧 Thread 类的测试文件（ArchNetworkBSDTests.cpp）需要更新以匹配 Phase 4 的 std::jthread 新 API
- **D-07:** 所有测试修复统一在 Phase 5 处理——不要求 Phase 2-4 每个阶段完成时测试都通过，Phase 5 集中修复所有积累的测试问题

### 冒烟测试方式
- **D-08:** 使用双机实景测试——Mac 和 Windows 通过局域网直连，一台作为服务器一台作为客户端
- **D-09:** 冒烟测试覆盖范围和检查清单由 Claude 决定（建议：基于 Success Criteria 的 5 条目制定检查清单，每条有明确的通过标准）
- **D-10:** 冒烟测试发现回归问题时在 Phase 5 中直接修复后重新测试，直到全部通过

### 警告处理策略
- **D-11:** 项目代码零警告目标，不开启 -Werror。第三方库警告已通过 SYSTEM include 目录压制（如 Asio）
- **D-12:** 构建配置范围由 Claude 决定（建议：优先验证 Release 配置，Debug 配置如有余力也验证）
- **D-13:** 仅验证 macOS (clang) 和 Windows (MSVC) 两个平台的构建，不涉及其他编译器

### Claude's Discretion
- Windows 构建环境的具体配置步骤和依赖安装
- 冒烟测试检查清单的具体内容（基于 Success Criteria 5 条目展开）
- 冒烟测试发现问题的修复流程（发现→定位→修复→重测→通过）
- 构建配置范围（Release 优先，Debug 可选）
- 第三方警告的具体处理方式（优先 SYSTEM include 压制，必要时 pragma disable）

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 构建系统
- `CMakeLists.txt` — 根构建配置，定义项目结构、依赖、编译选项
- `cmake/Libraries.cmake` — 外部依赖管理（Qt、Asio、vcpkg 等）
- `cmake/FetchAsio.cmake` — Asio 集成，使用 SYSTEM include 压制第三方警告
- `src/CMakeLists.txt` — 源码子目录入口，添加 lib/、apps/、unittests/
- `src/lib/CMakeLists.txt` — 库目标定义
- `src/unittests/CMakeLists.txt` — 测试基础设施，create_test() 函数定义

### 测试框架
- `src/unittests/platform/CMakeLists.txt` — 平台测试的条件编译配置（if(WIN32)/elseif(APPLE)/elseif(UNIX)）
- `src/unittests/legacytests/CMakeLists.txt` — Legacy Google Test 配置

### 应用入口
- `src/apps/deskflow-gui/deskflow-gui.cpp` — GUI 应用入口
- `src/apps/deskflow-core/deskflow-core.cpp` — 核心 client/server 入口

### 配置与设置
- `src/lib/common/Settings.h` — 所有设置键和默认值
- `src/lib/gui/MainWindow.h/.cpp` — GUI 主窗口

### 项目约束
- `.planning/codebase/TESTING.md` — 测试框架、模式、运行命令完整参考
- `.planning/codebase/STRUCTURE.md` — 目录结构和文件位置参考
- `.planning/codebase/CONCERNS.md` — 已知代码质量问题

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `create_test()` CMake 函数: 已有成熟的测试注册机制，新测试或修复测试可以直接使用
- Qt Test 框架: 现代测试基础设施已完善（QCOMPARE, QSignalSpy 等）
- 平台条件编译: CMake 已有完善的平台条件编译模式（if(WIN32)/elseif(APPLE)），测试文件按平台自动包含/排除

### Established Patterns
- 两套测试体系: 现代 Qt Test（独立可执行）+ Legacy Google Test（单体可执行），修复时需匹配对应模式
- Asio SYSTEM include: 第三方依赖的警告压制已建立模式（FetchAsio.cmake），可作为其他第三方依赖的参考

### Integration Points
- `src/unittests/CMakeLists.txt` 的 `create_test()`: 新增或修改测试的注册入口
- `src/unittests/platform/CMakeLists.txt`: 平台测试的增删改入口
- `src/unittests/legacytests/`: Legacy 测试的 mock 和 shared 基础设施

</code_context>

<specifics>
## Specific Ideas

- 冒烟测试可以利用 Deskflow 自身的 KVM 功能来验证——Mac 作为服务器，Windows 作为客户端，实际操作鼠标穿越、剪贴板共享、拖拽文件
- Windows 构建环境可能需要从零配置，包括 Visual Studio、CMake、vcpkg、Qt 6 等依赖安装

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 5-Build Validation & Regression*
*Context gathered: 2026-05-13*
