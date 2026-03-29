# 还原 golutra UI 效果实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将 clawteam-chat 前端 UI 还原为 golutra 风格的多智能体工作空间界面，同时保留 Feishu/Weixin 渠道配置功能。

**架构：** 采用 Vue 3 + Pinia + Tailwind CSS + xterm.js 技术栈，按 feature 模块组织代码，实现工作空间管理、终端面板、聊天界面、成员侧栏等核心功能。

**技术栈：** Vue 3.5, Pinia 3.0, Tailwind CSS 4.x, xterm.js 6.0, vue-router 4.x

---

## 文件结构

### 新增文件

| 文件 | 职责 |
|------|------|
| `src/features/workspace/workspaceStore.ts` | 工作区列表、当前工作区状态管理 |
| `src/features/workspace/projectStore.ts` | 成员、路线图、技能持久化 |
| `src/features/terminal/terminalStore.ts` | 终端标签页、布局模式状态 |
| `src/features/terminal/terminalBridge.ts` | 终端相关 Tauri/API 命令封装 |
| `src/features/chat/chatBridge.ts` | 聊天相关 API 命令封装 |
| `src/components/SidebarNav.vue` | 侧边导航栏 |
| `src/components/WorkspaceSelection.vue` | 工作区选择下拉 |
| `src/components/TerminalWorkspace.vue` | 终端工作区容器 |
| `src/components/TerminalPane.vue` | 单个终端面板（xterm.js） |
| `src/components/MembersSidebar.vue` | 成员侧栏 |
| `src/components/ChatSidebar.vue` | 会话列表侧栏 |
| `src/components/Settings.vue` | 设置页面 |
| `src/types/workspace.ts` | 工作区类型定义 |
| `src/types/terminal.ts` | 终端类型定义 |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `src/App.vue` | 重构为 golutra 风格布局 |
| `src/components/ChatInterface.vue` | 整合 MembersSidebar、ChatSidebar |
| `src/stores/chat.ts` | 扩展会话管理功能 |
| `src/stores/agent.ts` | 对齐 golutra 成员模型 |
| `src/router/index.ts` | 添加设置页路由 |
| `src/types/message.ts` | 扩展消息类型 |

### 保留文件（Feishu/Weixin 配置）

| 文件 | 说明 |
|------|------|
| `src/components/channel/ChannelConfig.vue` | 渠道配置弹窗 |
| `src/components/channel/FeishuConfig.vue` | 飞书配置表单 |
| `src/components/channel/WeixinConfig.vue` | 微信配置表单 |
| `src/stores/channel.ts` | 渠道状态管理 |

---

## 任务分解

### 任务 1：类型定义扩展

**文件：**
- 创建：`src/types/workspace.ts`
- 创建：`src/types/terminal.ts`
- 修改：`src/types/message.ts`

- [ ] **步骤 1：创建 workspace.ts 类型定义**

```typescript
// src/types/workspace.ts
export interface WorkspaceEntry {
  id: string;
  name: string;
  path: string;
  lastOpenedAt: number;
}

export interface Member {
  id: string;
  name: string;
  role: string;
  roleKey: string;
  roleType: 'owner' | 'admin' | 'assistant' | 'member';
  avatar: string;
  status: MemberStatus;
  manualStatus?: MemberStatus;
  terminalStatus?: TerminalConnectionStatus;
  terminalType?: TerminalType;
  terminalCommand?: string;
  terminalPath?: string;
  autoStartTerminal: boolean;
}

export type MemberStatus = 'online' | 'working' | 'dnd' | 'offline';
export type TerminalConnectionStatus = 'connected' | 'disconnected' | 'reconnecting';
export type TerminalType = 'claude' | 'gemini' | 'codex' | 'opencode' | 'qwen' | 'shell';

export interface ProjectData {
  projectId: string;
  version: number;
  members: Member[];
  memberSequence: Record<string, number>;
  terminal: {
    recentClosedTabs: ProjectTerminalRecentTab[];
  };
  roadmap: {
    objective: string;
    tasks: RoadmapTask[];
  };
  skills: {
    current: ProjectSkill[];
  };
}

export interface ProjectTerminalRecentTab {
  id: string;
  memberId: string;
  closedAt: number;
}

export interface RoadmapTask {
  id: string;
  title: string;
  status: 'pending' | 'in-progress' | 'completed';
}

export interface ProjectSkill {
  id: string;
  name: string;
  enabled: boolean;
}
```

- [ ] **步骤 2：创建 terminal.ts 类型定义**

```typescript
// src/types/terminal.ts
export type TerminalSessionStatus = 'pending' | 'online' | 'working' | 'offline' | 'broken';

export interface TerminalSession {
  id: string;
  memberId?: string;
  workspaceId?: string;
  terminalType: TerminalType;
  status: TerminalSessionStatus;
  cwd?: string;
  createdAt: number;
  lastOutputAt: number;
}

export interface TerminalOutput {
  sessionId: string;
  data: string;
  timestamp: number;
}

export interface TerminalSnapshot {
  sessionId: string;
  lines: string[];
  cursorPosition: { x: number; y: number };
  capturedAt: number;
}

export type TerminalType = 'claude' | 'gemini' | 'codex' | 'opencode' | 'qwen' | 'shell';
```

