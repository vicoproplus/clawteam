# ClawTeam 前端 Vue 3.5 迁移计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将 clawteam-chat 从 React 19 + Redux Toolkit 迁移到 Vue 3.5 + Pinia + pinia/colada

**架构：** 采用 Vue 3.5 Composition API + Pinia Setup Store 模式，参考 Golutra 的最佳实践。pinia/colada 用于异步数据获取和缓存。

**技术栈：** Vue 3.5.12, Pinia 3.0.4, pinia/colada 1.0+, Vue Router 4.5, Tailwind CSS 4.1.17, TypeScript 5.9, Vite 7.2

---

## 迁移策略

### 删除的依赖（React 相关）
- react, react-dom, react-redux, react-router, @reduxjs/toolkit
- @radix-ui/* (替换为原生 Vue 组件)
- @vitejs/plugin-react
- lucide-react (替换为 lucide-vue-next)

### 新增的依赖（Vue 相关）
- vue, vue-router, pinia, @pinia/colada
- @vitejs/plugin-vue
- lucide-vue-next (Vue 图标库)

### 保留的依赖
- tailwindcss, @tailwindcss/vite
- clsx, tailwind-merge, class-variance-authority
- typescript, vite

---

## 文件结构

### 删除文件

| 文件 | 原因 |
|------|------|
| `ui/clawteam-chat/src/main.tsx` | React 入口 |
| `ui/clawteam-chat/src/App.tsx` | React 根组件 |
| `ui/clawteam-chat/src/store/*.ts` | Redux store |
| `ui/clawteam-chat/src/components/**/*.tsx` | React 组件 |
| `ui/clawteam-chat/src/hooks/useAppDispatch.ts` | Redux hook |
| `ui/clawteam-chat/src/features/**/*.tsx` | React 功能组件 |

### 新建文件

| 文件 | 职责 |
|------|------|
| `ui/clawteam-chat/src/main.ts` | Vue 入口 |
| `ui/clawteam-chat/src/App.vue` | Vue 根组件 |
| `ui/clawteam-chat/src/router/index.ts` | Vue Router 配置 |
| `ui/clawteam-chat/src/stores/index.ts` | Pinia 实例 |
| `ui/clawteam-chat/src/stores/chat.ts` | 聊天 Store |
| `ui/clawteam-chat/src/stores/agent.ts` | Agent Store |
| `ui/clawteam-chat/src/stores/skill.ts` | Skill Store |
| `ui/clawteam-chat/src/composables/useWebSocket.ts` | WebSocket composable |
| `ui/clawteam-chat/src/composables/useGateway.ts` | API composable (pinia/colada) |
| `ui/clawteam-chat/src/components/*.vue` | Vue SFC 组件 |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `ui/clawteam-chat/package.json` | 更新依赖 |
| `ui/clawteam-chat/vite.config.ts` | Vue 插件配置 |
| `ui/clawteam-chat/tsconfig.json` | Vue 类型支持 |
| `ui/clawteam-chat/index.html` | 更新入口脚本 |
| `ui/clawteam-chat/src/index.css` | 保留，Tailwind 样式 |
| `ui/clawteam-chat/src/types/*.ts` | 保留类型定义 |

---

## 任务分解

### 任务 0：创建类型定义文件

**注意：** 此任务确保所有类型定义在 Store 创建之前就已准备好。

**文件：**
- 保留：`ui/clawteam-chat/src/types/agent.ts`
- 保留：`ui/clawteam-chat/src/types/message.ts`
- 创建：`ui/clawteam-chat/src/types/skill.ts`
- 创建：`ui/clawteam-chat/src/types/audit.ts`

- [ ] **步骤 1：确认 agent.ts 类型定义存在**

检查 `src/types/agent.ts` 是否包含以下类型：

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
```

- [ ] **步骤 2：创建 skill.ts**

```typescript
// Skill 变量类型
export type SkillVarType = 'text' | 'number' | 'file' | 'directory' | 'select'

export interface SkillVariable {
  name: string
  label: string
  varType: SkillVarType
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

- [ ] **步骤 3：创建 audit.ts**

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

- [ ] **步骤 4：确认 message.ts 类型定义存在**

检查 `src/types/message.ts` 是否包含以下类型：

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

// 会话
export interface Session {
  id: string
  name: string
  createdAt: number
  updatedAt: number
  messages: ChatMessage[]
}
```

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/types/
git commit -m "feat: add type definitions for clawteam-chat"
```

---

### 任务 1：更新项目配置

**文件：**
- 修改：`ui/clawteam-chat/package.json`
- 修改：`ui/clawteam-chat/vite.config.ts`
- 修改：`ui/clawteam-chat/tsconfig.json`
- 修改：`ui/clawteam-chat/tsconfig.app.json`
- 修改：`ui/clawteam-chat/index.html`
- 创建：`ui/clawteam-chat/tsconfig.node.json`

- [ ] **步骤 1：更新 package.json**

```json
{
  "name": "clawteam-chat",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vue-tsc -b && vite build",
    "preview": "vite preview",
    "check": "vue-tsc --noEmit"
  },
  "dependencies": {
    "@pinia/colada": "^0.14.0",
    "@vueuse/core": "^13.0.0",
    "clsx": "^2.1.1",
    "lucide-vue-next": "^0.553.0",
    "pinia": "^3.0.4",
    "tailwind-merge": "^3.4.0",
    "vue": "^3.5.12",
    "vue-router": "^4.5.0"
  },
  "devDependencies": {
    "@tailwindcss/vite": "^4.1.17",
    "@types/node": "^24.10.0",
    "@vitejs/plugin-vue": "^5.2.4",
    "tailwindcss": "^4.1.17",
    "typescript": "~5.9.3",
    "vite": "^7.2.2",
    "vue-tsc": "^2.2.0"
  }
}
```

- [ ] **步骤 2：更新 vite.config.ts**

```typescript
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import tailwindcss from '@tailwindcss/vite'
import path from 'path'

export default defineConfig({
  plugins: [vue(), tailwindcss()],
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

- [ ] **步骤 3：更新 tsconfig.json**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "useDefineForClassFields": true,
    "module": "ESNext",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "preserve",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "baseUrl": ".",
    "paths": {
      "@/*": ["./src/*"]
    }
  },
  "include": ["src/**/*.ts", "src/**/*.tsx", "src/**/*.vue"],
  "references": [{ "path": "./tsconfig.node.json" }]
}
```

- [ ] **步骤 4：创建 tsconfig.node.json**

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

- [ ] **步骤 5：更新 tsconfig.app.json**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "useDefineForClassFields": true,
    "module": "ESNext",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "preserve",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "baseUrl": ".",
    "paths": {
      "@/*": ["./src/*"]
    }
  },
  "include": ["src/**/*.ts", "src/**/*.tsx", "src/**/*.vue"]
}
```

