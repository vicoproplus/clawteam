# ClawTeam 前端 UI 扩展 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 构建 ClawTeam 的前端 UI 扩展，基于 MoonClaw UI 复用，新增多 Agent 聊天界面 (clawteam-chat)。

**架构：** React 19 + Redux Toolkit + Tailwind CSS 4，复用 MoonClaw UI core 和 web 基础架构，新增 clawteam-chat 子项目。

**技术栈：** React 19.2, TypeScript 5.9, Vite 7.2, Tailwind CSS 4.1, Redux Toolkit 2.10, Radix UI, Lucide React

---

## 文件结构

### 新建文件

| 文件 | 职责 |
|------|------|
| `ui/package.json` | Monorepo 根配置 |
| `ui/pnpm-workspace.yaml` | pnpm workspace 配置 |
| `ui/.nvmrc` | Node 版本 |
| `ui/.prettierrc` | Prettier 配置 |
| `ui/.prettierignore` | Prettier 忽略 |
| `ui/.env.example` | 环境变量示例 |
| `ui/.env.development` | 开发环境变量 |
| `ui/.env.production` | 生产环境变量 |

### 从 MoonClaw UI 复制

| 源路径 | 目标路径 | 修改 |
|--------|----------|------|
| `reference/moonclaw/ui/core/` | `ui/core/` | 完全复制 |
| `ui/clawteam-chat/` | 新建 | 多 Agent 聊天扩展 |

### clawteam-chat 新建文件

```
ui/clawteam-chat/
├── package.json
├── tsconfig.json
├── tsconfig.node.json
├── vite.config.ts
├── tailwind.config.ts
├── postcss.config.cjs
├── index.html
├── src/
│   ├── main.tsx
│   ├── App.tsx
│   ├── index.css
│   ├── components/
│   │   ├── AgentPanel.tsx
│   │   ├── AgentCard.tsx
│   │   ├── ChatInterface.tsx
│   │   ├── MessageList.tsx
│   │   ├── ChatInput.tsx
│   │   ├── OutputPanel.tsx
│   │   ├── SkillStore.tsx
│   │   ├── AgentConfig.tsx
│   │   └── ui/
│   │       ├── button.tsx
│   │       ├── card.tsx
│   │       ├── input.tsx
│   │       ├── scroll-area.tsx
│   │       └── tabs.tsx
│   ├── store/
│   │   ├── index.ts
│   │   ├── chatSlice.ts
│   │   ├── agentSlice.ts
│   │   ├── skillSlice.ts
│   │   └── auditSlice.ts
│   ├── hooks/
│   │   ├── useWebSocket.ts
│   │   ├── useAgent.ts
│   │   └── useChat.ts
│   ├── types/
│   │   ├── agent.ts
│   │   ├── skill.ts
│   │   ├── message.ts
│   │   └── audit.ts
│   ├── api/
│   │   └── gateway.ts
│   └── lib/
│       └── utils.ts
```

---

## 任务分解

### 任务 1：复制 UI 基础架构

**文件：**
- 复制：`reference/moonclaw/ui/` → `ui/`

- [ ] **步骤 1：复制 UI 根配置文件**

```bash
mkdir -p ui
cp reference/moonclaw/ui/package.json ui/
cp reference/moonclaw/ui/pnpm-workspace.yaml ui/
cp reference/moonclaw/ui/.nvmrc ui/
cp reference/moonclaw/ui/.prettierrc ui/
cp reference/moonclaw/ui/.prettierignore ui/
cp reference/moonclaw/ui/.env.example ui/
cp reference/moonclaw/ui/.env.development ui/
cp reference/moonclaw/ui/.env.production ui/
```

- [ ] **步骤 2：修改根 package.json**

将名称改为 `clawteam-ui-workspace`：

```json
{
  "name": "clawteam-ui-workspace",
  "packageManager": "pnpm@9.15.9"
}
```

- [ ] **步骤 3：复制 core 模块**

```bash
cp -r reference/moonclaw/ui/core ui/core
```

- [ ] **步骤 4：修改 core/package.json**

将名称改为 `@clawteam/core`：

```json
{
  "name": "@clawteam/core",
  "version": "0.1.0"
}
```

- [ ] **步骤 5：安装依赖**

```bash
cd ui
pnpm install
```

- [ ] **步骤 6：验证 core 构建**

运行：`cd ui/core && pnpm build`
预期：PASS

- [ ] **步骤 7：Commit**

```bash
git add ui/package.json ui/pnpm-workspace.yaml ui/core/
git commit -m "feat: copy UI core from moonclaw"
```

---

### 任务 2：创建 clawteam-chat 项目骨架

**文件：**
- 创建：`ui/clawteam-chat/package.json`
- 创建：`ui/clawteam-chat/tsconfig.json`
- 创建：`ui/clawteam-chat/tsconfig.node.json`
- 创建：`ui/clawteam-chat/vite.config.ts`
- 创建：`ui/clawteam-chat/tailwind.config.ts`
- 创建：`ui/clawteam-chat/postcss.config.cjs`
- 创建：`ui/clawteam-chat/index.html`

- [ ] **步骤 1：创建 package.json**