- [ ] **步骤 3：扩展 message.ts 类型**

在 `src/types/message.ts` 添加：

```typescript
// 扩展会话类型
export interface Conversation {
  id: string;
  workspaceId: string;
  title: string;
  createdAt: number;
  updatedAt: number;
  unreadCount: number;
  lastMessage?: ChatMessage;
}

// 扩展消息元数据
export interface MessageMetadata {
  skillId?: string;
  linkedAgentId?: string;
  dispatchOutcome?: 'dispatched' | 'queued' | 'duplicate' | 'skipped';
  terminalSessionId?: string;
}
```

- [ ] **步骤 4：验证类型编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无类型错误

---

### 任务 2：工作空间状态管理

**文件：**
- 创建：`src/features/workspace/workspaceStore.ts`
- 创建：`src/features/workspace/projectStore.ts`

- [ ] **步骤 1：创建 workspaceStore.ts**

```typescript
// src/features/workspace/workspaceStore.ts
import { ref, computed } from 'vue'
import { defineStore } from 'pinia'
import type { WorkspaceEntry } from '@/types/workspace'

export const useWorkspaceStore = defineStore('workspace', () => {
  const workspaces = ref<WorkspaceEntry[]>([])
  const currentWorkspaceId = ref<string | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  const currentWorkspace = computed(() => {
    return workspaces.value.find(w => w.id === currentWorkspaceId.value) ?? null
  })

  const loadWorkspaces = async () => {
    loading.value = true
    error.value = null
    try {
      const response = await fetch('/api/workspaces')
      if (!response.ok) throw new Error('Failed to fetch workspaces')
      workspaces.value = await response.json()
      if (workspaces.value.length > 0 && !currentWorkspaceId.value) {
        currentWorkspaceId.value = workspaces.value[0].id
      }
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    } finally {
      loading.value = false
    }
  }

  const selectWorkspace = (id: string) => {
    currentWorkspaceId.value = id
  }

  const addWorkspace = async (path: string) => {
    try {
      const response = await fetch('/api/workspaces', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path }),
      })
      if (!response.ok) throw new Error('Failed to add workspace')
      const workspace = await response.json()
      workspaces.value.push(workspace)
      currentWorkspaceId.value = workspace.id
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    }
  }

  const removeWorkspace = async (id: string) => {
    try {
      await fetch(`/api/workspaces/${id}`, { method: 'DELETE' })
      workspaces.value = workspaces.value.filter(w => w.id !== id)
      if (currentWorkspaceId.value === id) {
        currentWorkspaceId.value = workspaces.value[0]?.id ?? null
      }
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    }
  }

  return {
    workspaces,
    currentWorkspaceId,
    currentWorkspace,
    loading,
    error,
    loadWorkspaces,
    selectWorkspace,
    addWorkspace,
    removeWorkspace,
  }
})
```

- [ ] **步骤 2：创建 projectStore.ts**

```typescript
// src/features/workspace/projectStore.ts
import { ref, computed } from 'vue'
import { defineStore } from 'pinia'
import type { ProjectData, Member } from '@/types/workspace'

export const useProjectStore = defineStore('project', () => {
  const projectData = ref<ProjectData | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  const members = computed(() => projectData.value?.members ?? [])
  const roadmap = computed(() => projectData.value?.roadmap ?? null)
  const skills = computed(() => projectData.value?.skills?.current ?? [])

  const loadProject = async (workspaceId: string) => {
    loading.value = true
    error.value = null
    try {
      const response = await fetch(`/api/workspaces/${workspaceId}/project`)
      if (!response.ok) throw new Error('Failed to fetch project')
      projectData.value = await response.json()
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    } finally {
      loading.value = false
    }
  }

  const updateMember = async (memberId: string, updates: Partial<Member>) => {
    if (!projectData.value) return
    const index = projectData.value.members.findIndex(m => m.id === memberId)
    if (index !== -1) {
      projectData.value.members[index] = { ...projectData.value.members[index], ...updates }
      await fetch(`/api/members/${memberId}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(updates),
      })
    }
  }

  return {
    projectData,
    members,
    roadmap,
    skills,
    loading,
    error,
    loadProject,
    updateMember,
  }
})
```

- [ ] **步骤 3：验证编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无错误

---

### 任务 3：终端状态管理

**文件：**
- 创建：`src/features/terminal/terminalStore.ts`
- 创建：`src/features/terminal/terminalBridge.ts`

- [ ] **步骤 1：创建 terminalStore.ts**

```typescript
// src/features/terminal/terminalStore.ts
import { ref, computed } from 'vue'
import { defineStore } from 'pinia'
import type { TerminalSession, TerminalSessionStatus } from '@/types/terminal'

export type LayoutMode = 'single' | 'split-vertical' | 'split-horizontal' | 'grid'