- [ ] **步骤 6：更新 index.html**

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
    <div id="app"></div>
    <script type="module" src="/src/main.ts"></script>
  </body>
</html>
```

- [ ] **步骤 7：安装依赖**

**Windows PowerShell 命令：**

```powershell
cd ui/clawteam-chat
Remove-Item -Recurse -Force node_modules -ErrorAction SilentlyContinue
Remove-Item pnpm-lock.yaml -ErrorAction SilentlyContinue
pnpm install
```

**Linux/macOS 命令：**

```bash
cd ui/clawteam-chat
rm -rf node_modules pnpm-lock.yaml
pnpm install
```

- [ ] **步骤 8：Commit**

```bash
git add ui/clawteam-chat/package.json ui/clawteam-chat/vite.config.ts ui/clawteam-chat/tsconfig*.json ui/clawteam-chat/index.html
git commit -m "refactor: migrate to Vue 3.5 + Pinia tech stack"
```

---

### 任务 2：创建 Vue 入口和路由

**文件：**
- 创建：`ui/clawteam-chat/src/main.ts`
- 创建：`ui/clawteam-chat/src/App.vue`
- 创建：`ui/clawteam-chat/src/router/index.ts`
- 创建：`ui/clawteam-chat/src/stores/index.ts`
- 创建：`ui/clawteam-chat/env.d.ts`

- [ ] **步骤 1：创建 env.d.ts**

```typescript
/// <reference types="vite/client" />

declare module '*.vue' {
  import type { DefineComponent } from 'vue'
  const component: DefineComponent<object, object, unknown>
  export default component
}

interface ImportMetaEnv {
  readonly VITE_API_BASE: string
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}
```

- [ ] **步骤 2：创建 stores/index.ts**

```typescript
import { createPinia } from 'pinia'
import { PiniaColada } from '@pinia/colada'

export const pinia = createPinia()

export const setupPinia = () => {
  // Pinia Colada 插件用于数据获取和缓存
  pinia.use(PiniaColada, {
    // 配置选项
  })
  return pinia
}
```

- [ ] **步骤 3：创建 router/index.ts**

```typescript
import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'

const routes: RouteRecordRaw[] = [
  {
    path: '/',
    name: 'home',
    component: () => import('@/views/ChatView.vue'),
  },
  {
    path: '/session/:sessionId',
    name: 'session',
    component: () => import('@/views/ChatView.vue'),
  },
]

export const router = createRouter({
  history: createWebHistory(),
  routes,
})
```

- [ ] **步骤 4：创建 main.ts**

```typescript
import { createApp } from 'vue'
import { setupPinia } from '@/stores'
import { router } from '@/router'
import App from '@/App.vue'
import '@/index.css'

const app = createApp(App)

app.use(setupPinia())
app.use(router)

app.mount('#app')
```

- [ ] **步骤 5：创建 App.vue（简化版）**

注意：此处使用简化版，数据初始化逻辑将在任务 3 完成后添加。

```vue
<script setup lang="ts">
import { RouterView } from 'vue-router'
</script>

<template>
  <div class="h-screen bg-gray-50">
    <RouterView />
  </div>
</template>
```

- [ ] **步骤 6：创建 views 目录和占位文件**

```bash
mkdir -p ui/clawteam-chat/src/views
```

```vue
<!-- ui/clawteam-chat/src/views/ChatView.vue -->
<script setup lang="ts">
import ChatInterface from '@/components/ChatInterface.vue'
</script>

<template>
  <ChatInterface />
</template>
```

- [ ] **步骤 7：Commit**

```bash
git add ui/clawteam-chat/src/main.ts ui/clawteam-chat/src/App.vue ui/clawteam-chat/src/router/ ui/clawteam-chat/src/stores/index.ts ui/clawteam-chat/env.d.ts ui/clawteam-chat/src/views/
git commit -m "feat: add Vue entry point, router and Pinia setup"
```

---

### 任务 3：创建 Pinia Stores

**文件：**
- 创建：`ui/clawteam-chat/src/stores/chat.ts`
- 创建：`ui/clawteam-chat/src/stores/agent.ts`
- 创建：`ui/clawteam-chat/src/stores/skill.ts`

- [ ] **步骤 1：创建 agent.ts**

```typescript
import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { AgentConfig, AgentState, TerminalSessionStatus, AgentRole } from '@/types/agent'

