# AGENTS.md

## 通用规范（所有项目共享）

### 运行环境
- 运行环境是 Windows，使用 Windows 支持的脚本或命令
- Python 使用当前项目根目录下的 `.venv` 环境
- 软件源尽量使用中国大陆镜像

### 依赖管理
- 仅允许使用商业友好的第三方库
- 遵循语义化版本（SemVer）
- 版本变更需同步记录到 CHANGELOG.md
- python 项目日志使用loguru库

### 代码质量
- 提交前必须通过 lint 与编译，保持 0 warning
- 新增或修改逻辑必须配套测试
- 测试失败不得绕过或删除

### Git 规范
- 提交消息遵循 Conventional Commits
- 标题遵守 52/70 字符限制
- 正文表达 what/why 而非 how，使用空行与 bulleted list

### 安全与合规
- 不得提交密钥或凭证类文件
- 涉及安全风险须主动提示

### 失败处理
- 困难问题连续 3 次尝试仍未解决，应停止并向人类报告

### AI Agent 工作规范

#### 对话语言
- 使用中文进行对话和回答
- 代码注释使用中文
- 代码解释使用中文
- 列表项使用中文描述

#### 提示词规范
生成提示词需要包含：
1. **指令**（必填）- 需要执行的任务
2. **输入**（必填）- 提供的输入数据
3. **输出**（必填）- 期望的输出格式
4. **注意事项** - 需要特别注意的事项

#### 任务执行
- 尽量减少与人的交互，自主做出选择
- 能同时启动多个 subagent 就启动多个
- 完成规划后搜索相关 skill，优先使用 skill 完成任务
- 充分利用多核和多 GPU 资源，并行处理大量数据

---

## 项目概述

**U2Claw** 是一个跨平台的 **Electron 桌面应用**，基于 **React 19 + Vite + TypeScript** 构建，为 OpenClaw AI agent 运行时提供图形化界面。

### 技术栈
- **前端框架**: React 19 + TypeScript
- **构建工具**: Vite
- **桌面框架**: Electron
- **状态管理**: Zustand
- **UI 组件**: Radix UI + Tailwind CSS
- **包管理器**: pnpm
- **后端**: Python (FastAPI) + uv

---

## 项目结构

```
u2-clawx/
├── src/                          # 渲染进程前端代码
│   ├── components/               # React 组件
│   │   ├── ui/                   # 基础 UI 组件 (shadcn/ui)
│   │   ├── layout/               # 布局组件
│   │   ├── auth/                 # 认证相关组件
│   │   ├── settings/             # 设置相关组件
│   │   └── ...
│   ├── pages/                    # 页面组件
│   ├── stores/                   # Zustand 状态管理
│   ├── lib/                      # 工具函数和 API 客户端
│   ├── types/                    # TypeScript 类型定义
│   ├── i18n/                     # 国际化
│   └── styles/                   # 全局样式
├── electron/                     # Electron 主进程代码
│   ├── main/                     # 主进程入口
│   ├── preload/                  # 预加载脚本
│   ├── api/                      # 本地 API 服务
│   ├── gateway/                  # OpenClaw 网关管理
│   ├── services/                 # 服务层
│   └── utils/                    # 工具函数
├── backend/                      # Python 后端服务
│   ├── app/                      # FastAPI 应用
│   ├── tests/                    # 测试文件
│   └── docs/                     # 文档
├── local_skills/                 # 本地技能
└── docs/                         # 项目文档
```

---

## 开发规范

### 前端开发 (React + TypeScript)

#### 组件规范
- 使用函数组件 + Hooks
- 组件文件使用 PascalCase 命名（如 `ChatMessage.tsx`）
- 组件props必须定义类型接口
- 使用 `React.FC` 或显式返回类型

```typescript
// 正确示例
interface ChatMessageProps {
  content: string;
  timestamp: Date;
}

export const ChatMessage: React.FC<ChatMessageProps> = ({ content, timestamp }) => {
  return <div>{content}</div>;
};
```

#### 状态管理
- 使用 Zustand 进行全局状态管理
- store 文件位于 `src/stores/`
- 按功能模块拆分 store

#### IPC 通信规范
- **禁止**直接使用 `window.electron.ipcRenderer.invoke`
- 必须使用 `src/lib/api-client.ts` 中的 `invokeIpc` 函数
- ESLint 会拦截违规调用

```typescript
// 正确示例
import { invokeIpc } from '@/lib/api-client';

const result = await invokeIpc('channel-name', data);
```

### Electron 开发

#### 主进程规范
- 主进程代码位于 `electron/main/`
- IPC 处理器定义在 `electron/main/ipc-handlers.ts`
- 使用类型安全的 IPC 通道名

#### 预加载脚本
- 预加载脚本位于 `electron/preload/`
- 仅暴露必要的 API 到渲染进程
- 使用 `contextBridge` 进行安全隔离

### Python 后端开发

#### 代码规范
- 遵循 PEP 8 规范
- 使用类型注解
- 异步函数使用 `async/await`

#### 项目结构
- 路由位于 `backend/app/routers/`
- 模型位于 `backend/app/models/`
- 服务层位于 `backend/app/services/`

---

## 代码风格配置

### ESLint 配置
- 使用 `@typescript-eslint` 进行 TypeScript 检查
- 启用 `react-hooks` 和 `react-refresh` 规则
- 未使用变量需以 `_` 开头

### Prettier 配置
```json
{
  "semi": true,
  "singleQuote": true,
  "tabWidth": 2,
  "trailingComma": "es5",
  "printWidth": 100
}
```

---

## 常用命令

| 任务 | 命令 |
|------|------|
| 安装依赖 + 下载 uv | `pnpm run init` |
| 开发服务器 (Vite + Electron) | `pnpm dev` |
| 代码检查 (ESLint, auto-fix) | `pnpm run lint` |
| 类型检查 | `pnpm run typecheck` |
| 单元测试 (Vitest) | `pnpm test` |
| E2E 测试 (Playwright) | `pnpm run test:e2e` |
| 构建前端 | `pnpm run build:vite` |
| 构建完整应用 | `pnpm run build` |
| 打包 Windows 版本 | `pnpm run package:win` |
| 打包 Mac 版本 | `pnpm run package:mac` |

---

## 文档规范

### 必须维护的文档
- `CHANGELOG.md` - 版本变更日志
- `BACKLOG.md` - 待办事项
- `CHECKLIST.md` - 检查清单
- `RELEASE-NOTE.md` - 发布说明

### 代码注释
- 使用中文进行代码注释
- 复杂逻辑必须添加注释说明
- 公共 API 必须添加 JSDoc 注释

---

## 代码提交前检查清单
- [ ] 通过 `pnpm run lint` 检查
- [ ] 通过 `pnpm run typecheck` 类型检查
- [ ] 相关测试通过
- [ ] 无密钥或敏感信息泄露