export const useTerminalStore = defineStore('terminal', () => {
  const sessions = ref<Record<string, TerminalSession>>({})
  const sessionOrder = ref<string[]>([])
  const activeSessionId = ref<string | null>(null)
  const layoutMode = ref<LayoutMode>('single')
  const outputs = ref<Record<string, string>>({})

  const activeSession = computed(() => {
    return activeSessionId.value ? sessions.value[activeSessionId.value] ?? null : null
  })

  const sessionsList = computed(() => {
    return sessionOrder.value.map(id => sessions.value[id]).filter(Boolean)
  })

  const createSession = (session: TerminalSession) => {
    sessions.value[session.id] = session
    if (!sessionOrder.value.includes(session.id)) {
      sessionOrder.value.push(session.id)
    }
    if (!activeSessionId.value) {
      activeSessionId.value = session.id
    }
  }

  const closeSession = (id: string) => {
    delete sessions.value[id]
    sessionOrder.value = sessionOrder.value.filter(sid => sid !== id)
    if (activeSessionId.value === id) {
      activeSessionId.value = sessionOrder.value[0] ?? null
    }
  }

  const updateSessionStatus = (id: string, status: TerminalSessionStatus) => {
    const session = sessions.value[id]
    if (session) {
      session.status = status
    }
  }

  const setActiveSession = (id: string) => {
    if (sessions.value[id]) {
      activeSessionId.value = id
    }
  }

  const appendOutput = (sessionId: string, data: string) => {
    if (!outputs.value[sessionId]) {
      outputs.value[sessionId] = ''
    }
    outputs.value[sessionId] += data
  }

  return {
    sessions,
    sessionOrder,
    activeSessionId,
    activeSession,
    sessionsList,
    layoutMode,
    outputs,
    createSession,
    closeSession,
    updateSessionStatus,
    setActiveSession,
    appendOutput,
  }
})
```

- [ ] **步骤 2：创建 terminalBridge.ts**

```typescript
// src/features/terminal/terminalBridge.ts
import type { TerminalType, TerminalSessionStatus } from '@/types/terminal'

export interface CreateSessionOptions {
  cwd?: string
  workspaceId?: string
  terminalType?: TerminalType
  memberId?: string
}

export async function createTerminalSession(options?: CreateSessionOptions): Promise<string> {
  const response = await fetch('/api/terminal/sessions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(options ?? {}),
  })
  if (!response.ok) throw new Error('Failed to create terminal session')
  const { id } = await response.json()
  return id
}

export async function closeTerminalSession(sessionId: string): Promise<void> {
  await fetch(`/api/terminal/sessions/${sessionId}`, { method: 'DELETE' })
}

export async function writeToTerminal(sessionId: string, data: string): Promise<void> {
  await fetch(`/api/terminal/sessions/${sessionId}/input`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ data }),
  })
}

export async function resizeTerminal(sessionId: string, cols: number, rows: number): Promise<void> {
  await fetch(`/api/terminal/sessions/${sessionId}/resize`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ cols, rows }),
  })
}

export async function getTerminalOutput(sessionId: string): Promise<string> {
  const response = await fetch(`/api/terminal/sessions/${sessionId}/output`)
  if (!response.ok) return ''
  const { output } = await response.json()
  return output ?? ''
}
```

- [ ] **步骤 3：验证编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无错误

---

### 任务 4：侧边导航组件

**文件：**
- 创建：`src/components/SidebarNav.vue`
- 创建：`src/components/WorkspaceSelection.vue`

- [ ] **步骤 1：创建 SidebarNav.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useWorkspaceStore } from '@/features/workspace/workspaceStore'
import { cn } from '@/lib/utils'
import { 
  MessageSquare, 
  Terminal, 
  Settings, 
  FolderOpen,
  ChevronDown
} from 'lucide-vue-next'
import WorkspaceSelection from './WorkspaceSelection.vue'

const route = useRoute()
const router = useRouter()
const workspaceStore = useWorkspaceStore()

const navItems = [
  { path: '/', icon: MessageSquare, label: '聊天' },
  { path: '/terminal', icon: Terminal, label: '终端' },
  { path: '/settings', icon: Settings, label: '设置' },
]

const isActive = (path: string) => {
  if (path === '/') return route.path === '/'
  return route.path.startsWith(path)
}
</script>

<template>
  <div class="w-14 bg-gray-900 flex flex-col items-center py-3">
    <!-- Logo -->
    <div class="w-10 h-10 rounded-lg bg-blue-600 flex items-center justify-center mb-6">
      <span class="text-white font-bold text-lg">C</span>
    </div>

    <!-- Workspace Selector -->
    <div class="mb-6">
      <WorkspaceSelection />
    </div>

    <!-- Navigation Items -->
    <nav class="flex-1 flex flex-col items-center gap-2">
      <button
        v-for="item in navItems"
        :key="item.path"
        :class="cn(
          'w-10 h-10 rounded-lg flex items-center justify-center transition-colors',
          isActive(item.path)
            ? 'bg-gray-700 text-white'
            : 'text-gray-400 hover:bg-gray-800 hover:text-gray-200'
        )"
        :title="item.label"
        @click="router.push(item.path)"
      >
        <component :is="item.icon" class="h-5 w-5" />
      </button>
    </nav>

    <!-- Bottom Actions -->
    <div class="mt-auto flex flex-col items-center gap-2">
      <button
        :class="cn(
          'w-10 h-10 rounded-lg flex items-center justify-center text-gray-400 hover:bg-gray-800 hover:text-gray-200'
        )"
        title="打开文件夹"
      >
        <FolderOpen class="h-5 w-5" />
      </button>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：创建 WorkspaceSelection.vue**

```vue
<script setup lang="ts">
import { ref, computed } from 'vue'
import { useWorkspaceStore } from '@/features/workspace/workspaceStore'
import { useProjectStore } from '@/features/workspace/projectStore'
import { cn } from '@/lib/utils'
import { ChevronDown, Plus, FolderOpen } from 'lucide-vue-next'