export const useAgentStore = defineStore('agent', () => {
  // State
  const agents = ref<Record<string, AgentState>>({})
  const agentList = ref<string[]>([])
  const selectedAgentId = ref<string | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  // Getters
  const selectedAgent = computed(() => {
    if (!selectedAgentId.value) return null
    return agents.value[selectedAgentId.value] ?? null
  })

  const groupedByRole = computed(() => {
    const groups: Record<AgentRole, AgentState[]> = {
      assistant: [],
      worker: [],
      supervisor: [],
    }
    for (const id of agentList.value) {
      const agent = agents.value[id]
      if (agent) {
        groups[agent.config.role].push(agent)
      }
    }
    return groups
  })

  // Actions
  const loadAgents = async () => {
    loading.value = true
    error.value = null
    try {
      const response = await fetch('/api/agents')
      if (!response.ok) throw new Error('Failed to fetch agents')
      const configs: AgentConfig[] = await response.json()
      
      agents.value = {}
      agentList.value = []
      
      for (const config of configs) {
        agents.value[config.id] = {
          config,
          status: 'offline',
          lastOutputAt: 0,
        }
        agentList.value.push(config.id)
      }
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
      // 使用默认数据
      const defaultConfig: AgentConfig = {
        id: 'assistant-1',
        name: '主助手',
        role: 'assistant',
        cliTool: 'opencode',
        args: [],
        env: {},
        autoStart: true,
        skills: [],
        enabled: true,
      }
      agents.value = {
        'assistant-1': {
          config: defaultConfig,
          status: 'offline',
          lastOutputAt: 0,
        },
      }
      agentList.value = ['assistant-1']
    } finally {
      loading.value = false
    }
  }

  const updateAgentStatus = (agentId: string, status: TerminalSessionStatus) => {
    const agent = agents.value[agentId]
    if (agent) {
      agent.status = status
      if (status === 'working') {
        agent.workingSince = Date.now()
      } else {
        agent.workingSince = undefined
      }
    }
  }

  const selectAgent = (id: string) => {
    selectedAgentId.value = id
  }

  const updateAgentSession = (agentId: string, sessionId: string) => {
    const agent = agents.value[agentId]
    if (agent) {
      agent.sessionId = sessionId
    }
  }

  return {
    // State
    agents,
    agentList,
    selectedAgentId,
    loading,
    error,
    // Getters
    selectedAgent,
    groupedByRole,
    // Actions
    loadAgents,
    updateAgentStatus,
    selectAgent,
    updateAgentSession,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useAgentStore, import.meta.hot))
}
```

- [ ] **步骤 2：创建 chat.ts**

```typescript
import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { ChatMessage, Session, MessageStatus } from '@/types/message'

export const useChatStore = defineStore('chat', () => {
  // State
  const sessions = ref<Record<string, Session>>({})
  const currentSessionId = ref<string | null>(null)
  const messages = ref<Record<string, ChatMessage[]>>({})
  const pendingMessages = ref<string[]>([])

  // Getters
  const currentSession = computed(() => {
    if (!currentSessionId.value) return null
    return sessions.value[currentSessionId.value] ?? null
  })

  const currentMessages = computed(() => {
    const id = currentSessionId.value
    if (!id) return []
    return messages.value[id] ?? []
  })

  // Actions
  const createSession = (session: Session) => {
    sessions.value[session.id] = session
    if (!messages.value[session.id]) {
      messages.value[session.id] = []
    }
    if (!currentSessionId.value) {
      currentSessionId.value = session.id
    }
  }

  const setCurrentSession = (id: string) => {
    currentSessionId.value = id
  }

  const addMessage = (message: ChatMessage) => {
    const sessionId = message.sessionId
    if (!messages.value[sessionId]) {
      messages.value[sessionId] = []
    }
    messages.value[sessionId].push(message)
    if (sessions.value[sessionId]) {
      sessions.value[sessionId].updatedAt = Date.now()
    }
  }

  const updateMessageStatus = (messageId: string, status: MessageStatus) => {
    for (const sessionId in messages.value) {
      const msg = messages.value[sessionId].find(m => m.id === messageId)
      if (msg) {
        msg.status = status
        break
      }
    }
  }

  const addPendingMessage = (id: string) => {
    pendingMessages.value.push(id)
  }

  const removePendingMessage = (id: string) => {
    pendingMessages.value = pendingMessages.value.filter(i => i !== id)
  }

  return {
    // State
    sessions,
    currentSessionId,
    messages,
    pendingMessages,
    // Getters
    currentSession,
    currentMessages,
    // Actions
    createSession,
    setCurrentSession,
    addMessage,
    updateMessageStatus,
    addPendingMessage,
    removePendingMessage,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useChatStore, import.meta.hot))
}
```

- [ ] **步骤 3：创建 skill.ts**

```typescript
import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { Skill } from '@/types/skill'

export const useSkillStore = defineStore('skill', () => {
  // State
  const skills = ref<Record<string, Skill>>({})
  const skillList = ref<string[]>([])
  const selectedSkillId = ref<string | null>(null)
  const skillDialogOpen = ref(false)
  const loading = ref(false)

  // Getters
  const selectedSkill = computed(() => {
    if (!selectedSkillId.value) return null
    return skills.value[selectedSkillId.value] ?? null
  })

  const skillOptions = computed(() => {
    return skillList.value.map(id => ({
      id,
      name: skills.value[id]?.name ?? id,
    }))
  })

  // Actions
  const loadSkills = async () => {
    loading.value = true
    try {
      const response = await fetch('/api/skills')
      if (!response.ok) throw new Error('Failed to fetch skills')
      const items: Skill[] = await response.json()
      
      skills.value = {}
      skillList.value = []
      
      for (const skill of items) {
        skills.value[skill.id] = skill
        skillList.value.push(skill.id)
      }
    } catch {
      // 使用默认空数据
      skills.value = {}
      skillList.value = []
    } finally {
      loading.value = false
    }
  }

  const selectSkill = (id: string | null) => {
    selectedSkillId.value = id
  }

  const openSkillDialog = () => {
    skillDialogOpen.value = true
  }

  const closeSkillDialog = () => {
    skillDialogOpen.value = false
  }

  return {
    // State
    skills,
    skillList,
    selectedSkillId,
    skillDialogOpen,
    loading,
    // Getters
    selectedSkill,
    skillOptions,
    // Actions
    loadSkills,
    selectSkill,
    openSkillDialog,
    closeSkillDialog,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useSkillStore, import.meta.hot))
}
```

- [ ] **步骤 4：运行类型检查**

```bash
cd ui/clawteam-chat
pnpm check
```

预期：可能有类型错误（组件尚未创建），但 store 应该通过

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/stores/
git commit -m "feat: add Pinia stores for chat, agent, skill"
```