```json
{
  "name": "clawteam-chat",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "lint": "eslint .",
    "preview": "vite preview",
    "check": "tsc -b --noEmit"
  },
  "dependencies": {
    "@clawteam/core": "workspace:^",
    "@reduxjs/toolkit": "^2.10.1",
    "@tailwindcss/vite": "^4.1.17",
    "clsx": "^2.1.1",
    "lucide-react": "^0.553.0",
    "react": "^19.2.0",
    "react-dom": "^19.2.0",
    "react-redux": "^9.2.0",
    "react-router": "^7.9.5",
    "tailwind-merge": "^3.4.0",
    "tailwindcss": "^4.1.17"
  },
  "devDependencies": {
    "@eslint/js": "^9.39.1",
    "@types/node": "^24.10.0",
    "@types/react": "^19.2.2",
    "@types/react-dom": "^19.2.2",
    "@vitejs/plugin-react": "^5.1.0",
    "eslint": "^9.39.1",
    "eslint-plugin-react-hooks": "^5.2.0",
    "eslint-plugin-react-refresh": "^0.4.24",
    "globals": "^16.5.0",
    "typescript": "~5.9.3",
    "typescript-eslint": "^8.46.3",
    "vite": "^7.2.2"
  }
}
```

- [ ] **步骤 2：创建 tsconfig.json**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "useDefineForClassFields": true,
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "react-jsx",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "baseUrl": ".",
    "paths": {
      "@/*": ["./src/*"]
    }
  },
  "include": ["src"],
  "references": [{ "path": "./tsconfig.node.json" }]
}
```

- [ ] **步骤 3：创建 tsconfig.node.json**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2023"],
    "module": "ESNext",
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true
  },
  "include": ["vite.config.ts"]
}
```

- [ ] **步骤 4：创建 vite.config.ts**

```typescript
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import path from 'path'

export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    port: 5174,
    proxy: {
      '/api': {
        target: 'http://localhost:3000',
        changeOrigin: true,
      },
      '/ws': {
        target: 'ws://localhost:3000',
        ws: true,
      },
    },
  },
})
```

- [ ] **步骤 5：创建 index.html**

```html
<!doctype html>
<html lang="zh-CN">
  <head>
    <meta charset="UTF-8" />
    <link rel="icon" type="image/svg+xml" href="/vite.svg" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>ClawTeam - Multi-Agent CLI Orchestrator</title>
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
```

- [ ] **步骤 6：创建入口文件 src/main.tsx**

```tsx
import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { Provider } from 'react-redux'
import { BrowserRouter } from 'react-router'
import App from './App'
import { store } from './store'
import './index.css'

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <Provider store={store}>
      <BrowserRouter>
        <App />
      </BrowserRouter>
    </Provider>
  </StrictMode>,
)
```

- [ ] **步骤 7：创建 App.tsx**

```tsx
import { Routes, Route } from 'react-router'
import { ChatInterface } from './components/ChatInterface'

function App() {
  return (
    <div className="h-screen bg-gray-50">
      <Routes>
        <Route path="/" element={<ChatInterface />} />
        <Route path="/session/:sessionId" element={<ChatInterface />} />
      </Routes>
    </div>
  )
}

export default App
```

- [ ] **步骤 8：创建 index.css**

```css
@import 'tailwindcss';

:root {
  font-family: Inter, system-ui, Avenir, Helvetica, Arial, sans-serif;
}

body {
  margin: 0;
  padding: 0;
  min-height: 100vh;
}

#root {
  min-height: 100vh;
}
```

- [ ] **步骤 9：安装依赖并验证**

```bash
cd ui
pnpm install
cd clawteam-chat
pnpm check
```

- [ ] **步骤 10：Commit**

```bash
git add ui/clawteam-chat/
git commit -m "feat: add clawteam-chat project skeleton"
```

---

### 任务 3：创建类型定义

**文件：**
- 创建：`ui/clawteam-chat/src/types/agent.ts`
- 创建：`ui/clawteam-chat/src/types/skill.ts`
- 创建：`ui/clawteam-chat/src/types/message.ts`
- 创建：`ui/clawteam-chat/src/types/audit.ts`

- [ ] **步骤 1：创建 agent.ts**

```typescript
// Agent 角色定义
export type AgentRole = 'assistant' | 'worker' | 'supervisor'

// 终端会话状态
export type TerminalSessionStatus = 'pending' | 'online' | 'working' | 'offline' | 'broken'

// 派发策略
export type DispatchStrategy = 'round-robin' | 'least-loaded' | 'capability-match' | 'manual'

// 角色配置
export interface AssistantConfig {
  availableWorkers: string[]
  availableSupervisors: string[]
  dispatchStrategy: DispatchStrategy
}

export interface WorkerConfig {
  assistantId?: string
  capabilities: string[]
}

export interface SupervisorConfig {
  assistantId?: string
  reviewCriteria: string[]
}

export type RoleConfig = 
  | { type: 'assistant'; config: AssistantConfig }
  | { type: 'worker'; config: WorkerConfig }
  | { type: 'supervisor'; config: SupervisorConfig }

// Agent 配置
export interface AgentConfig {
  id: string
  name: string
  role: AgentRole
  cliTool: string
  args: string[]
  env: Record<string, string>
  cwd?: string
  autoStart: boolean
  roleConfig?: RoleConfig
  skills: string[]
  enabled: boolean
}

// Agent 运行时状态
export interface AgentState {
  config: AgentConfig
  status: TerminalSessionStatus
  lastOutputAt: number
  workingSince?: number
  sessionId?: string
}

// Agent 面板显示数据
export interface AgentPanelItem {
  id: string
  name: string
  role: AgentRole
  status: TerminalSessionStatus
  cliTool: string
  required: boolean // 助手为必须，其他为可选
}
```