const workspaceStore = useWorkspaceStore()
const projectStore = useProjectStore()

const isOpen = ref(false)

const handleSelectWorkspace = async (id: string) => {
  workspaceStore.selectWorkspace(id)
  await projectStore.loadProject(id)
  isOpen.value = false
}

const handleAddWorkspace = () => {
  // TODO: 打开文件夹选择对话框
  console.log('Add workspace')
}
</script>

<template>
  <div class="relative">
    <button
      :class="cn(
        'w-10 h-10 rounded-lg flex items-center justify-center transition-colors',
        isOpen ? 'bg-gray-700 text-white' : 'text-gray-400 hover:bg-gray-800 hover:text-gray-200'
      )"
      title="选择工作区"
      @click="isOpen = !isOpen"
    >
      <ChevronDown class="h-5 w-5" />
    </button>

    <!-- Dropdown -->
    <div
      v-if="isOpen"
      class="absolute left-12 top-0 w-56 bg-white rounded-lg shadow-lg border z-50"
    >
      <div class="p-2">
        <div class="text-xs text-gray-500 px-2 py-1">工作区</div>
        <button
          v-for="workspace in workspaceStore.workspaces"
          :key="workspace.id"
          :class="cn(
            'w-full px-2 py-2 text-left text-sm rounded-md flex items-center gap-2',
            workspace.id === workspaceStore.currentWorkspaceId
              ? 'bg-blue-50 text-blue-600'
              : 'hover:bg-gray-50'
          )"
          @click="handleSelectWorkspace(workspace.id)"
        >
          <FolderOpen class="h-4 w-4 text-gray-400" />
          <span class="truncate">{{ workspace.name }}</span>
        </button>
        
        <div class="border-t mt-2 pt-2">
          <button
            class="w-full px-2 py-2 text-left text-sm rounded-md flex items-center gap-2 hover:bg-gray-50 text-gray-600"
            @click="handleAddWorkspace"
          >
            <Plus class="h-4 w-4" />
            添加工作区
          </button>
        </div>
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 3：验证编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无错误

---

### 任务 5：终端面板组件

**文件：**
- 创建：`src/components/TerminalWorkspace.vue`
- 创建：`src/components/TerminalPane.vue`
- 安装 xterm.js 依赖

- [ ] **步骤 1：安装 xterm.js**

运行：`cd ui/clawteam-chat && pnpm add xterm @xterm/addon-fit @xterm/addon-web-links`
预期：依赖安装成功

- [ ] **步骤 2：创建 TerminalPane.vue**

```vue
<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch } from 'vue'
import { Terminal } from 'xterm'
import { FitAddon } from '@xterm/addon-fit'
import { WebLinksAddon } from '@xterm/addon-web-links'
import 'xterm/css/xterm.css'
import { useTerminalStore } from '@/features/terminal/terminalStore'
import { writeToTerminal, resizeTerminal } from '@/features/terminal/terminalBridge'
import type { TerminalSession } from '@/types/terminal'

interface Props {
  session: TerminalSession
  active: boolean
}

const props = defineProps<Props>()

const terminalRef = ref<HTMLElement | null>(null)
const terminalStore = useTerminalStore()

let terminal: Terminal | null = null
let fitAddon: FitAddon | null = null

onMounted(() => {
  if (!terminalRef.value) return

  terminal = new Terminal({
    fontSize: 13,
    fontFamily: 'Consolas, Monaco, monospace',
    theme: {
      background: '#1e1e1e',
      foreground: '#d4d4d4',
      cursor: '#ffffff',
      selection: 'rgba(255, 255, 255, 0.3)',
    },
    cursorBlink: true,
    scrollback: 5000,
  })

  fitAddon = new FitAddon()
  terminal.loadAddon(fitAddon)
  terminal.loadAddon(new WebLinksAddon())

  terminal.open(terminalRef.value)
  fitAddon.fit()

  // 处理输入
  terminal.onData(async (data) => {
    await writeToTerminal(props.session.id, data)
  })

  // 处理调整大小
  terminal.onResize(async ({ cols, rows }) => {
    await resizeTerminal(props.session.id, cols, rows)
  })

  // 初始输出
  const output = terminalStore.outputs[props.session.id]
  if (output) {
    terminal.write(output)
  }
})

watch(() => terminalStore.outputs[props.session.id], (newOutput) => {
  if (terminal && newOutput) {
    terminal.write(newOutput)
    terminalStore.outputs[props.session.id] = ''
  }
})

onUnmounted(() => {
  terminal?.dispose()
})
</script>

<template>
  <div 
    ref="terminalRef" 
    :class="[
      'h-full w-full bg-[#1e1e1e]',
      !active && 'opacity-50'
    ]"
  />
</template>
```