---

### 任务 4：创建工具函数和 Composables

**文件：**
- 创建：`ui/clawteam-chat/src/lib/utils.ts`
- 创建：`ui/clawteam-chat/src/composables/useWebSocket.ts`
- 创建：`ui/clawteam-chat/src/composables/useGateway.ts`

- [ ] **步骤 1：创建 lib/utils.ts**

```typescript
import { clsx, type ClassValue } from 'clsx'
import { twMerge } from 'tailwind-merge'

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs))
}
```

- [ ] **步骤 2：创建 composables/useWebSocket.ts**

```typescript
import { ref, onUnmounted } from 'vue'
import { useChatStore } from '@/stores/chat'
import { useAgentStore } from '@/stores/agent'

interface WebSocketMessage {
  type: 'message' | 'status' | 'output' | 'agent_status'
  payload: unknown
}

export function useWebSocket(url: string) {
  const wsRef = ref<WebSocket | null>(null)
  const connected = ref(false)
  const chatStore = useChatStore()
  const agentStore = useAgentStore()

  const handleMessage = (data: WebSocketMessage) => {
    switch (data.type) {
      case 'message':
        chatStore.addMessage(data.payload as Parameters<typeof chatStore.addMessage>[0])
        break
      case 'status':
        chatStore.updateMessageStatus(
          (data.payload as { messageId: string; status: string }).messageId,
          (data.payload as { messageId: string; status: string }).status as Parameters<typeof chatStore.updateMessageStatus>[1]
        )
        break
      case 'agent_status':
        agentStore.updateAgentStatus(
          (data.payload as { agentId: string; status: string }).agentId,
          (data.payload as { agentId: string; status: string }).status as Parameters<typeof agentStore.updateAgentStatus>[1]
        )
        break
    }
  }

  const connect = () => {
    wsRef.value = new WebSocket(url)

    wsRef.value.onopen = () => {
      console.log('WebSocket connected')
      connected.value = true
    }

    wsRef.value.onmessage = (event) => {
      try {
        const data: WebSocketMessage = JSON.parse(event.data)
        handleMessage(data)
      } catch (e) {
        console.error('Failed to parse WebSocket message:', e)
      }
    }

    wsRef.value.onclose = () => {
      console.log('WebSocket disconnected, reconnecting...')
      connected.value = false
      setTimeout(connect, 3000)
    }

    wsRef.value.onerror = (error) => {
      console.error('WebSocket error:', error)
    }
  }

  const disconnect = () => {
    wsRef.value?.close()
    wsRef.value = null
    connected.value = false
  }

  const send = (message: unknown) => {
    if (wsRef.value?.readyState === WebSocket.OPEN) {
      wsRef.value.send(JSON.stringify(message))
    }
  }

  // 自动连接
  connect()

  // 自动断开
  onUnmounted(() => {
    disconnect()
  })

  return {
    connected,
    send,
    connect,
    disconnect,
  }
}
```

- [ ] **步骤 3：创建 composables/useGateway.ts（使用 pinia/colada）**

```typescript
import { useQuery, useMutation } from '@pinia/colada'
import type { AgentConfig } from '@/types/agent'
import type { Skill } from '@/types/skill'

const API_BASE = '/api'

// 获取 Agents 列表
export function useAgentsQuery() {
  return useQuery<AgentConfig[]>({
    key: ['agents'],
    query: async () => {
      const response = await fetch(`${API_BASE}/agents`)
      if (!response.ok) throw new Error('Failed to fetch agents')
      return response.json()
    },
  })
}

// 获取 Skills 列表
export function useSkillsQuery() {
  return useQuery<Skill[]>({
    key: ['skills'],
    query: async () => {
      const response = await fetch(`${API_BASE}/skills`)
      if (!response.ok) throw new Error('Failed to fetch skills')
      return response.json()
    },
  })
}

// 发送消息
export function useSendMessageMutation() {
  return useMutation({
    mutation: async (payload: { sessionId: string; content: string; skillId?: string }) => {
      const response = await fetch(`${API_BASE}/sessions/${payload.sessionId}/messages`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ content: payload.content, skillId: payload.skillId }),
      })
      if (!response.ok) throw new Error('Failed to send message')
      return response.json()
    },
  })
}

// 获取会话消息
export function useSessionMessagesQuery(sessionId: string) {
  return useQuery({
    key: ['messages', sessionId],
    query: async () => {
      const response = await fetch(`${API_BASE}/sessions/${sessionId}/messages`)
      if (!response.ok) throw new Error('Failed to fetch messages')
      return response.json()
    },
    enabled: !!sessionId,
  })
}
```

- [ ] **步骤 4：运行类型检查**

```bash
cd ui/clawteam-chat
pnpm check
```

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/lib/ ui/clawteam-chat/src/composables/
git commit -m "feat: add utility functions and composables"
```

---

### 任务 5：创建基础 UI 组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/ui/Button.vue`
- 创建：`ui/clawteam-chat/src/components/ui/Card.vue`
- 创建：`ui/clawteam-chat/src/components/ui/Input.vue`

- [ ] **步骤 1：创建 ui/Button.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'

interface Props {
  variant?: 'default' | 'destructive' | 'outline' | 'secondary' | 'ghost' | 'link'
  size?: 'default' | 'sm' | 'lg' | 'icon'
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  variant: 'default',
  size: 'default',
  disabled: false,
})

const variantClasses: Record<string, string> = {
  default: 'bg-blue-600 text-white hover:bg-blue-700',
  destructive: 'bg-red-600 text-white hover:bg-red-700',
  outline: 'border border-gray-300 bg-white hover:bg-gray-50',
  secondary: 'bg-gray-100 text-gray-900 hover:bg-gray-200',
  ghost: 'hover:bg-gray-100',
  link: 'text-blue-600 underline-offset-4 hover:underline',
}

const sizeClasses: Record<string, string> = {
  default: 'h-10 px-4 py-2',
  sm: 'h-9 rounded-md px-3',
  lg: 'h-11 rounded-md px-8',
  icon: 'h-10 w-10',
}