- [ ] **步骤 2：创建 skill.ts**

```typescript
// Skill 变量类型
export type SkillVarType = 'text' | 'number' | 'file' | 'directory' | 'select'

export interface SkillVariable {
  name: string
  label: string
  type: SkillVarType
  required: boolean
  default?: string
  options?: string[] // for select type
}

// Skill 执行模板
export interface SkillTemplate {
  messageTemplate: string
  variables: SkillVariable[]
  timeoutMs?: number
}

// Skill 定义
export interface Skill {
  id: string
  name: string
  nameKey: string
  description: string
  icon: string
  color: string
  bg: string
  ring: string
  version: string
  tags: string[]
  applicableRoles: string[]
  template: SkillTemplate
}
```

- [ ] **步骤 3：创建 message.ts**

```typescript
// 消息状态
export type MessageStatus = 'pending' | 'dispatched' | 'processing' | 'completed' | 'failed'

// 消息发送者类型
export type SenderType = 'user' | 'assistant' | 'worker' | 'supervisor' | 'system'

// 聊天消息
export interface ChatMessage {
  id: string
  sessionId: string
  senderType: SenderType
  senderId: string
  senderName: string
  content: string
  status: MessageStatus
  createdAt: number
  linkedAgent?: string
  metadata?: Record<string, unknown>
}

// CLI 输出块
export interface CliOutputChunk {
  sessionId: string
  agentId: string
  type: 'stdout' | 'stderr'
  content: string
  timestamp: number
}

// 会话
export interface Session {
  id: string
  name: string
  createdAt: number
  updatedAt: number
  messages: ChatMessage[]
}
```

- [ ] **步骤 4：创建 audit.ts**

```typescript
// 审计事件类型
export type AuditEventType =
  | 'job-triggered'
  | 'job-completed'
  | 'cli-started'
  | 'cli-completed'
  | 'cli-failed'
  | 'cli-killed'
  | 'output-captured'
  | 'user-paused'
  | 'user-resumed'
  | 'user-cancelled'
  | 'process-timeout'
  | 'output-error'

// 审计日志条目
export interface AuditLogEntry {
  id: string
  timestamp: number
  eventType: AuditEventType
  sessionId?: string
  targetId?: string
  jobId?: string
  runId?: string
  userId?: string
  channel?: string
  details: Record<string, unknown>
}
```

- [ ] **步骤 5：运行类型检查**

运行：`cd ui/clawteam-chat && pnpm check`
预期：PASS

- [ ] **步骤 6：Commit**

```bash
git add ui/clawteam-chat/src/types/
git commit -m "feat: add type definitions for clawteam-chat"
```

---

### 任务 4：创建 Redux Store

**文件：**
- 创建：`ui/clawteam-chat/src/store/index.ts`
- 创建：`ui/clawteam-chat/src/store/chatSlice.ts`
- 创建：`ui/clawteam-chat/src/store/agentSlice.ts`
- 创建：`ui/clawteam-chat/src/store/skillSlice.ts`

- [ ] **步骤 1：创建 chatSlice.ts**

```typescript
import { createSlice, PayloadAction } from '@reduxjs/toolkit'
import type { ChatMessage, Session, MessageStatus } from '../types/message'

interface ChatState {
  sessions: Record<string, Session>
  currentSessionId: string | null
  messages: Record<string, ChatMessage[]>
  pendingMessages: string[]
}

const initialState: ChatState = {
  sessions: {},
  currentSessionId: null,
  messages: {},
  pendingMessages: [],
}

const chatSlice = createSlice({
  name: 'chat',
  initialState,
  reducers: {
    createSession: (state, action: PayloadAction<Session>) => {
      state.sessions[action.payload.id] = action.payload
      state.messages[action.payload.id] = []
    },
    setCurrentSession: (state, action: PayloadAction<string>) => {
      state.currentSessionId = action.payload
    },
    addMessage: (state, action: PayloadAction<ChatMessage>) => {
      const sessionId = action.payload.sessionId
      if (!state.messages[sessionId]) {
        state.messages[sessionId] = []
      }
      state.messages[sessionId].push(action.payload)
      if (state.sessions[sessionId]) {
        state.sessions[sessionId].updatedAt = Date.now()
      }
    },
    updateMessageStatus: (
      state,
      action: PayloadAction<{ messageId: string; status: MessageStatus }>
    ) => {
      for (const sessionId in state.messages) {
        const msg = state.messages[sessionId].find(m => m.id === action.payload.messageId)
        if (msg) {
          msg.status = action.payload.status
          break
        }
      }
    },
    addPendingMessage: (state, action: PayloadAction<string>) => {
      state.pendingMessages.push(action.payload)
    },
    removePendingMessage: (state, action: PayloadAction<string>) => {
      state.pendingMessages = state.pendingMessages.filter(id => id !== action.payload)
    },
  },
})

export const {
  createSession,
  setCurrentSession,
  addMessage,
  updateMessageStatus,
  addPendingMessage,
  removePendingMessage,
} = chatSlice.actions

export default chatSlice.reducer
```

- [ ] **步骤 2：创建 agentSlice.ts**