- [ ] **步骤 3：创建 TerminalWorkspace.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { useTerminalStore } from '@/features/terminal/terminalStore'
import TerminalPane from './TerminalPane.vue'
import { Plus, X, ChevronLeft, ChevronRight } from 'lucide-vue-next'
import { cn } from '@/lib/utils'

const terminalStore = useTerminalStore()

const sessions = computed(() => terminalStore.sessionsList)
const activeId = computed(() => terminalStore.activeSessionId)

const handleAddTerminal = () => {
  // TODO: 创建新终端
  console.log('Add terminal')
}

const handleCloseTerminal = (id: string) => {
  terminalStore.closeSession(id)
}

const handleSelectTerminal = (id: string) => {
  terminalStore.setActiveSession(id)
}
</script>

<template>
  <div class="h-full flex flex-col bg-gray-900">
    <!-- Tab Bar -->
    <div class="flex items-center bg-gray-800 border-b border-gray-700">
      <div class="flex-1 flex items-center overflow-x-auto">
        <button
          v-for="session in sessions"
          :key="session.id"
          :class="cn(
            'px-3 py-2 text-sm flex items-center gap-2 border-r border-gray-700 min-w-[120px] max-w-[200px]',
            session.id === activeId
              ? 'bg-gray-900 text-white'
              : 'bg-gray-800 text-gray-400 hover:bg-gray-750'
          )"
          @click="handleSelectTerminal(session.id)"
        >
          <span class="truncate flex-1">{{ session.terminalType }}</span>
          <button
            class="hover:bg-gray-700 rounded p-0.5"
            @click.stop="handleCloseTerminal(session.id)"
          >
            <X class="h-3 w-3" />
          </button>
        </button>
      </div>
      
      <button
        class="px-3 py-2 text-gray-400 hover:text-white hover:bg-gray-700"
        @click="handleAddTerminal"
      >
        <Plus class="h-4 w-4" />
      </button>
    </div>

    <!-- Terminal Content -->
    <div class="flex-1 relative">
      <div
        v-for="session in sessions"
        :key="session.id"
        v-show="session.id === activeId"
        class="absolute inset-0"
      >
        <TerminalPane
          :session="session"
          :active="session.id === activeId"
        />
      </div>
      
      <div
        v-if="sessions.length === 0"
        class="h-full flex items-center justify-center text-gray-500"
      >
        <div class="text-center">
          <p class="text-sm">暂无终端会话</p>
          <button
            class="mt-2 text-blue-400 hover:text-blue-300 text-sm"
            @click="handleAddTerminal"
          >
            + 创建新终端
          </button>
        </div>
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 4：验证编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无错误

---

### 任务 6：成员侧栏和会话列表

**文件：**
- 创建：`src/components/MembersSidebar.vue`
- 创建：`src/components/ChatSidebar.vue`

- [ ] **步骤 1：创建 MembersSidebar.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { useProjectStore } from '@/features/workspace/projectStore'
import { cn } from '@/lib/utils'
import { Bot, User, Eye, Circle } from 'lucide-vue-next'
import type { Member, MemberStatus } from '@/types/workspace'

const projectStore = useProjectStore()
const members = computed(() => projectStore.members)

const statusColors: Record<MemberStatus, string> = {
  online: 'text-green-500',
  working: 'text-blue-500',
  dnd: 'text-red-500',
  offline: 'text-gray-400',
}

const roleIcons = {
  assistant: Bot,
  worker: User,
  supervisor: Eye,
  owner: User,
  admin: User,
  member: User,
}
</script>

<template>
  <div class="w-56 border-l bg-white flex flex-col">
    <div class="px-3 py-2 border-b">
      <h3 class="text-sm font-medium text-gray-700">成员</h3>
    </div>
    
    <div class="flex-1 overflow-auto p-2 space-y-1">
      <button
        v-for="member in members"
        :key="member.id"
        class="w-full px-2 py-2 rounded-md flex items-center gap-2 hover:bg-gray-50 text-left"
      >
        <div class="relative">
          <div class="w-8 h-8 rounded-full bg-gray-200 flex items-center justify-center">
            <component :is="roleIcons[member.roleType]" class="h-4 w-4 text-gray-500" />
          </div>
          <Circle
            :class="cn('absolute -bottom-0.5 -right-0.5 w-3 h-3 fill-current', statusColors[member.status])"
          />
        </div>
        <div class="flex-1 min-w-0">
          <div class="text-sm font-medium truncate">{{ member.name }}</div>
          <div class="text-xs text-gray-500 truncate">{{ member.role }}</div>
        </div>
      </button>
      
      <div v-if="members.length === 0" class="text-center text-gray-400 py-4 text-sm">
        暂无成员
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：创建 ChatSidebar.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { useChatStore } from '@/stores/chat'
import { cn } from '@/lib/utils'
import { MessageSquare, Plus } from 'lucide-vue-next'
import type { Session } from '@/types/message'