const classes = computed(() =>
  cn(
    'inline-flex items-center justify-center rounded-md text-sm font-medium transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-blue-500 disabled:pointer-events-none disabled:opacity-50',
    variantClasses[props.variant],
    sizeClasses[props.size]
  )
)
</script>

<template>
  <button :class="classes" :disabled="disabled">
    <slot />
  </button>
</template>
```

- [ ] **步骤 2：创建 ui/Card.vue**

```vue
<script setup lang="ts">
import { cn } from '@/lib/utils'

interface Props {
  class?: string
}

const props = defineProps<Props>()
</script>

<template>
  <div :class="cn('rounded-lg border bg-white shadow-sm', props.class)">
    <slot />
  </div>
</template>
```

- [ ] **步骤 2.1：创建 ui/CardHeader.vue**

```vue
<script setup lang="ts">
import { cn } from '@/lib/utils'

interface Props {
  class?: string
}

const props = defineProps<Props>()
</script>

<template>
  <div :class="cn('flex flex-col space-y-1.5 p-6', props.class)">
    <slot />
  </div>
</template>
```

- [ ] **步骤 2.2：创建 ui/CardTitle.vue**

```vue
<script setup lang="ts">
import { cn } from '@/lib/utils'

interface Props {
  class?: string
}

const props = defineProps<Props>()
</script>

<template>
  <h3 :class="cn('text-lg font-semibold leading-none tracking-tight', props.class)">
    <slot />
  </h3>
</template>
```

- [ ] **步骤 2.3：创建 ui/CardContent.vue**

```vue
<script setup lang="ts">
import { cn } from '@/lib/utils'

interface Props {
  class?: string
}

const props = defineProps<Props>()
</script>

<template>
  <div :class="cn('p-6 pt-0', props.class)">
    <slot />
  </div>
</template>
```

- [ ] **步骤 3：创建 ui/Input.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'

interface Props {
  modelValue?: string
  type?: string
  placeholder?: string
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  modelValue: '',
  type: 'text',
  placeholder: '',
  disabled: false,
})

const emit = defineEmits<{
  'update:modelValue': [value: string]
  keydown: [event: KeyboardEvent]
}>()

const classes = computed(() =>
  cn(
    'flex h-10 w-full rounded-md border border-gray-300 bg-white px-3 py-2 text-sm placeholder:text-gray-400 focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:cursor-not-allowed disabled:opacity-50'
  )
)
</script>

<template>
  <input
    :type="type"
    :class="classes"
    :value="modelValue"
    :placeholder="placeholder"
    :disabled="disabled"
    @input="emit('update:modelValue', ($event.target as HTMLInputElement).value)"
    @keydown="emit('keydown', $event)"
  />
</template>
```

- [ ] **步骤 4：创建 ui/index.ts**

```typescript
export { default as Button } from './Button.vue'
export { default as Card } from './Card.vue'
export { default as CardHeader } from './CardHeader.vue'
export { default as CardTitle } from './CardTitle.vue'
export { default as CardContent } from './CardContent.vue'
export { default as Input } from './Input.vue'
```

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/components/ui/
git commit -m "feat: add base UI components (Button, Card, Input)"
```

---

### 任务 5.5：配置测试环境（TDD）

**注意：** 此任务配置 Vitest 测试环境，为后续开发提供测试支持。

**文件：**
- 修改：`ui/clawteam-chat/package.json`
- 创建：`ui/clawteam-chat/vitest.config.ts`
- 创建：`ui/clawteam-chat/src/stores/__tests__/agent.spec.ts`
- 创建：`ui/clawteam-chat/src/stores/__tests__/chat.spec.ts`

- [ ] **步骤 1：添加测试依赖到 package.json**

在 `devDependencies` 中添加：

```json
{
  "devDependencies": {
    "@vue/test-utils": "^2.4.6",
    "vitest": "^3.0.0",
    "jsdom": "^26.0.0",
    "happy-dom": "^17.0.0"
  }
}
```

并在 `scripts` 中添加：

```json
{
  "scripts": {
    "test": "vitest run",
    "test:watch": "vitest",
    "test:coverage": "vitest run --coverage"
  }
}
```

- [ ] **步骤 2：创建 vitest.config.ts**

```typescript
import { defineConfig } from 'vitest/config'
import vue from '@vitejs/plugin-vue'
import path from 'path'

export default defineConfig({
  plugins: [vue()],
  test: {
    environment: 'jsdom',
    globals: true,
    include: ['src/**/*.{test,spec}.{js,ts}'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
      include: ['src/**/*.ts', 'src/**/*.vue'],
      exclude: ['src/**/*.d.ts', 'src/**/__tests__/**'],
    },
  },
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
})
```

- [ ] **步骤 3：安装测试依赖**

```bash
cd ui/clawteam-chat
pnpm install
```

- [ ] **步骤 4：创建 stores/__tests__/agent.spec.ts**

```typescript
import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useAgentStore } from '../agent'

describe('Agent Store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('should initialize with empty agents', () => {
    const store = useAgentStore()
    expect(store.agentList).toEqual([])
    expect(store.agents).toEqual({})
  })

  it('should select an agent', () => {
    const store = useAgentStore()
    store.selectAgent('test-id')
    expect(store.selectedAgentId).toBe('test-id')
  })

  it('should update agent status', () => {
    const store = useAgentStore()
    // 模拟一个 agent
    store.agents = {
      'agent-1': {
        config: {
          id: 'agent-1',
          name: 'Test Agent',
          role: 'assistant',
          cliTool: 'test',
          args: [],
          env: {},
          autoStart: true,
          skills: [],
          enabled: true,
        },
        status: 'offline',
        lastOutputAt: 0,
      },
    }
    
    store.updateAgentStatus('agent-1', 'working')
    expect(store.agents['agent-1'].status).toBe('working')
    expect(store.agents['agent-1'].workingSince).toBeDefined()
  })
})
```

- [ ] **步骤 5：创建 stores/__tests__/chat.spec.ts**

```typescript
import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useChatStore } from '../chat'