```typescript
import { createSlice, PayloadAction } from '@reduxjs/toolkit'
import type { AgentConfig, AgentState, TerminalSessionStatus } from '../types/agent'

interface AgentStateSlice {
  agents: Record<string, AgentState>
  agentList: string[]
  selectedAgentId: string | null
}

const initialState: AgentStateSlice = {
  agents: {},
  agentList: [],
  selectedAgentId: null,
}

const agentSlice = createSlice({
  name: 'agent',
  initialState,
  reducers: {
    setAgents: (state, action: PayloadAction<AgentConfig[]>) => {
      state.agents = {}
      state.agentList = []
      for (const config of action.payload) {
        state.agents[config.id] = {
          config,
          status: 'offline',
          lastOutputAt: 0,
        }
        state.agentList.push(config.id)
      }
    },
    updateAgentStatus: (
      state,
      action: PayloadAction<{ agentId: string; status: TerminalSessionStatus }>
    ) => {
      const agent = state.agents[action.payload.agentId]
      if (agent) {
        agent.status = action.payload.status
        if (action.payload.status === 'working') {
          agent.workingSince = Date.now()
        } else {
          agent.workingSince = undefined
        }
      }
    },
    selectAgent: (state, action: PayloadAction<string>) => {
      state.selectedAgentId = action.payload
    },
    updateAgentSession: (
      state,
      action: PayloadAction<{ agentId: string; sessionId: string }>
    ) => {
      const agent = state.agents[action.payload.agentId]
      if (agent) {
        agent.sessionId = action.payload.sessionId
      }
    },
  },
})

export const { setAgents, updateAgentStatus, selectAgent, updateAgentSession } = agentSlice.actions

export default agentSlice.reducer
```

- [ ] **步骤 3：创建 skillSlice.ts**

```typescript
import { createSlice, PayloadAction } from '@reduxjs/toolkit'
import type { Skill } from '../types/skill'

interface SkillState {
  skills: Record<string, Skill>
  skillList: string[]
  selectedSkillId: string | null
  skillDialogOpen: boolean
}

const initialState: SkillState = {
  skills: {},
  skillList: [],
  selectedSkillId: null,
  skillDialogOpen: false,
}

const skillSlice = createSlice({
  name: 'skill',
  initialState,
  reducers: {
    setSkills: (state, action: PayloadAction<Skill[]>) => {
      state.skills = {}
      state.skillList = []
      for (const skill of action.payload) {
        state.skills[skill.id] = skill
        state.skillList.push(skill.id)
      }
    },
    selectSkill: (state, action: PayloadAction<string | null>) => {
      state.selectedSkillId = action.payload
    },
    openSkillDialog: state => {
      state.skillDialogOpen = true
    },
    closeSkillDialog: state => {
      state.skillDialogOpen = false
    },
  },
})

export const { setSkills, selectSkill, openSkillDialog, closeSkillDialog } = skillSlice.actions

export default skillSlice.reducer
```

- [ ] **步骤 4：创建 store/index.ts**

```typescript
import { configureStore } from '@reduxjs/toolkit'
import chatReducer from './chatSlice'
import agentReducer from './agentSlice'
import skillReducer from './skillSlice'

export const store = configureStore({
  reducer: {
    chat: chatReducer,
    agent: agentReducer,
    skill: skillReducer,
  },
})

export type RootState = ReturnType<typeof store.getState>
export type AppDispatch = typeof store.dispatch
```

- [ ] **步骤 5：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 6：Commit**

```bash
git add ui/clawteam-chat/src/store/
git commit -m "feat: add Redux store with chat, agent, skill slices"
```

---

### 任务 5：创建基础 UI 组件

**文件：**
- 创建：`ui/clawteam-chat/src/lib/utils.ts`
- 创建：`ui/clawteam-chat/src/components/ui/button.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/card.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/input.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/scroll-area.tsx`

- [ ] **步骤 1：创建 utils.ts**

```typescript
import { clsx, type ClassValue } from 'clsx'
import { twMerge } from 'tailwind-merge'

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}
```

- [ ] **步骤 2：创建 button.tsx**

```tsx
import * as React from 'react'
import { cn } from '@/lib/utils'

export interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: 'default' | 'destructive' | 'outline' | 'secondary' | 'ghost' | 'link'
  size?: 'default' | 'sm' | 'lg' | 'icon'
}

const buttonVariants = {
  default: 'bg-blue-600 text-white hover:bg-blue-700',
  destructive: 'bg-red-600 text-white hover:bg-red-700',
  outline: 'border border-gray-300 bg-white hover:bg-gray-50',
  secondary: 'bg-gray-100 text-gray-900 hover:bg-gray-200',
  ghost: 'hover:bg-gray-100',
  link: 'text-blue-600 underline-offset-4 hover:underline',
}

const buttonSizes = {
  default: 'h-10 px-4 py-2',
  sm: 'h-9 rounded-md px-3',
  lg: 'h-11 rounded-md px-8',
  icon: 'h-10 w-10',
}

const Button = React.forwardRef<HTMLButtonElement, ButtonProps>(
  ({ className, variant = 'default', size = 'default', ...props }, ref) => {
    return (
      <button
        className={cn(
          'inline-flex items-center justify-center rounded-md text-sm font-medium transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-blue-500 disabled:pointer-events-none disabled:opacity-50',
          buttonVariants[variant],
          buttonSizes[size],
          className
        )}
        ref={ref}
        {...props}
      />
    )
  }
)
Button.displayName = 'Button'

export { Button }
```