const chatStore = useChatStore()

const sessions = computed(() => {
  return Object.values(chatStore.sessions).sort((a, b) => b.updatedAt - a.updatedAt)
})

const handleSelectSession = (id: string) => {
  chatStore.setCurrentSession(id)
}

const handleNewSession = () => {
  const id = `session-${Date.now()}`
  chatStore.createSession({
    id,
    name: `新会话 ${Object.keys(chatStore.sessions).length + 1}`,
    createdAt: Date.now(),
    updatedAt: Date.now(),
    messages: [],
  })
}

const formatTime = (timestamp: number) => {
  const date = new Date(timestamp)
  const now = new Date()
  if (date.toDateString() === now.toDateString()) {
    return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
  }
  return date.toLocaleDateString([], { month: 'short', day: 'numeric' })
}
</script>

<template>
  <div class="w-64 border-r bg-gray-50 flex flex-col">
    <div class="px-3 py-2 border-b flex items-center justify-between">
      <h3 class="text-sm font-medium text-gray-700">会话</h3>
      <button
        class="p-1 hover:bg-gray-200 rounded"
        @click="handleNewSession"
      >
        <Plus class="h-4 w-4 text-gray-500" />
      </button>
    </div>
    
    <div class="flex-1 overflow-auto p-2 space-y-1">
      <button
        v-for="session in sessions"
        :key="session.id"
        :class="cn(
          'w-full px-3 py-2 rounded-md text-left',
          session.id === chatStore.currentSessionId
            ? 'bg-blue-50 text-blue-600'
            : 'hover:bg-gray-100'
        )"
        @click="handleSelectSession(session.id)"
      >
        <div class="flex items-center gap-2">
          <MessageSquare class="h-4 w-4 text-gray-400 flex-shrink-0" />
          <div class="flex-1 min-w-0">
            <div class="text-sm font-medium truncate">{{ session.name }}</div>
            <div class="text-xs text-gray-500 truncate">
              {{ session.messages.length }} 条消息
            </div>
          </div>
        </div>
      </button>
      
      <div v-if="sessions.length === 0" class="text-center text-gray-400 py-4 text-sm">
        暂无会话
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 3：验证编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无错误

---

### 任务 7：设置页面

**文件：**
- 创建：`src/components/Settings.vue`
- 保留：`src/components/channel/*`（Feishu/Weixin 配置）

- [ ] **步骤 1：创建 Settings.vue**

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { useChannelStore } from '@/stores/channel'
import ChannelConfig from './channel/ChannelConfig.vue'
import { Button, Input, Switch } from '@/components/ui'
import { Save, RefreshCw } from 'lucide-vue-next'

const channelStore = useChannelStore()
const channelConfigOpen = ref(false)

// 通用设置
const settings = ref({
  theme: 'system',
  language: 'zh-CN',
  autoStartAgents: true,
  soundEnabled: true,
})

const handleSaveSettings = () => {
  console.log('Save settings:', settings.value)
}
</script>

<template>
  <div class="h-full flex">
    <!-- Settings Sidebar -->
    <div class="w-48 border-r bg-gray-50 p-3">
      <nav class="space-y-1">
        <button class="w-full px-3 py-2 text-sm text-left rounded-md bg-white shadow-sm">
          通用设置
        </button>
        <button 
          class="w-full px-3 py-2 text-sm text-left rounded-md hover:bg-gray-100"
          @click="channelConfigOpen = true"
        >
          渠道配置
        </button>
        <button class="w-full px-3 py-2 text-sm text-left rounded-md hover:bg-gray-100">
          关于
        </button>
      </nav>
    </div>

    <!-- Settings Content -->
    <div class="flex-1 overflow-auto p-6">
      <div class="max-w-2xl">
        <h1 class="text-xl font-semibold mb-6">通用设置</h1>
        
        <div class="space-y-6">
          <!-- 外观 -->
          <div>
            <h2 class="text-sm font-medium text-gray-500 uppercase tracking-wide mb-3">外观</h2>
            <div class="space-y-4">
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">主题</div>
                  <div class="text-xs text-gray-500">选择界面主题</div>
                </div>
                <select v-model="settings.theme" class="border rounded-md px-3 py-1.5 text-sm">
                  <option value="system">跟随系统</option>
                  <option value="light">浅色</option>
                  <option value="dark">深色</option>
                </select>
              </div>
              
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">语言</div>
                  <div class="text-xs text-gray-500">界面语言</div>
                </div>
                <select v-model="settings.language" class="border rounded-md px-3 py-1.5 text-sm">
                  <option value="zh-CN">简体中文</option>
                  <option value="en-US">English</option>
                </select>
              </div>
            </div>
          </div>

          <!-- 行为 -->
          <div>
            <h2 class="text-sm font-medium text-gray-500 uppercase tracking-wide mb-3">行为</h2>
            <div class="space-y-4">
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">自动启动 Agent</div>
                  <div class="text-xs text-gray-500">工作区打开时自动启动配置的 Agent</div>
                </div>
                <Switch v-model="settings.autoStartAgents" />
              </div>
              
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">声音提醒</div>
                  <div class="text-xs text-gray-500">接收消息时播放提示音</div>
                </div>
                <Switch v-model="settings.soundEnabled" />
              </div>
            </div>
          </div>
        </div>

        <div class="mt-8 pt-4 border-t">
          <Button @click="handleSaveSettings">
            <Save class="h-4 w-4 mr-2" />
            保存设置
          </Button>
        </div>
      </div>
    </div>

    <!-- Channel Config Modal -->
    <ChannelConfig
      :open="channelConfigOpen"
      @update:open="channelConfigOpen = $event"
    />
  </div>