describe('Chat Store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('should initialize with empty sessions', () => {
    const store = useChatStore()
    expect(store.currentSessionId).toBeNull()
    expect(store.sessions).toEqual({})
  })

  it('should create a session', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })
    
    expect(store.sessions['session-1']).toBeDefined()
    expect(store.currentSessionId).toBe('session-1')
  })

  it('should add a message', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })
    
    store.addMessage({
      id: 'msg-1',
      sessionId: 'session-1',
      senderType: 'user',
      senderId: 'user-1',
      senderName: 'Test User',
      content: 'Hello',
      status: 'pending',
      createdAt: Date.now(),
    })
    
    expect(store.messages['session-1']).toHaveLength(1)
    expect(store.messages['session-1'][0].content).toBe('Hello')
  })

  it('should update message status', () => {
    const store = useChatStore()
    store.createSession({
      id: 'session-1',
      name: 'Test Session',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })
    
    store.addMessage({
      id: 'msg-1',
      sessionId: 'session-1',
      senderType: 'user',
      senderId: 'user-1',
      senderName: 'Test User',
      content: 'Hello',
      status: 'pending',
      createdAt: Date.now(),
    })
    
    store.updateMessageStatus('msg-1', 'completed')
    expect(store.messages['session-1'][0].status).toBe('completed')
  })
})
```

- [ ] **步骤 6：运行测试**

```bash
cd ui/clawteam-chat
pnpm test
```

预期：所有测试通过

- [ ] **步骤 7：Commit**

```bash
git add ui/clawteam-chat/vitest.config.ts ui/clawteam-chat/src/stores/__tests__/
git commit -m "feat: add Vitest test configuration and store tests"
```

---

### 任务 6：创建 AgentPanel 组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/AgentCard.vue`
- 创建：`ui/clawteam-chat/src/components/AgentPanel.vue`

- [ ] **步骤 1：创建 AgentCard.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { Bot, Loader2, CheckCircle, XCircle, Clock } from 'lucide-vue-next'
import { cn } from '@/lib/utils'
import type { AgentRole, TerminalSessionStatus } from '@/types/agent'

interface Props {
  id: string
  name: string
  role: AgentRole
  status: TerminalSessionStatus
  cliTool: string
  required: boolean
  selected?: boolean
}

const props = defineProps<Props>()
const emit = defineEmits<{ click: [] }>()

const statusIcons: Record<TerminalSessionStatus, typeof CheckCircle> = {
  pending: Clock,
  online: CheckCircle,
  working: Loader2,
  offline: XCircle,
  broken: XCircle,
}

const statusColors: Record<TerminalSessionStatus, string> = {
  pending: 'text-gray-400',
  online: 'text-green-500',
  working: 'text-blue-500',
  offline: 'text-gray-400',
  broken: 'text-red-500',
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

const StatusIcon = computed(() => statusIcons[props.status])
const cardClasses = computed(() =>
  cn(
    'p-3 rounded-lg border cursor-pointer transition-all',
    roleColors[props.role],
    props.selected && 'ring-2 ring-blue-500',
    !props.required && 'opacity-80 hover:opacity-100'
  )
)
</script>

<template>
  <div :class="cardClasses" @click="emit('click')">
    <div class="flex items-center gap-2">
      <Bot class="h-5 w-5 text-gray-600" />
      <div class="flex-1 min-w-0">
        <div class="font-medium text-sm truncate">{{ name }}</div>
        <div class="text-xs text-gray-500">
          {{ roleLabels[role] }} · {{ cliTool }}
        </div>
      </div>
      <component :is="StatusIcon" :class="cn('h-4 w-4', statusColors[status], status === 'working' && 'animate-spin')" />
    </div>
    <div v-if="required" class="mt-1 text-xs text-gray-400">必需</div>
  </div>
</template>
```

- [ ] **步骤 2：创建 AgentPanel.vue**

```vue
<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useAgentStore } from '@/stores/agent'
import AgentCard from './AgentCard.vue'
import { Button } from './ui'
import { Plus } from 'lucide-vue-next'
import type { AgentRole } from '@/types/agent'

const agentStore = useAgentStore()
const { agents, agentList, selectedAgentId, groupedByRole } = storeToRefs(agentStore)

const roleOrder: AgentRole[] = ['assistant', 'worker', 'supervisor']
const roleLabels: Record<AgentRole, string> = {
  assistant: '助手',
  worker: '员工',
  supervisor: '监工',
}
</script>

<template>
  <div class="w-64 border-r bg-gray-50 flex flex-col">
    <div class="p-4 border-b">
      <h2 class="font-semibold">Agent 成员</h2>
    </div>
    
    <div class="flex-1 overflow-auto p-4 space-y-4">
      <template v-for="role in roleOrder" :key="role">
        <div v-if="groupedByRole[role]?.length">
          <div class="text-xs font-medium text-gray-500 mb-2">
            {{ roleLabels[role] }}
          </div>
          <div class="space-y-2">
            <AgentCard
              v-for="agent in groupedByRole[role]"
              :key="agent.config.id"
              :id="agent.config.id"
              :name="agent.config.name"
              :role="agent.config.role"
              :status="agent.status"
              :cli-tool="agent.config.cliTool"
              :required="agent.config.role === 'assistant'"
              :selected="selectedAgentId === agent.config.id"
              @click="agentStore.selectAgent(agent.config.id)"
            />
          </div>
        </div>
      </template>
    </div>
    
    <div class="p-4 border-t">
      <Button variant="outline" size="sm" class="w-full">
        <Plus class="h-4 w-4 mr-2" />
        添加 Agent
      </Button>
    </div>
  </div>