- [ ] **步骤 3：创建 card.tsx**

```tsx
import * as React from 'react'
import { cn } from '@/lib/utils'

const Card = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div
      ref={ref}
      className={cn('rounded-lg border bg-white shadow-sm', className)}
      {...props}
    />
  )
)
Card.displayName = 'Card'

const CardHeader = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div ref={ref} className={cn('flex flex-col space-y-1.5 p-6', className)} {...props} />
  )
)
CardHeader.displayName = 'CardHeader'

const CardTitle = React.forwardRef<HTMLParagraphElement, React.HTMLAttributes<HTMLHeadingElement>>(
  ({ className, ...props }, ref) => (
    <h3
      ref={ref}
      className={cn('text-lg font-semibold leading-none tracking-tight', className)}
      {...props}
    />
  )
)
CardTitle.displayName = 'CardTitle'

const CardContent = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div ref={ref} className={cn('p-6 pt-0', className)} {...props} />
  )
)
CardContent.displayName = 'CardContent'

export { Card, CardHeader, CardTitle, CardContent }
```

- [ ] **步骤 4：创建 input.tsx**

```tsx
import * as React from 'react'
import { cn } from '@/lib/utils'

export interface InputProps extends React.InputHTMLAttributes<HTMLInputElement> {}

const Input = React.forwardRef<HTMLInputElement, InputProps>(
  ({ className, type, ...props }, ref) => {
    return (
      <input
        type={type}
        className={cn(
          'flex h-10 w-full rounded-md border border-gray-300 bg-white px-3 py-2 text-sm placeholder:text-gray-400 focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:cursor-not-allowed disabled:opacity-50',
          className
        )}
        ref={ref}
        {...props}
      />
    )
  }
)
Input.displayName = 'Input'

export { Input }
```

- [ ] **步骤 5：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 6：Commit**

```bash
git add ui/clawteam-chat/src/lib/ ui/clawteam-chat/src/components/ui/
git commit -m "feat: add base UI components (button, card, input)"
```

---

### 任务 6：创建 AgentPanel 组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/AgentCard.tsx`
- 创建：`ui/clawteam-chat/src/components/AgentPanel.tsx`

- [ ] **步骤 1：创建 AgentCard.tsx**

```tsx
import { Bot, Loader2, CheckCircle, XCircle, Clock } from 'lucide-react'
import { cn } from '@/lib/utils'
import type { AgentRole, TerminalSessionStatus } from '@/types/agent'

interface AgentCardProps {
  id: string
  name: string
  role: AgentRole
  status: TerminalSessionStatus
  cliTool: string
  required: boolean
  selected?: boolean
  onClick?: () => void
}

const statusIcons: Record<TerminalSessionStatus, React.ReactNode> = {
  pending: <Clock className="h-4 w-4 text-gray-400" />,
  online: <CheckCircle className="h-4 w-4 text-green-500" />,
  working: <Loader2 className="h-4 w-4 text-blue-500 animate-spin" />,
  offline: <XCircle className="h-4 w-4 text-gray-400" />,
  broken: <XCircle className="h-4 w-4 text-red-500" />,
}

const roleLabels: Record<AgentRole, string> = {
  assistant: '助手',
  worker: '员工',
  supervisor: '监工',
}

const roleColors: Record<AgentRole, string> = {
  assistant: 'bg-blue-50 border-blue-200',
  worker: 'bg-green-50 border-green-200',
  supervisor: 'bg-purple-50 border-purple-200',
}

export function AgentCard({
  name,
  role,
  status,
  cliTool,
  required,
  selected,
  onClick,
}: AgentCardProps) {
  return (
    <div
      className={cn(
        'p-3 rounded-lg border cursor-pointer transition-all',
        roleColors[role],
        selected && 'ring-2 ring-blue-500',
        !required && 'opacity-80 hover:opacity-100'
      )}
      onClick={onClick}
    >
      <div className="flex items-center gap-2">
        <Bot className="h-5 w-5 text-gray-600" />
        <div className="flex-1 min-w-0">
          <div className="font-medium text-sm truncate">{name}</div>
          <div className="text-xs text-gray-500">
            {roleLabels[role]} · {cliTool}
          </div>
        </div>
        {statusIcons[status]}
      </div>
      {required && (
        <div className="mt-1 text-xs text-gray-400">必需</div>
      )}
    </div>
  )
}
```

- [ ] **步骤 2：创建 AgentPanel.tsx**