</template>
```

- [ ] **步骤 2：验证编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无错误

---

### 任务 8：重构 App.vue 和路由

**文件：**
- 修改：`src/App.vue`
- 修改：`src/router/index.ts`
- 修改：`src/views/ChatView.vue`

- [ ] **步骤 1：更新路由配置**

```typescript
// src/router/index.ts
import { createRouter, createWebHistory } from 'vue-router'
import ChatView from '@/views/ChatView.vue'
import Settings from '@/components/Settings.vue'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      name: 'chat',
      component: ChatView,
    },
    {
      path: '/terminal',
      name: 'terminal',
      component: () => import('@/components/TerminalWorkspace.vue'),
    },
    {
      path: '/settings',
      name: 'settings',
      component: Settings,
    },
  ],
})

export default router
```

- [ ] **步骤 2：重构 App.vue**

```vue
<script setup lang="ts">
import { onMounted } from 'vue'
import { RouterView } from 'vue-router'
import SidebarNav from '@/components/SidebarNav.vue'
import { useWorkspaceStore } from '@/features/workspace/workspaceStore'
import { useProjectStore } from '@/features/workspace/projectStore'
import { useAgentStore } from '@/stores/agent'
import { useChannelStore } from '@/stores/channel'

const workspaceStore = useWorkspaceStore()
const projectStore = useProjectStore()
const agentStore = useAgentStore()
const channelStore = useChannelStore()

onMounted(async () => {
  await workspaceStore.loadWorkspaces()
  if (workspaceStore.currentWorkspaceId) {
    await projectStore.loadProject(workspaceStore.currentWorkspaceId)
  }
  await agentStore.loadAgents()
  await channelStore.loadConfigs()
})
</script>

<template>
  <div class="h-screen flex bg-gray-50">
    <SidebarNav />
    <main class="flex-1 overflow-hidden">
      <RouterView />
    </main>
  </div>
</template>
```

- [ ] **步骤 3：更新 ChatView.vue**

```vue
<script setup lang="ts">
import { onMounted, ref } from 'vue'
import ChatInterface from '@/components/ChatInterface.vue'
import ChatSidebar from '@/components/ChatSidebar.vue'
import MembersSidebar from '@/components/MembersSidebar.vue'
import { useChatStore } from '@/stores/chat'
import { storeToRefs } from 'pinia'

const chatStore = useChatStore()

onMounted(() => {
  // 创建默认会话
  if (Object.keys(chatStore.sessions).length === 0) {
    chatStore.createSession({
      id: 'default',
      name: '默认会话',
      createdAt: Date.now(),
      updatedAt: Date.now(),
      messages: [],
    })
  }
})
</script>

<template>
  <div class="h-full flex">
    <ChatSidebar />
    <ChatInterface />
    <MembersSidebar />
  </div>