</template>
```

- [ ] **步骤 3：运行类型检查**

```bash
cd ui/clawteam-chat
pnpm check
```

- [ ] **步骤 4：Commit**

```bash
git add ui/clawteam-chat/src/components/AgentCard.vue ui/clawteam-chat/src/components/AgentPanel.vue
git commit -m "feat: add AgentPanel and AgentCard Vue components"
```

---

### 任务 7：创建 ChatInterface 组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/MessageList.vue`
- 创建：`ui/clawteam-chat/src/components/ChatInput.vue`
- 创建：`ui/clawteam-chat/src/components/ChatInterface.vue`
- 创建：`ui/clawteam-chat/src/components/OutputPanel.vue`

- [ ] **步骤 1：创建 MessageList.vue**

```vue
<script setup lang="ts">
import { User, Bot, Wrench, Eye } from 'lucide-vue-next'
import { cn } from '@/lib/utils'
import type { ChatMessage, SenderType } from '@/types/message'

interface Props {
  messages: ChatMessage[]
}

defineProps<Props>()

const senderIcons: Record<SenderType, typeof User> = {
  user: User,
  assistant: Bot,
  worker: Wrench,
  supervisor: Eye,
  system: User,
}

const senderColors: Record<SenderType, string> = {
  user: 'bg-blue-100',
  assistant: 'bg-purple-100',
  worker: 'bg-green-100',
  supervisor: 'bg-orange-100',
  system: 'bg-gray-100',
}

const formatTime = (timestamp: number) => {
  return new Date(timestamp).toLocaleTimeString()
}
</script>

<template>
  <div class="flex-1 overflow-auto p-4 space-y-4">
    <div
      v-for="message in messages"
      :key="message.id"
      :class="cn(
        'flex gap-3 p-3 rounded-lg',
        message.senderType === 'user' ? 'flex-row-reverse' : 'flex-row'
      )"
    >
      <div
        :class="cn(
          'flex-shrink-0 w-8 h-8 rounded-full flex items-center justify-center',
          senderColors[message.senderType]
        )"
      >
        <component :is="senderIcons[message.senderType]" class="h-4 w-4" />
      </div>
      
      <div :class="cn('flex-1', message.senderType === 'user' && 'text-right')">
        <div class="flex items-center gap-2 mb-1">
          <span class="font-medium text-sm">{{ message.senderName }}</span>
          <span class="text-xs text-gray-400">
            {{ formatTime(message.createdAt) }}
          </span>
        </div>
        <div
          :class="cn(
            'inline-block px-3 py-2 rounded-lg text-sm',
            message.senderType === 'user'
              ? 'bg-blue-500 text-white'
              : 'bg-gray-100 text-gray-900'
          )"
        >
          {{ message.content }}
        </div>
        <div v-if="message.status === 'processing'" class="text-xs text-gray-400 mt-1">
          处理中...
        </div>
        <div v-if="message.linkedAgent" class="text-xs text-gray-400 mt-1">
          → {{ message.linkedAgent }}
        </div>
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：创建 ChatInput.vue**

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { Send, ChevronDown } from 'lucide-vue-next'
import { Button, Input } from './ui'
import { useSkillStore } from '@/stores/skill'
import { storeToRefs } from 'pinia'

interface Props {
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  disabled: false,
})

const emit = defineEmits<{
  send: [content: string, skillId?: string]
}>()

const skillStore = useSkillStore()
const { skillOptions } = storeToRefs(skillStore)

const input = ref('')
const showSkillMenu = ref(false)
const selectedSkillId = ref<string | null>(null)

const handleSend = () => {
  if (input.value.trim() && !props.disabled) {
    emit('send', input.value.trim(), selectedSkillId.value ?? undefined)
    input.value = ''
    selectedSkillId.value = null
  }
}

const handleKeyDown = (e: KeyboardEvent) => {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault()
    handleSend()
  }
}

const selectSkill = (id: string) => {
  selectedSkillId.value = id
  showSkillMenu.value = false
}

const selectedSkillName = () => {
  if (!selectedSkillId.value) return '选择 Skill'
  return skillOptions.value.find(s => s.id === selectedSkillId.value)?.name ?? '选择 Skill'
}
</script>

<template>
  <div class="border-t bg-white p-4">
    <div class="flex gap-2">
      <div class="relative">
        <Button
          variant="outline"
          size="sm"
          :disabled="disabled"
          @click="showSkillMenu = !showSkillMenu"
        >
          {{ selectedSkillName() }}
          <ChevronDown class="h-4 w-4 ml-1" />
        </Button>
        
        <div
          v-if="showSkillMenu"
          class="absolute bottom-full left-0 mb-1 w-48 bg-white border rounded-lg shadow-lg z-10"
        >
          <button
            v-for="skill in skillOptions"
            :key="skill.id"
            class="w-full px-3 py-2 text-left text-sm hover:bg-gray-50"
            @click="selectSkill(skill.id)"
          >
            {{ skill.name }}
          </button>
        </div>
      </div>
      
      <Input
        v-model="input"
        :disabled="disabled"
        placeholder="输入消息..."
        class="flex-1"
        @keydown="handleKeyDown"
      />
      
      <Button :disabled="disabled || !input.trim()" @click="handleSend">
        <Send class="h-4 w-4" />
      </Button>
    </div>
  </div>
</template>
```

- [ ] **步骤 3：创建 OutputPanel.vue**

```vue
<script setup lang="ts">
defineProps<{
  output?: string
}>()
</script>

<template>
  <div class="w-96 border-l bg-gray-900 text-gray-100 p-4 font-mono text-sm overflow-auto">
    <div class="text-gray-400 mb-2">CLI 输出</div>
    <div v-if="output" class="whitespace-pre-wrap">
      {{ output }}
    </div>
    <div v-else class="text-gray-500">
      等待输出...
    </div>
  </div>
</template>
```

- [ ] **步骤 4：创建 ChatInterface.vue**