```tsx
import { Plus } from 'lucide-react'
import { useSelector, useDispatch } from 'react-redux'
import { AgentCard } from './AgentCard'
import { Button } from './ui/button'
import { selectAgent } from '@/store/agentSlice'
import type { RootState } from '@/store'
import type { AgentRole } from '@/types/agent'

export function AgentPanel() {
  const dispatch = useDispatch()
  const { agents, agentList, selectedAgentId } = useSelector((state: RootState) => state.agent)

  // 按角色分组
  const groupedAgents = agentList.reduce(
    (acc, id) => {
      const agent = agents[id]
      if (agent) {
        const role = agent.config.role
        if (!acc[role]) acc[role] = []
        acc[role].push(agent)
      }
      return acc
    },
    {} as Record<AgentRole, typeof agents[string][]>
  )

  const roleOrder: AgentRole[] = ['assistant', 'worker', 'supervisor']
  const roleLabels: Record<AgentRole, string> = {
    assistant: '助手',
    worker: '员工',
    supervisor: '监工',
  }

  return (
    <div className="w-64 border-r bg-gray-50 flex flex-col">
      <div className="p-4 border-b">
        <h2 className="font-semibold">Agent 成员</h2>
      </div>
      
      <div className="flex-1 overflow-auto p-4 space-y-4">
        {roleOrder.map(role => {
          const agentsInRole = groupedAgents[role]
          if (!agentsInRole || agentsInRole.length === 0) return null
          
          return (
            <div key={role}>
              <div className="text-xs font-medium text-gray-500 mb-2">
                {roleLabels[role]}
              </div>
              <div className="space-y-2">
                {agentsInRole.map(agent => (
                  <AgentCard
                    key={agent.config.id}
                    id={agent.config.id}
                    name={agent.config.name}
                    role={agent.config.role}
                    status={agent.status}
                    cliTool={agent.config.cliTool}
                    required={agent.config.role === 'assistant'}
                    selected={selectedAgentId === agent.config.id}
                    onClick={() => dispatch(selectAgent(agent.config.id))}
                  />
                ))}
              </div>
            </div>
          )
        })}
      </div>
      
      <div className="p-4 border-t">
        <Button variant="outline" size="sm" className="w-full">
          <Plus className="h-4 w-4 mr-2" />
          添加 Agent
        </Button>
      </div>
    </div>
  )
}
```

- [ ] **步骤 3：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add ui/clawteam-chat/src/components/AgentCard.tsx ui/clawteam-chat/src/components/AgentPanel.tsx
git commit -m "feat: add AgentPanel and AgentCard components"
```

---

### 任务 7：创建 ChatInterface 组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/MessageList.tsx`
- 创建：`ui/clawteam-chat/src/components/ChatInput.tsx`
- 创建：`ui/clawteam-chat/src/components/ChatInterface.tsx`

- [ ] **步骤 1：创建 MessageList.tsx**

```tsx
import { User, Bot, Wrench, Eye } from 'lucide-react'
import { cn } from '@/lib/utils'
import type { ChatMessage, SenderType } from '@/types/message'

interface MessageListProps {
  messages: ChatMessage[]
}

const senderIcons: Record<SenderType, React.ReactNode> = {
  user: <User className="h-4 w-4" />,
  assistant: <Bot className="h-4 w-4" />,
  worker: <Wrench className="h-4 w-4" />,
  supervisor: <Eye className="h-4 w-4" />,
  system: null,
}

const senderColors: Record<SenderType, string> = {
  user: 'bg-blue-100',
  assistant: 'bg-purple-100',
  worker: 'bg-green-100',
  supervisor: 'bg-orange-100',
  system: 'bg-gray-100',
}

export function MessageList({ messages }: MessageListProps) {
  return (
    <div className="flex-1 overflow-auto p-4 space-y-4">
      {messages.map(message => (
        <div
          key={message.id}
          className={cn(
            'flex gap-3 p-3 rounded-lg',
            message.senderType === 'user' ? 'flex-row-reverse' : 'flex-row'
          )}
        >
          <div
            className={cn(
              'flex-shrink-0 w-8 h-8 rounded-full flex items-center justify-center',
              senderColors[message.senderType]
            )}
          >
            {senderIcons[message.senderType]}
          </div>
          
          <div className={cn('flex-1', message.senderType === 'user' && 'text-right')}>
            <div className="flex items-center gap-2 mb-1">
              <span className="font-medium text-sm">{message.senderName}</span>
              <span className="text-xs text-gray-400">
                {new Date(message.createdAt).toLocaleTimeString()}
              </span>
            </div>
            <div
              className={cn(
                'inline-block px-3 py-2 rounded-lg text-sm',
                message.senderType === 'user'
                  ? 'bg-blue-500 text-white'
                  : 'bg-gray-100 text-gray-900'
              )}
            >
              {message.content}
            </div>
            {message.status === 'processing' && (
              <div className="text-xs text-gray-400 mt-1">处理中...</div>
            )}
            {message.linkedAgent && (
              <div className="text-xs text-gray-400 mt-1">
                → {message.linkedAgent}
              </div>
            )}
          </div>
        </div>
      ))}
    </div>
  )
}
```

- [ ] **步骤 2：创建 ChatInput.tsx**

```tsx
import { useState } from 'react'
import { Send, ChevronDown } from 'lucide-react'
import { Button } from './ui/button'
import { Input } from './ui/input'

interface ChatInputProps {
  onSend: (message: string, skillId?: string) => void
  disabled?: boolean
  skills: { id: string; name: string }[]
}

export function ChatInput({ onSend, disabled, skills }: ChatInputProps) {
  const [input, setInput] = useState('')
  const [showSkillMenu, setShowSkillMenu] = useState(false)
  const [selectedSkill, setSelectedSkill] = useState<string | null>(null)

  const handleSend = () => {
    if (input.trim() && !disabled) {
      onSend(input.trim(), selectedSkill || undefined)
      setInput('')
      setSelectedSkill(null)
    }
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      handleSend()
    }
  }

  return (
    <div className="border-t bg-white p-4">
      <div className="flex gap-2">
        <div className="relative">
          <Button
            variant="outline"
            size="sm"
            onClick={() => setShowSkillMenu(!showSkillMenu)}
            disabled={disabled}
          >
            {selectedSkill
              ? skills.find(s => s.id === selectedSkill)?.name
              : '选择 Skill'}
            <ChevronDown className="h-4 w-4 ml-1" />
          </Button>
          
          {showSkillMenu && (
            <div className="absolute bottom-full left-0 mb-1 w-48 bg-white border rounded-lg shadow-lg z-10">
              {skills.map(skill => (
                <button
                  key={skill.id}
                  className="w-full px-3 py-2 text-left text-sm hover:bg-gray-50"
                  onClick={() => {
                    setSelectedSkill(skill.id)
                    setShowSkillMenu(false)
                  }}
                >
                  {skill.name}
                </button>
              ))}
            </div>
          )}
        </div>
        
        <Input
          value={input}
          onChange={e => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="输入消息..."
          disabled={disabled}
          className="flex-1"
        />
        
        <Button onClick={handleSend} disabled={disabled || !input.trim()}>
          <Send className="h-4 w-4" />
        </Button>
      </div>
    </div>
  )
}
```