</template>
```

- [ ] **步骤 4：更新 ChatInterface.vue**

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { storeToRefs } from 'pinia'
import MessageList from './MessageList.vue'
import ChatInput from './ChatInput.vue'
import OutputPanel from './OutputPanel.vue'
import SkillStore from './skill/SkillStore.vue'
import AuditPanel from './audit/AuditPanel.vue'
import ChannelConfig from './channel/ChannelConfig.vue'
import { useChatStore } from '@/stores/chat'
import { Button } from '@/components/ui'
import { Settings, Package, ScrollText, PanelRightOpen, PanelRightClose } from 'lucide-vue-next'

const chatStore = useChatStore()
const { currentMessages, currentSessionId } = storeToRefs(chatStore)

const showOutput = ref(false)
const showSkillStore = ref(false)
const showAuditPanel = ref(false)
const channelConfigOpen = ref(false)

const handleSend = (content: string, skillId?: string) => {
  const sessionId = currentSessionId.value || 'default'

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
}
</script>

<template>
  <div class="flex-1 flex flex-col min-w-0">
    <!-- Header -->
    <div class="border-b px-4 py-2 flex items-center justify-between bg-white">
      <h1 class="font-semibold text-sm">ClawTeam Chat</h1>
      <div class="flex items-center gap-1">
        <Button
          variant="ghost"
          size="sm"
          class="h-8 text-xs"
          :class="showSkillStore && 'bg-blue-50 text-blue-600'"
          @click="showSkillStore = !showSkillStore"
        >
          <Package class="h-3.5 w-3.5 mr-1" />
          Skill
        </Button>
        <Button
          variant="ghost"
          size="sm"
          class="h-8 text-xs"
          :class="showAuditPanel && 'bg-blue-50 text-blue-600'"
          @click="showAuditPanel = !showAuditPanel"
        >
          <ScrollText class="h-3.5 w-3.5 mr-1" />
          审计
        </Button>
        <Button
          variant="ghost"
          size="sm"
          class="h-8 text-xs"
          @click="channelConfigOpen = true"
        >
          <Settings class="h-3.5 w-3.5 mr-1" />
          渠道
        </Button>
        <Button
          variant="ghost"
          size="sm"
          class="h-8 text-xs"
          @click="showOutput = !showOutput"
        >
          <component :is="showOutput ? PanelRightClose : PanelRightOpen" class="h-3.5 w-3.5 mr-1" />
          输出
        </Button>
      </div>
    </div>

    <!-- Main Content -->
    <div class="flex-1 flex overflow-hidden">
      <div class="flex-1 flex flex-col min-w-0">
        <MessageList :messages="currentMessages" />
        <ChatInput @send="handleSend" />
      </div>

      <SkillStore v-if="showSkillStore" />
      <OutputPanel v-if="showOutput" />
      <div v-if="showAuditPanel" class="w-80 border-l bg-white">
        <AuditPanel />
      </div>
    </div>

    <!-- Channel Config Modal -->
    <ChannelConfig
      :open="channelConfigOpen"
      @update:open="channelConfigOpen = $event"
    />
  </div>
</template>
```

- [ ] **步骤 5：验证编译**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无错误

---

### 任务 9：集成测试

**文件：**
- 无新增文件，运行现有测试

- [ ] **步骤 1：运行类型检查**

运行：`cd ui/clawteam-chat && pnpm check`
预期：无类型错误

- [ ] **步骤 2：运行单元测试**

运行：`cd ui/clawteam-chat && pnpm test`
预期：所有测试通过

- [ ] **步骤 3：启动开发服务器验证**

运行：`cd ui/clawteam-chat && pnpm dev`
预期：
- 应用正常启动
- 侧边导航显示正确
- 工作区选择可用
- 聊天界面正常
- 成员侧栏显示
- 设置页面可访问
- Feishu/Weixin 配置可用

- [ ] **步骤 4：构建验证**

运行：`cd ui/clawteam-chat && pnpm build`
预期：构建成功，无错误

---

### 任务 10：Commit 变更

- [ ] **步骤 1：检查变更**

运行：`git status`
预期：所有新增和修改的文件已列出

- [ ] **步骤 2：暂存变更**

```bash
git add ui/clawteam-chat/src/types/workspace.ts
git add ui/clawteam-chat/src/types/terminal.ts
git add ui/clawteam-chat/src/features/workspace/
git add ui/clawteam-chat/src/features/terminal/
git add ui/clawteam-chat/src/components/SidebarNav.vue
git add ui/clawteam-chat/src/components/WorkspaceSelection.vue
git add ui/clawteam-chat/src/components/TerminalWorkspace.vue
git add ui/clawteam-chat/src/components/TerminalPane.vue
git add ui/clawteam-chat/src/components/MembersSidebar.vue
git add ui/clawteam-chat/src/components/ChatSidebar.vue
git add ui/clawteam-chat/src/components/Settings.vue
git add ui/clawteam-chat/src/App.vue
git add ui/clawteam-chat/src/router/index.ts
git add ui/clawteam-chat/src/views/ChatView.vue
git add ui/clawteam-chat/src/components/ChatInterface.vue
git add ui/clawteam-chat/package.json
```

- [ ] **步骤 3：提交变更**

```bash
git commit -m "feat(ui): restore golutra-style UI with workspace and terminal support

- Add workspace and project state management
- Add terminal store and bridge for xterm.js integration
- Add SidebarNav with workspace selection
- Add TerminalWorkspace and TerminalPane components
- Add MembersSidebar and ChatSidebar
- Add Settings page with channel config integration
- Preserve Feishu/Weixin channel configuration
- Refactor App.vue and ChatView.vue layout"
```

---

## 验收标准

1. **UI 布局**：还原 golutra 风格的三栏布局（侧边导航 + 主内容 + 成员侧栏）
2. **工作空间管理**：支持多工作区切换
3. **终端面板**：支持多终端标签页，使用 xterm.js
4. **聊天功能**：保留原有聊天功能，新增会话列表侧栏
5. **成员显示**：显示工作区成员及其状态
6. **设置页面**：新增通用设置页
7. **渠道配置**：Feishu/Weixin 配置功能完整保留
8. **无类型错误**：所有 TypeScript 检查通过
9. **无运行时错误**：应用正常启动和运行

---

## 风险与注意事项

1. **xterm.js 版本兼容性**：确保 xterm.js 6.x 与 Vue 3 兼容
2. **路由模式**：根据实际部署环境选择 history 或 hash 模式
3. **API 接口**：部分 API 调用可能需要 mock 或后端支持
4. **移动端适配**：当前设计主要针对桌面端