```vue
<script setup lang="ts">
import { ref, computed } from 'vue'
import { storeToRefs } from 'pinia'
import AgentPanel from './AgentPanel.vue'
import MessageList from './MessageList.vue'
import ChatInput from './ChatInput.vue'
import OutputPanel from './OutputPanel.vue'
import { useChatStore } from '@/stores/chat'

const chatStore = useChatStore()
const { currentMessages, currentSessionId } = storeToRefs(chatStore)

const showOutput = ref(false)

const handleSend = (content: string, skillId?: string) => {
  const sessionId = currentSessionId.value || 'default'
  
  // 创建消息
  chatStore.addMessage({
    id: `msg-${Date.now()}`,
    sessionId,
    senderType: 'user',
    senderId: 'user',
    senderName: '用户',
    content,
    status: 'pending',
    createdAt: Date.now(),
    metadata: skillId ? { skillId } : undefined,
  })
  
  // TODO: 通过 WebSocket 发送到后端
  console.log('Send message:', content, 'skill:', skillId)
}
</script>

<template>
  <div class="flex h-full">
    <AgentPanel />
    
    <div class="flex-1 flex flex-col">
      <div class="border-b px-4 py-3 flex items-center justify-between">
        <h1 class="font-semibold">ClawTeam</h1>
        <button
          class="text-sm text-gray-500 hover:text-gray-700"
          @click="showOutput = !showOutput"
        >
          {{ showOutput ? '隐藏输出' : '显示输出' }}
        </button>
      </div>
      
      <MessageList :messages="currentMessages" />
      <ChatInput @send="handleSend" />
    </div>
    
    <OutputPanel v-if="showOutput" />
  </div>
</template>
```

- [ ] **步骤 5：运行类型检查**

```bash
cd ui/clawteam-chat
pnpm check
```

- [ ] **步骤 6：Commit**

```bash
git add ui/clawteam-chat/src/components/ChatInterface.vue ui/clawteam-chat/src/components/MessageList.vue ui/clawteam-chat/src/components/ChatInput.vue ui/clawteam-chat/src/components/OutputPanel.vue
git commit -m "feat: add ChatInterface with MessageList and ChatInput Vue components"
```

---

### 任务 8：清理旧文件并验证构建

**文件：**
- 删除：`ui/clawteam-chat/src/main.tsx`
- 删除：`ui/clawteam-chat/src/App.tsx`
- 删除：`ui/clawteam-chat/src/store/`
- 删除：`ui/clawteam-chat/src/features/` (如果存在 React 组件)
- 删除：`ui/clawteam-chat/src/hooks/useAppDispatch.ts`
- 删除：`ui/clawteam-chat/src/api/` (替换为 composables)
- 删除：`ui/clawteam-chat/src/components/ui/*.tsx` (如果有)

- [ ] **步骤 1：删除旧的 React 文件**

**Windows PowerShell 命令：**

```powershell
cd ui/clawteam-chat/src
Remove-Item -Force main.tsx, App.tsx -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force store, features, hooks, api -ErrorAction SilentlyContinue
Remove-Item -Force components/ui/*.tsx -ErrorAction SilentlyContinue
Remove-Item -Force components/*.tsx -ErrorAction SilentlyContinue
```

**Linux/macOS 命令：**

```bash
cd ui/clawteam-chat/src
rm -rf main.tsx App.tsx store/ features/ hooks/ api/ components/ui/*.tsx
rm -f components/*.tsx
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat
pnpm check
```

预期：PASS

- [ ] **步骤 3：运行开发服务器**

```bash
cd ui/clawteam-chat
pnpm dev
```

预期：服务器启动成功，访问 http://localhost:5174

- [ ] **步骤 4：运行构建**

```bash
cd ui/clawteam-chat
pnpm build
```

预期：构建成功

- [ ] **步骤 5：更新根 package.json scripts**

修改 `ui/package.json`:

```json
{
  "scripts": {
    "check:core": "cd core && pnpm check",
    "check:chat": "cd clawteam-chat && pnpm check",
    "check": "run-p check:*",
    "dev:chat": "cd clawteam-chat && pnpm dev",
    "build:chat": "cd clawteam-chat && pnpm build",
    "format": "prettier --write .",
    "format:check": "prettier --check ."
  }
}
```

- [ ] **步骤 6：Final Commit**

```bash
git add ui/
git commit -m "refactor: complete Vue 3.5 migration for clawteam-chat"
```

---

## 验证清单

- [ ] `pnpm check` 通过
- [ ] `pnpm build` 通过
- [ ] `pnpm dev` 启动成功
- [ ] Vue DevTools 可正常使用
- [ ] Pinia store 状态管理正常
- [ ] HMR 热更新正常工作

---

## 迁移完成后的目录结构

```
ui/clawteam-chat/
├── index.html
├── package.json
├── vite.config.ts
├── tsconfig.json
├── tsconfig.app.json
├── tsconfig.node.json
├── env.d.ts
└── src/
    ├── main.ts
    ├── App.vue
    ├── index.css
    ├── router/
    │   └── index.ts
    ├── stores/
    │   ├── index.ts
    │   ├── agent.ts
    │   ├── chat.ts
    │   └── skill.ts
    ├── composables/
    │   ├── useWebSocket.ts
    │   └── useGateway.ts
    ├── components/
    │   ├── ui/
    │   │   ├── index.ts
    │   │   ├── Button.vue
    │   │   ├── Card.vue
    │   │   └── Input.vue
    │   ├── AgentCard.vue
    │   ├── AgentPanel.vue
    │   ├── ChatInput.vue
    │   ├── ChatInterface.vue
    │   ├── MessageList.vue
    │   └── OutputPanel.vue
    ├── views/
    │   └── ChatView.vue
    ├── lib/
    │   └── utils.ts
    └── types/
        ├── agent.ts
        ├── skill.ts
        ├── message.ts
        └── audit.ts
```

---

## 下一步计划

完成此迁移后，继续执行：

1. **集成测试** - 确保所有功能正常工作
2. **WebSocket 连接** - 完善后端通信
3. **pinia/colada 数据获取** - 使用 useQuery/useMutation 替代手动 fetch
4. **国际化** - 添加 vue-i18n 支持