- [ ] **步骤 3：创建 ChatInterface.tsx**

```tsx
import { useSelector, useDispatch } from 'react-redux'
import { useState } from 'react'
import { AgentPanel } from './AgentPanel'
import { MessageList } from './MessageList'
import { ChatInput } from './ChatInput'
import { OutputPanel } from './OutputPanel'
import { addMessage, createSession } from '@/store/chatSlice'
import type { RootState } from '@/store'
import type { ChatMessage } from '@/types/message'

export function ChatInterface() {
  const dispatch = useDispatch()
  const { currentSessionId, messages } = useSelector((state: RootState) => state.chat)
  const { skills, skillList } = useSelector((state: RootState) => state.skill)
  
  const [showOutput, setShowOutput] = useState(false)

  // 确保有当前会话
  const sessionId = currentSessionId || 'default'
  const sessionMessages = messages[sessionId] || []

  const handleSend = (content: string, skillId?: string) => {
    // 创建消息
    const message: ChatMessage = {
      id: `msg-${Date.now()}`,
      sessionId,
      senderType: 'user',
      senderId: 'user',
      senderName: '用户',
      content,
      status: 'pending',
      createdAt: Date.now(),
      metadata: skillId ? { skillId } : undefined,
    }
    
    dispatch(addMessage(message))
    
    // TODO: 通过 WebSocket 发送到后端
    console.log('Send message:', content, 'skill:', skillId)
  }

  const skillOptions = skillList.map(id => ({
    id,
    name: skills[id]?.name || id,
  }))

  return (
    <div className="flex h-full">
      <AgentPanel />
      
      <div className="flex-1 flex flex-col">
        <div className="border-b px-4 py-3 flex items-center justify-between">
          <h1 className="font-semibold">ClawTeam</h1>
          <button
            className="text-sm text-gray-500 hover:text-gray-700"
            onClick={() => setShowOutput(!showOutput)}
          >
            {showOutput ? '隐藏输出' : '显示输出'}
          </button>
        </div>
        
        <MessageList messages={sessionMessages} />
        <ChatInput onSend={handleSend} skills={skillOptions} />
      </div>
      
      {showOutput && <OutputPanel />}
    </div>
  )
}
```

- [ ] **步骤 4：创建占位 OutputPanel.tsx**

```tsx
export function OutputPanel() {
  return (
    <div className="w-96 border-l bg-gray-900 text-gray-100 p-4 font-mono text-sm overflow-auto">
      <div className="text-gray-400 mb-2">CLI 输出</div>
      <div className="text-gray-500">
        等待输出...
      </div>
    </div>
  )
}
```

- [ ] **步骤 5：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 6：Commit**

```bash
git add ui/clawteam-chat/src/components/ChatInterface.tsx ui/clawteam-chat/src/components/MessageList.tsx ui/clawteam-chat/src/components/ChatInput.tsx ui/clawteam-chat/src/components/OutputPanel.tsx
git commit -m "feat: add ChatInterface with MessageList and ChatInput"
```

---

### 任务 8：创建 Hooks 和 API 层

**文件：**
- 创建：`ui/clawteam-chat/src/hooks/useWebSocket.ts`
- 创建：`ui/clawteam-chat/src/hooks/useAppDispatch.ts`
- 创建：`ui/clawteam-chat/src/api/gateway.ts`

- [ ] **步骤 1：创建 useAppDispatch.ts**

```typescript
import { useDispatch } from 'react-redux'
import type { AppDispatch } from '@/store'

export const useAppDispatch = () => useDispatch<AppDispatch>()
```

- [ ] **步骤 2：创建 useWebSocket.ts**

```typescript
import { useEffect, useRef, useCallback } from 'react'
import { useAppDispatch } from './useAppDispatch'
import { addMessage, updateMessageStatus } from '@/store/chatSlice'
import { updateAgentStatus } from '@/store/agentSlice'

interface WebSocketMessage {
  type: 'message' | 'status' | 'output' | 'agent_status'
  payload: unknown
}

export function useWebSocket(url: string) {
  const wsRef = useRef<WebSocket | null>(null)
  const dispatch = useAppDispatch()
  
  const connect = useCallback(() => {
    wsRef.current = new WebSocket(url)
    
    wsRef.current.onopen = () => {
      console.log('WebSocket connected')
    }
    
    wsRef.current.onmessage = (event) => {
      try {
        const data: WebSocketMessage = JSON.parse(event.data)
        handleMessage(data)
      } catch (e) {
        console.error('Failed to parse WebSocket message:', e)
      }
    }
    
    wsRef.current.onclose = () => {
      console.log('WebSocket disconnected, reconnecting...')
      setTimeout(connect, 3000)
    }
    
    wsRef.current.onerror = (error) => {
      console.error('WebSocket error:', error)
    }
  }, [url])
  
  const handleMessage = (data: WebSocketMessage) => {
    switch (data.type) {
      case 'message':
        dispatch(addMessage(data.payload as Parameters<typeof addMessage>[0]))
        break
      case 'status':
        dispatch(updateMessageStatus(data.payload as Parameters<typeof updateMessageStatus>[0]))
        break
      case 'agent_status':
        dispatch(updateAgentStatus(data.payload as Parameters<typeof updateAgentStatus>[0]))
        break
      case 'output':
        // TODO: 处理输出
        break
    }
  }
  
  useEffect(() => {
    connect()
    return () => {
      wsRef.current?.close()
    }
  }, [connect])
  
  const send = useCallback((message: unknown) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(message))
    }
  }, [])
  
  return { send }
}
```

- [ ] **步骤 3：创建 api/gateway.ts**

```typescript
import type { AgentConfig } from '@/types/agent'
import type { Skill } from '@/types/skill'

const API_BASE = '/api'

export async function fetchAgents(): Promise<AgentConfig[]> {
  const response = await fetch(`${API_BASE}/agents`)
  if (!response.ok) throw new Error('Failed to fetch agents')
  return response.json()
}

export async function fetchSkills(): Promise<Skill[]> {
  const response = await fetch(`${API_BASE}/skills`)
  if (!response.ok) throw new Error('Failed to fetch skills')
  return response.json()
}

export async function sendMessage(
  sessionId: string,
  content: string,
  skillId?: string
): Promise<{ messageId: string }> {
  const response = await fetch(`${API_BASE}/sessions/${sessionId}/messages`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ content, skillId }),
  })
  if (!response.ok) throw new Error('Failed to send message')
  return response.json()
}

export async function fetchSessionMessages(sessionId: string): Promise<unknown[]> {
  const response = await fetch(`${API_BASE}/sessions/${sessionId}/messages`)
  if (!response.ok) throw new Error('Failed to fetch messages')
  return response.json()
}
```

- [ ] **步骤 4：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/hooks/ ui/clawteam-chat/src/api/
git commit -m "feat: add WebSocket hook and gateway API"
```

---

### 任务 9：集成测试与构建

**文件：**
- 修改：`ui/clawteam-chat/src/App.tsx`
- 修改：`ui/clawteam-chat/src/main.tsx`

- [ ] **步骤 1：更新 App.tsx 添加初始化逻辑**

```tsx
import { useEffect } from 'react'
import { Routes, Route } from 'react-router'
import { useDispatch } from 'react-redux'
import { ChatInterface } from './components/ChatInterface'
import { setAgents } from './store/agentSlice'
import { setSkills } from './store/skillSlice'
import { createSession } from './store/chatSlice'
import { fetchAgents, fetchSkills } from './api/gateway'
import type { AgentConfig } from './types/agent'
import type { Skill } from './types/skill'

function App() {
  const dispatch = useDispatch()

  useEffect(() => {
    // 初始化数据
    const init = async () => {
      try {
        const [agents, skills] = await Promise.all([
          fetchAgents(),
          fetchSkills(),
        ])
        dispatch(setAgents(agents as AgentConfig[]))
        dispatch(setSkills(skills as Skill[]))
        
        // 创建默认会话
        dispatch(createSession({
          id: 'default',
          name: '默认会话',
          createdAt: Date.now(),
          updatedAt: Date.now(),
          messages: [],
        }))
      } catch (e) {
        console.error('Failed to initialize:', e)
        // 使用默认数据
        dispatch(setAgents([{
          id: 'assistant-1',
          name: '主助手',
          role: 'assistant',
          cliTool: 'opencode',
          args: [],
          env: {},
          autoStart: true,
          skills: [],
          enabled: true,
        }]))
      }
    }
    
    init()
  }, [dispatch])

  return (
    <div className="h-screen bg-gray-50">
      <Routes>
        <Route path="/" element={<ChatInterface />} />
        <Route path="/session/:sessionId" element={<ChatInterface />} />
      </Routes>
    </div>
  )
}

export default App
```

- [ ] **步骤 2：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 3：运行开发服务器**

运行：`pnpm dev`
预期：服务器启动成功

- [ ] **步骤 4：运行构建**

运行：`pnpm build`
预期：构建成功

- [ ] **步骤 5：更新根 package.json scripts**

```json
{
  "scripts": {
    "dev:chat": "cd clawteam-chat && pnpm dev",
    "build:chat": "cd clawteam-chat && pnpm build",
    "check:chat": "cd clawteam-chat && pnpm check"
  }
}
```

- [ ] **步骤 6：Final Commit**

```bash
git add ui/
git commit -m "feat: complete clawteam-chat frontend with integration"
```

---

## 验证清单

- [ ] `pnpm check` 通过
- [ ] `pnpm build` 通过
- [ ] `pnpm dev` 启动成功
- [ ] UI 组件渲染正常
- [ ] Redux store 状态管理正常

---

## 下一步计划

完成此计划后，继续执行：

1. **计划 C: 渠道集成与端到端测试** - Feishu/Weixin 集成、WebSocket 连接、E2E 测试
