# ClawTeam 前端开发计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 基于 Vue 3.5 + Pinia + Tailwind CSS 构建四视图前端应用（聊天、员工管理、技能商店、设置），匹配原型 UI.html 设计

**架构：** 三栏布局（全局导航 64px | 上下文侧边栏 256px | 主内容区）+ 四视图路由 + 深色主题

**技术栈：** Vue 3.5.12 + Pinia 3.0.4 + @pinia/colada 0.14.0 + Vue Router 4.5.0 + Tailwind CSS 4.1.17 + @vueuse/core 13.0.0 + lucide-vue-next 0.553.0

---

## 文件结构

### 新建文件

```
ui/clawteam-chat/src/
├── components/
│   ├── layout/
│   │   ├── AppLayout.vue           # 主布局容器
│   │   ├── GlobalNav.vue           # 左侧全局导航 (64px)
│   │   ├── ContextSidebar.vue      # 上下文侧边栏 (256px)
│   │   └── TopBar.vue              # 顶部栏
│   ├── agent/
│   │   ├── AgentView.vue           # 员工管理视图
│   │   ├── AgentEditModal.vue      # Agent 编辑弹窗
│   │   ├── ExpertPanel.vue         # 专家中心面板
│   │   ├── ExpertCard.vue          # 专家卡片 (白底)
│   │   ├── ExpertModal.vue         # 添加专家弹窗
│   │   └── CustomExpertCard.vue    # 自定义专家卡片
│   ├── skill/
│   │   ├── SkillView.vue           # 技能商店视图
│   │   ├── SkillCard.vue           # 技能卡片
│   │   └── SkillFilters.vue        # 技能筛选组件
│   ├── settings/
│   │   ├── SettingsView.vue        # 设置视图
│   │   ├── AccountSettings.vue     # 账号设置
│   │   ├── AppearanceSettings.vue  # 外观设置
│   │   ├── CliSettings.vue         # CLI 配置
│   │   ├── CliCard.vue             # CLI 工具卡片
│   │   └── ShellCard.vue           # 终端卡片
│   └── common/
│       ├── Toast.vue               # Toast 通知
│       └── Modal.vue               # 通用弹窗
├── stores/
│   ├── expert.ts                   # 专家状态管理
│   ├── settings.ts                 # 设置状态管理
│   └── toast.ts                    # Toast 通知管理
├── types/
│   ├── expert.ts                   # 专家类型
│   └── settings.ts                 # 设置类型
└── views/
    ├── AgentsView.vue              # 员工管理视图入口
    └── SkillsView.vue              # 技能商店视图入口
```

### 修改文件

```
ui/clawteam-chat/src/
├── App.vue                         # 重构为使用 AppLayout
├── main.ts                         # 注册新 store
├── index.css                       # 扩展深色主题样式
├── router/index.ts                 # 添加新路由
├── stores/
│   ├── agent.ts                    # 扩展专家管理
│   └── index.ts                    # 导出新 store
└── components/
    ├── SidebarNav.vue              # 重构为 GlobalNav
    └── Settings.vue                # 迁移到 SettingsView
```

---

## 任务分解

### 任务 1：类型定义

**文件：**
- 创建：`ui/clawteam-chat/src/types/expert.ts`
- 创建：`ui/clawteam-chat/src/types/settings.ts`

- [ ] **步骤 1：创建 expert.ts 类型定义**

```typescript
// ui/clawteam-chat/src/types/expert.ts

export interface Expert {
  id: number
  name: string
  role: string
  category: string
  desc: string
  tags: string[]
  added: boolean
  avatar?: string
}

export const EXPERT_CATEGORIES = [
  '全部',
  '设计',
  '工程技术',
  '内容创作',
  '数据分析',
  '产品管理',
  '市场营销',
  '其他',
] as const

export type ExpertCategory = (typeof EXPERT_CATEGORIES)[number]
```

- [ ] **步骤 2：创建 settings.ts 类型定义**

```typescript
// ui/clawteam-chat/src/types/settings.ts

export type Theme = 'dark' | 'light' | 'system'

export interface CliConfig {
  id: string
  name: string
  command: string
  installed: boolean
  custom?: boolean
  icon?: string
}

export interface ShellConfig {
  id: string
  name: string
  path: string
}

export interface SettingsState {
  theme: Theme
  selectedCli: string
  selectedShell: string
  cliTools: CliConfig[]
  shells: ShellConfig[]
  pathPool: string[]
}
```

- [ ] **步骤 3：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

预期：无错误

- [ ] **步骤 4：Commit**

```bash
git add ui/clawteam-chat/src/types/expert.ts ui/clawteam-chat/src/types/settings.ts
git commit -m "feat(ui): add expert and settings type definitions"
```

---

### 任务 2：Toast Store 和组件

**文件：**
- 创建：`ui/clawteam-chat/src/stores/toast.ts`
- 创建：`ui/clawteam-chat/src/components/common/Toast.vue`

- [ ] **步骤 1：创建 toast.ts store**

```typescript
// ui/clawteam-chat/src/stores/toast.ts
import { defineStore } from 'pinia'
import { ref } from 'vue'

export type ToastType = 'success' | 'error' | 'info'

export interface Toast {
  id: number
  type: ToastType
  message: string
}

export const useToastStore = defineStore('toast', () => {
  const toasts = ref<Toast[]>([])
  let nextId = 1

  function show(type: ToastType, message: string, duration = 2500) {
    const id = nextId++
    toasts.value.push({ id, type, message })
    setTimeout(() => remove(id), duration)
  }

  function remove(id: number) {
    toasts.value = toasts.value.filter((t) => t.id !== id)
  }

  function success(message: string) {
    show('success', message)
  }
  function error(message: string) {
    show('error', message)
  }
  function info(message: string) {
    show('info', message)
  }

  return { toasts, show, remove, success, error, info }
})
```

- [ ] **步骤 2：创建 Toast.vue 组件**

```vue
<!-- ui/clawteam-chat/src/components/common/Toast.vue -->
<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useToastStore, type ToastType } from '@/stores/toast'
import { CheckCircle, XCircle, Info } from 'lucide-vue-next'

const toastStore = useToastStore()
const { toasts } = storeToRefs(toastStore)

const iconMap: Record<ToastType, typeof CheckCircle> = {
  success: CheckCircle,
  error: XCircle,
  info: Info,
}

const colorMap: Record<ToastType, string> = {
  success: 'text-green-500',
  error: 'text-red-500',
  info: 'text-sky-500',
}
</script>

<template>
  <Teleport to="body">
    <div class="fixed top-5 right-5 z-[1000] flex flex-col gap-2">
      <TransitionGroup name="toast">
        <div
          v-for="toast in toasts"
          :key="toast.id"
          class="bg-gray-800 border border-gray-700 rounded-xl px-4 py-3 text-sm text-gray-200 shadow-lg flex items-center gap-2.5 min-w-[260px]"
        >
          <component :is="iconMap[toast.type]" :class="['w-4 h-4', colorMap[toast.type]]" />
          <span>{{ toast.message }}</span>
        </div>
      </TransitionGroup>
    </div>
  </Teleport>
</template>

<style scoped>
.toast-enter-active {
  animation: toastIn 0.3s ease;
}
.toast-leave-active {
  animation: toastOut 0.3s ease;
}

@keyframes toastIn {
  from {
    opacity: 0;
    transform: translateX(30px);
  }
  to {
    opacity: 1;
    transform: translateX(0);
  }
}

@keyframes toastOut {
  from {
    opacity: 1;
    transform: translateX(0);
  }
  to {
    opacity: 0;
    transform: translateX(30px);
  }
}
</style>
```

- [ ] **步骤 3：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 4：Commit**

```bash
git add ui/clawteam-chat/src/stores/toast.ts ui/clawteam-chat/src/components/common/Toast.vue
git commit -m "feat(ui): add toast store and component"
```

---

### 任务 3：Expert Store

**文件：**
- 创建：`ui/clawteam-chat/src/stores/expert.ts`

- [ ] **步骤 1：创建 expert.ts store**

```typescript
// ui/clawteam-chat/src/stores/expert.ts
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { Expert, ExpertCategory } from '@/types/expert'

// 预置专家数据
const DEFAULT_EXPERTS: Expert[] = [
  {
    id: 1,
    name: 'Kai',
    role: '内容创作专家',
    category: '内容创作',
    desc: '擅长创作引人入胜的多平台内容，让品牌故事触达目标受众',
    tags: ['文案', '品牌', '社媒'],
    added: true,
  },
  {
    id: 2,
    name: 'Phoebe',
    role: '数据分析报告师',
    category: '数据分析',
    desc: '将复杂数据转化为清晰可执行的业务报告，让数据驱动决策',
    tags: ['数据清洗', '可视化', '报告'],
    added: true,
  },
  {
    id: 3,
    name: 'Jude',
    role: '中国电商运营专家',
    category: '市场营销',
    desc: '精通天猫京东拼多多等多平台运营，从选品到爆款一站式操盘',
    tags: ['电商', '运营', '选品'],
    added: true,
  },
  {
    id: 4,
    name: 'Maya',
    role: '抖音策略师',
    category: '市场营销',
    desc: '精通抖音算法和内容生态，打造短视频爆款并实现商业化变现',
    tags: ['抖音', '短视频', '变现'],
    added: false,
  },
  {
    id: 5,
    name: 'Sam',
    role: 'UI设计师',
    category: '设计',
    desc: '专注于用户体验与界面美学，打造直观且美观的数字产品界面',
    tags: ['UI', 'UX', '设计系统'],
    added: false,
  },
  {
    id: 6,
    name: 'Ula',
    role: '销售教练',
    category: '产品管理',
    desc: '提供实战销售技巧培训，帮助团队提升转化率与客单价',
    tags: ['销售', '培训', '转化'],
    added: false,
  },
  {
    id: 7,
    name: 'Ben',
    role: '品牌策略师',
    category: '市场营销',
    desc: '构建品牌核心价值，制定长期品牌发展战略与视觉识别系统',
    tags: ['品牌', '策略', 'VI'],
    added: false,
  },
  {
    id: 8,
    name: 'Fay',
    role: '小红书运营专家',
    category: '内容创作',
    desc: '深谙小红书种草逻辑，通过笔记优化与社群互动提升品牌曝光',
    tags: ['小红书', '种草', '社群'],
    added: false,
  },
]

export const useExpertStore = defineStore('expert', () => {
  const experts = ref<Expert[]>([...DEFAULT_EXPERTS])
  const activeCategory = ref<ExpertCategory>('全部')
  const searchQuery = ref('')
  let nextId = 100

  // Getters
  const categories = computed(() => {
    const cats = new Set(experts.value.map((e) => e.category))
    return ['全部', ...Array.from(cats)] as ExpertCategory[]
  })

  const filteredExperts = computed(() => {
    return experts.value.filter((e) => {
      const matchCategory =
        activeCategory.value === '全部' || e.category === activeCategory.value
      const q = searchQuery.value.toLowerCase()
      const matchSearch =
        !q ||
        e.name.toLowerCase().includes(q) ||
        e.role.toLowerCase().includes(q) ||
        e.desc.toLowerCase().includes(q)
      return matchCategory && matchSearch
    })
  })

  const addedExperts = computed(() => experts.value.filter((e) => e.added))

  const categoryCounts = computed(() => {
    const counts: Record<string, number> = { 全部: experts.value.length }
    for (const e of experts.value) {
      counts[e.category] = (counts[e.category] || 0) + 1
    }
    return counts
  })

  // Actions
  function setActiveCategory(category: ExpertCategory) {
    activeCategory.value = category
  }

  function setSearchQuery(query: string) {
    searchQuery.value = query
  }

  function addExpertToProject(id: number) {
    const expert = experts.value.find((e) => e.id === id)
    if (expert) {
      expert.added = true
    }
  }

  function createExpert(expert: Omit<Expert, 'id' | 'added'>) {
    experts.value.unshift({
      ...expert,
      id: nextId++,
      added: true,
    })
  }

  function removeExpert(id: number) {
    const expert = experts.value.find((e) => e.id === id)
    if (expert) {
      expert.added = false
    }
  }

  return {
    experts,
    activeCategory,
    searchQuery,
    categories,
    filteredExperts,
    addedExperts,
    categoryCounts,
    setActiveCategory,
    setSearchQuery,
    addExpertToProject,
    createExpert,
    removeExpert,
  }
})
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/stores/expert.ts
git commit -m "feat(ui): add expert store with preset data"
```

---

### 任务 4：Settings Store

**文件：**
- 创建：`ui/clawteam-chat/src/stores/settings.ts`

- [ ] **步骤 1：创建 settings.ts store**

```typescript
// ui/clawteam-chat/src/stores/settings.ts
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { Theme, CliConfig, ShellConfig } from '@/types/settings'

const DEFAULT_CLI_TOOLS: CliConfig[] = [
  { id: 'gemini', name: 'Gemini CLI', command: 'gemini', installed: false },
  { id: 'codex', name: 'Codex', command: 'codex', installed: false },
  { id: 'claude', name: 'Claude Code', command: 'claude', installed: true },
  { id: 'opencode', name: 'opencode', command: 'opencode', installed: false },
  { id: 'qwen', name: 'Qwen Code', command: 'qwen', installed: false },
  { id: 'openclaw', name: 'OpenClaw', command: 'openclaw', installed: false },
  { id: 'iflow', name: 'iflow', command: 'iflow -p', installed: true, custom: true },
]

const DEFAULT_SHELLS: ShellConfig[] = [
  { id: 'default', name: '系统默认', path: '使用系统默认 shell' },
  { id: 'powershell', name: 'Windows PowerShell', path: 'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe' },
  { id: 'cmd', name: 'Command Prompt', path: 'C:\\Windows\\system32\\cmd.exe' },
]

export const useSettingsStore = defineStore('settings', () => {
  const theme = ref<Theme>('dark')
  const selectedCli = ref<string>('iflow')
  const selectedShell = ref<string>('cmd')
  const cliTools = ref<CliConfig[]>([...DEFAULT_CLI_TOOLS])
  const shells = ref<ShellConfig[]>([...DEFAULT_SHELLS])
  const pathPool = ref<string[]>([])

  function setTheme(newTheme: Theme) {
    theme.value = newTheme
    if (newTheme === 'dark') {
      document.documentElement.classList.add('dark')
    } else if (newTheme === 'light') {
      document.documentElement.classList.remove('dark')
    }
  }

  function selectCli(id: string) {
    selectedCli.value = id
  }

  function selectShell(id: string) {
    selectedShell.value = id
  }

  function addCliTool(tool: Omit<CliConfig, 'id'>) {
    const id = `custom-${Date.now()}`
    cliTools.value.push({ ...tool, id, custom: true })
  }

  function removeCliTool(id: string) {
    cliTools.value = cliTools.value.filter((t) => t.id !== id)
    if (selectedCli.value === id) {
      selectedCli.value = cliTools.value[0]?.id || ''
    }
  }

  function addPath(path: string) {
    if (path && !pathPool.value.includes(path)) {
      pathPool.value.push(path)
    }
  }

  function removePath(path: string) {
    pathPool.value = pathPool.value.filter((p) => p !== path)
  }

  return {
    theme,
    selectedCli,
    selectedShell,
    cliTools,
    shells,
    pathPool,
    setTheme,
    selectCli,
    selectShell,
    addCliTool,
    removeCliTool,
    addPath,
    removePath,
  }
})
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/stores/settings.ts
git commit -m "feat(ui): add settings store with CLI and shell management"
```

---

### 任务 5：Store 入口和注册

**文件：**
- 修改：`ui/clawteam-chat/src/stores/index.ts`
- 修改：`ui/clawteam-chat/src/main.ts`

- [ ] **步骤 1：更新 stores/index.ts**

```typescript
// ui/clawteam-chat/src/stores/index.ts
export { useAgentStore } from './agent'
export { useAuditStore } from './audit'
export { useChannelStore } from './channel'
export { useChatStore } from './chat'
export { useProjectStore } from './project'
export { useSkillStore } from './skill'
export { useTerminalStore } from './terminal'
export { useWorkspaceStore } from './workspace'
export { useExpertStore } from './expert'
export { useSettingsStore } from './settings'
export { useToastStore } from './toast'
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/stores/index.ts
git commit -m "feat(ui): export new stores from index"
```

---

### 任务 6：全局导航组件 GlobalNav

**文件：**
- 创建：`ui/clawteam-chat/src/components/layout/GlobalNav.vue`

- [ ] **步骤 1：创建 GlobalNav.vue**

```vue
<!-- ui/clawteam-chat/src/components/layout/GlobalNav.vue -->
<script setup lang="ts">
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { MessageSquare, Users, Store, Folder, HelpCircle, Settings } from 'lucide-vue-next'

const route = useRoute()
const router = useRouter()

const navItems = [
  { path: '/chat', icon: MessageSquare, label: '聊天' },
  { path: '/agents', icon: Users, label: '员工' },
  { path: '/skills', icon: Store, label: '商店' },
  { path: '/files', icon: Folder, label: '文件' },
]

const bottomItems = [
  { path: '/help', icon: HelpCircle, label: '帮助' },
  { path: '/settings', icon: Settings, label: '设置' },
]

const isActive = (path: string) => route.path.startsWith(path)

function navigate(path: string) {
  router.push(path)
}
</script>

<template>
  <div
    class="w-16 bg-gray-950 flex flex-col items-center py-4 gap-4 border-r border-gray-800 z-20"
  >
    <!-- Avatar -->
    <div class="relative group cursor-pointer">
      <div
        class="w-10 h-10 rounded-full border-2 border-gray-800 group-hover:border-sky-500 transition-colors bg-gradient-to-br from-purple-500 to-blue-600 flex items-center justify-center text-white font-bold"
      >
        C
      </div>
      <div
        class="absolute top-0 right-0 w-3 h-3 bg-green-500 border-2 border-gray-950 rounded-full"
      ></div>
    </div>

    <div class="w-8 h-px bg-gray-800 my-2"></div>

    <!-- Main Nav -->
    <button
      v-for="item in navItems"
      :key="item.path"
      @click="navigate(item.path)"
      :class="[
        'w-10 h-10 rounded-xl flex items-center justify-center transition-all',
        isActive(item.path)
          ? 'bg-sky-500/20 text-sky-400'
          : 'text-gray-400 hover:text-white hover:bg-gray-800',
      ]"
      :title="item.label"
    >
      <component :is="item.icon" class="w-5 h-5" />
    </button>

    <div class="flex-1"></div>

    <!-- Bottom Nav -->
    <button
      v-for="item in bottomItems"
      :key="item.path"
      @click="navigate(item.path)"
      :class="[
        'w-10 h-10 rounded-xl flex items-center justify-center transition-all',
        isActive(item.path)
          ? 'bg-sky-500/20 text-sky-400'
          : 'text-gray-400 hover:text-white hover:bg-gray-800',
      ]"
      :title="item.label"
    >
      <component :is="item.icon" class="w-5 h-5" />
    </button>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/layout/GlobalNav.vue
git commit -m "feat(ui): add global navigation component"
```

---

### 任务 7：上下文侧边栏 ContextSidebar

**文件：**
- 创建：`ui/clawteam-chat/src/components/layout/ContextSidebar.vue`

- [ ] **步骤 1：创建 ContextSidebar.vue**

```vue
<!-- ui/clawteam-chat/src/components/layout/ContextSidebar.vue -->
<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { GitBranch, Clock, User, Palette, Terminal } from 'lucide-vue-next'
import { useAgentStore } from '@/stores/agent'
import { useExpertStore } from '@/stores/expert'

const route = useRoute()
const agentStore = useAgentStore()
const expertStore = useExpertStore()

const currentView = computed(() => {
  if (route.path.startsWith('/chat')) return 'chat'
  if (route.path.startsWith('/agents')) return 'agents'
  if (route.path.startsWith('/skills')) return 'skills'
  if (route.path.startsWith('/settings')) return 'settings'
  return 'chat'
})

const settingsNavItems = [
  { id: 'account', label: '我的账号', icon: User },
  { id: 'appearance', label: '外观', icon: Palette },
  { id: 'cli', label: 'CLI配置', icon: Terminal },
]
</script>

<template>
  <div class="w-64 bg-gray-900/50 flex flex-col border-r border-gray-800 backdrop-blur-sm">
    <!-- Chat Sidebar -->
    <div v-if="currentView === 'chat'" class="p-4 h-full flex flex-col">
      <div class="flex items-center gap-3 mb-4">
        <div
          class="w-8 h-8 rounded-lg bg-sky-900/30 flex items-center justify-center text-sky-400 border border-sky-900/50"
        >
          <GitBranch class="w-4 h-4" />
        </div>
        <h2 class="text-base font-bold text-white">docs</h2>
      </div>

      <div
        class="text-xs font-semibold text-gray-500 uppercase tracking-wider mb-2"
      >
        频道
      </div>
      <div class="space-y-1 mb-6">
        <div
          class="flex items-center gap-3 px-3 py-2 rounded-lg bg-gray-800 border border-gray-700 cursor-pointer"
        >
          <div class="w-6 h-6 rounded-full bg-gradient-to-br from-purple-500 to-blue-600"></div>
          <div class="flex-1 min-w-0">
            <div class="text-sm font-medium text-white truncate">docs</div>
            <p class="text-xs text-gray-500 truncate">WorkBuddy: 我来看看...</p>
          </div>
        </div>
      </div>

      <div
        class="text-xs font-semibold text-gray-500 uppercase tracking-wider mb-2"
      >
        会话记录
      </div>
      <div class="space-y-1">
        <div
          class="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-gray-800/50 cursor-pointer text-gray-400"
        >
          <Clock class="w-3 h-3" />
          <span class="text-sm">昨天</span>
        </div>
        <div
          class="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-gray-800/50 cursor-pointer text-gray-400"
        >
          <Clock class="w-3 h-3" />
          <span class="text-sm">上周</span>
        </div>
      </div>
    </div>

    <!-- Agents Sidebar -->
    <div v-else-if="currentView === 'agents'" class="p-4 h-full overflow-y-auto">
      <div class="flex items-center gap-3 mb-6">
        <div
          class="w-10 h-10 rounded-xl bg-sky-900/30 flex items-center justify-center text-sky-400 border border-sky-900/50"
        >
          <Users class="w-5 h-5" />
        </div>
        <div>
          <h2 class="text-lg font-bold text-white">员工管理</h2>
          <p class="text-xs text-gray-500">管理助手与成员</p>
        </div>
      </div>
    </div>

    <!-- Settings Sidebar -->
    <div v-else-if="currentView === 'settings'" class="p-4 h-full overflow-y-auto">
      <h2 class="text-xl font-bold text-white mb-6">设置</h2>
      <div class="space-y-1">
        <div class="text-xs font-semibold text-gray-500 uppercase tracking-wider mb-2 ml-2">
          用户设置
        </div>
        <router-link
          v-for="item in settingsNavItems"
          :key="item.id"
          :to="`/settings/${item.id}`"
          class="flex items-center gap-3 px-3 py-2.5 rounded-lg text-gray-400 hover:text-white w-full transition-colors"
          :class="{
            'bg-sky-500/20 text-white': route.path === `/settings/${item.id}`,
          }"
        >
          <component :is="item.icon" class="w-4 h-4" />
          <span class="text-sm">{{ item.label }}</span>
        </router-link>
      </div>
    </div>

    <!-- Skills Sidebar -->
    <div v-else-if="currentView === 'skills'" class="p-4 h-full">
      <h2 class="text-lg font-bold text-white mb-1">商店</h2>
      <p class="text-xs text-gray-500 mb-6">统一管理技能、模板与插件</p>
    </div>
  </div>
</template>

<script lang="ts">
import { Users } from 'lucide-vue-next'
export default {
  components: { Users },
}
</script>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/layout/ContextSidebar.vue
git commit -m "feat(ui): add context sidebar component"
```

---

### 任务 8：主布局组件 AppLayout

**文件：**
- 创建：`ui/clawteam-chat/src/components/layout/AppLayout.vue`

- [ ] **步骤 1：创建 AppLayout.vue**

```vue
<!-- ui/clawteam-chat/src/components/layout/AppLayout.vue -->
<script setup lang="ts">
import GlobalNav from './GlobalNav.vue'
import ContextSidebar from './ContextSidebar.vue'
import Toast from '@/components/common/Toast.vue'
</script>

<template>
  <div class="h-screen flex bg-[#0B0F19] text-gray-200">
    <GlobalNav />
    <ContextSidebar />
    <main class="flex-1 overflow-hidden flex flex-col">
      <slot />
    </main>
    <Toast />
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/layout/AppLayout.vue
git commit -m "feat(ui): add app layout component"
```

---

### 任务 9：专家卡片组件 ExpertCard

**文件：**
- 创建：`ui/clawteam-chat/src/components/agent/ExpertCard.vue`

- [ ] **步骤 1：创建 ExpertCard.vue**

```vue
<!-- ui/clawteam-chat/src/components/agent/ExpertCard.vue -->
<script setup lang="ts">
import type { Expert } from '@/types/expert'

const props = defineProps<{ expert: Expert }>()
const emit = defineEmits<{ add: [expert: Expert] }>()

const avatarUrl = `https://picsum.photos/seed/expert${props.expert.id}/112/112.jpg`
</script>

<template>
  <div
    class="bg-white text-gray-800 rounded-xl p-5 pb-4 flex flex-col items-center border border-gray-200 transition-all cursor-pointer hover:-translate-y-1 hover:shadow-lg hover:border-purple-500 group"
    @click="emit('add', expert)"
  >
    <img :src="avatarUrl" :alt="expert.name" class="w-14 h-14 rounded-full border-3 border-gray-100 mb-2.5" />
    <div class="text-sm font-bold text-gray-900 mb-1.5">{{ expert.name }}</div>
    <span
      class="bg-purple-100 text-purple-700 px-3 py-0.5 rounded-full text-xs font-semibold mb-2"
    >
      {{ expert.role }}
    </span>
    <p class="text-xs text-gray-500 text-center leading-relaxed mb-3.5 flex-1">
      {{ expert.desc }}
    </p>
    <button
      class="w-full py-2 rounded-lg text-xs font-medium bg-gray-100 text-gray-600 border border-gray-200 transition-colors group-hover:bg-purple-500 group-hover:text-white group-hover:border-purple-500"
    >
      {{ expert.added ? '已添加' : '添加专家' }}
    </button>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/agent/ExpertCard.vue
git commit -m "feat(ui): add expert card component (white background design)"
```

---

### 任务 10：专家面板组件 ExpertPanel

**文件：**
- 创建：`ui/clawteam-chat/src/components/agent/ExpertPanel.vue`

- [ ] **步骤 1：创建 ExpertPanel.vue**

```vue
<!-- ui/clawteam-chat/src/components/agent/ExpertPanel.vue -->
<script setup lang="ts">
import { Search } from 'lucide-vue-next'
import { useExpertStore } from '@/stores/expert'
import { useToastStore } from '@/stores/toast'
import ExpertCard from './ExpertCard.vue'
import type { Expert, ExpertCategory } from '@/types/expert'

const expertStore = useExpertStore()
const toastStore = useToastStore()

function handleAddExpert(expert: Expert) {
  if (expert.added) {
    toastStore.info(`「${expert.name}」已在项目中`)
    return
  }
  expertStore.addExpertToProject(expert.id)
  toastStore.success(`已将「${expert.name}」添加到项目`)
}

function handleTabClick(category: ExpertCategory) {
  expertStore.setActiveCategory(category)
}
</script>

<template>
  <div
    class="mx-7 mb-6 rounded-xl border border-gray-600 overflow-hidden flex flex-col min-h-[420px]"
  >
    <!-- Header -->
    <div class="px-6 py-5 border-b border-gray-700 bg-gray-800/60">
      <div class="flex justify-between items-start mb-4">
        <div>
          <h2 class="text-lg font-bold text-white mb-1">专家中心</h2>
          <p class="text-xs text-gray-500">按行业分类浏览专家，召唤他们为你服务</p>
        </div>
        <div class="relative w-64">
          <Search class="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-500" />
          <input
            :value="expertStore.searchQuery"
            @input="expertStore.setSearchQuery(($event.target as HTMLInputElement).value)"
            type="text"
            placeholder="搜索专家职称或描述..."
            class="w-full bg-gray-800 border border-gray-700 rounded-lg pl-9 pr-3 py-2 text-sm text-gray-200 outline-none focus:border-purple-500 focus:ring-2 focus:ring-purple-500/20"
          />
        </div>
      </div>

      <!-- Category Tabs -->
      <div class="flex gap-6 overflow-x-auto border-b border-gray-700 pb-0 -mb-px">
        <button
          v-for="cat in expertStore.categories"
          :key="cat"
          @click="handleTabClick(cat)"
          :class="[
            'text-sm py-2 border-b-2 transition-colors whitespace-nowrap bg-transparent border-t-0 border-l-0 border-r-0',
            expertStore.activeCategory === cat
              ? 'text-purple-400 border-purple-500 font-medium'
              : 'text-gray-500 border-transparent hover:text-gray-400',
          ]"
        >
          {{ cat }}
          <span v-if="cat !== '全部'" class="text-xs ml-1 text-gray-600">
            ({{ expertStore.categoryCounts[cat] || 0 }})
          </span>
        </button>
      </div>
    </div>

    <!-- Expert Grid -->
    <div class="flex-1 overflow-y-auto p-5 bg-gray-900/40">
      <div v-if="expertStore.filteredExperts.length === 0" class="text-center py-12 text-gray-500">
        <p>没有找到匹配的专家</p>
      </div>
      <div v-else class="grid grid-cols-4 gap-4 max-lg:grid-cols-3 max-md:grid-cols-2">
        <ExpertCard
          v-for="(expert, i) in expertStore.filteredExperts"
          :key="expert.id"
          :expert="expert"
          @add="handleAddExpert"
          class="animate-fade-in"
          :style="{ animationDelay: `${i * 40}ms` }"
        />
      </div>
    </div>
  </div>
</template>

<style scoped>
@keyframes fade-in {
  from {
    opacity: 0;
    transform: translateY(6px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}
.animate-fade-in {
  animation: fade-in 0.3s ease both;
}
</style>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/agent/ExpertPanel.vue
git commit -m "feat(ui): add expert panel with category tabs and search"
```

---

### 任务 11：Agent 编辑弹窗 AgentEditModal

**文件：**
- 创建：`ui/clawteam-chat/src/components/agent/AgentEditModal.vue`

- [ ] **步骤 1：创建 AgentEditModal.vue**

```vue
<!-- ui/clawteam-chat/src/components/agent/AgentEditModal.vue -->
<script setup lang="ts">
import { ref, watch } from 'vue'
import { X, Check, Trash2 } from 'lucide-vue-next'
import type { AgentConfig, AgentRole } from '@/types/agent'
import { ROLE_LABELS } from '@/types/agent'

const props = defineProps<{
  show: boolean
  agent: AgentConfig | null
}>()

const emit = defineEmits<{
  close: []
  save: [agent: AgentConfig]
  remove: [id: string]
}>()

const name = ref('')
const role = ref<AgentRole>('assistant')
const prompt = ref('')

watch(
  () => props.agent,
  (agent) => {
    if (agent) {
      name.value = agent.name
      role.value = agent.role
      prompt.value = ''
    }
  },
  { immediate: true }
)

function handleClose() {
  emit('close')
}

function handleSave() {
  if (!props.agent) return
  if (!name.value.trim()) return

  emit('save', {
    ...props.agent,
    name: name.value.trim(),
    role: role.value,
  })
}

function handleRemove() {
  if (!props.agent) return
  emit('remove', props.agent.id)
}
</script>

<template>
  <Teleport to="body">
    <Transition name="modal">
      <div
        v-if="show"
        class="fixed inset-0 bg-black/60 backdrop-blur-sm z-50 flex items-center justify-center"
        @click.self="handleClose"
      >
        <div
          class="bg-gray-800 border border-gray-700 rounded-2xl w-[520px] max-w-[90vw] shadow-2xl"
        >
          <!-- Header -->
          <div
            class="px-6 py-5 flex items-center justify-between border-b border-gray-700"
          >
            <h3 class="text-base font-bold text-white">
              编辑{{ agent ? ROLE_LABELS[agent.role] : 'Agent' }}
            </h3>
            <button
              @click="handleClose"
              class="w-8 h-8 rounded-lg flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200 transition-colors"
            >
              <X class="w-4 h-4" />
            </button>
          </div>

          <!-- Body -->
          <div class="p-6">
            <div class="flex gap-3 mb-4">
              <div class="flex-1">
                <label class="block text-sm font-semibold mb-1.5 text-gray-200">名称</label>
                <input
                  v-model="name"
                  type="text"
                  placeholder="助手名称"
                  maxlength="20"
                  class="w-full bg-gray-800 border border-gray-700 rounded-lg px-3 py-2.5 text-sm text-gray-200 outline-none focus:border-sky-500 focus:ring-2 focus:ring-sky-500/20"
                />
              </div>
              <div class="flex-1">
                <label class="block text-sm font-semibold mb-1.5 text-gray-200">类型</label>
                <select
                  v-model="role"
                  class="w-full bg-gray-800 border border-gray-700 rounded-lg px-3 py-2.5 text-sm text-gray-200 outline-none focus:border-sky-500"
                >
                  <option value="assistant">助手</option>
                  <option value="supervisor">监工</option>
                  <option value="worker">成员</option>
                </select>
              </div>
            </div>

            <div>
              <label class="block text-sm font-semibold mb-1.5 text-gray-200">提示词设置</label>
              <textarea
                v-model="prompt"
                placeholder="定义该 Agent 的角色定位、行为规范和输出要求..."
                class="w-full bg-gray-800 border border-gray-700 rounded-lg px-3 py-3 text-sm text-gray-200 outline-none focus:border-sky-500 focus:ring-2 focus:ring-sky-500/20 resize-vertical min-h-[120px] leading-relaxed"
              />
              <p class="text-xs text-gray-500 mt-1.5 leading-relaxed">
                提示词决定了 Agent 的行为方式和专业表现。修改后会在下次对话时生效。
              </p>
            </div>
          </div>

          <!-- Footer -->
          <div
            class="px-6 py-4 flex justify-end gap-2 border-t border-gray-700"
          >
            <button
              v-if="agent && agent.id !== 'assistant-1'"
              @click="handleRemove"
              class="px-4 py-2 rounded-lg text-sm text-gray-500 hover:bg-red-500/10 hover:text-red-400 transition-colors flex items-center gap-1.5"
            >
              <Trash2 class="w-4 h-4" />
              移除
            </button>
            <div class="flex-1"></div>
            <button
              @click="handleClose"
              class="px-4 py-2 rounded-lg text-sm text-gray-400 bg-gray-700 hover:bg-gray-600 transition-colors"
            >
              取消
            </button>
            <button
              @click="handleSave"
              class="px-4 py-2 rounded-lg text-sm font-medium bg-sky-500 text-white hover:bg-sky-400 transition-colors flex items-center gap-1.5"
            >
              <Check class="w-4 h-4" />
              保存
            </button>
          </div>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.modal-enter-active,
.modal-leave-active {
  transition: opacity 0.2s ease;
}
.modal-enter-active .bg-gray-800,
.modal-leave-active .bg-gray-800 {
  transition: transform 0.25s ease;
}
.modal-enter-from,
.modal-leave-to {
  opacity: 0;
}
.modal-enter-from .bg-gray-800,
.modal-leave-to .bg-gray-800 {
  transform: translateY(10px) scale(0.98);
}
</style>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/agent/AgentEditModal.vue
git commit -m "feat(ui): add agent edit modal component"
```

---

### 任务 12：员工管理视图 AgentView

**文件：**
- 创建：`ui/clawteam-chat/src/views/AgentsView.vue`

- [ ] **步骤 1：创建 AgentsView.vue**

```vue
<!-- ui/clawteam-chat/src/views/AgentsView.vue -->
<script setup lang="ts">
import { computed, ref } from 'vue'
import { UserPlus, SortAsc, Plus, PenTool } from 'lucide-vue-next'
import { useAgentStore } from '@/stores/agent'
import { useToastStore } from '@/stores/toast'
import AgentEditModal from '@/components/agent/AgentEditModal.vue'
import ExpertPanel from '@/components/agent/ExpertPanel.vue'
import type { AgentConfig, AgentRole } from '@/types/agent'

const agentStore = useAgentStore()
const toastStore = useToastStore()

const editingAgent = ref<AgentConfig | null>(null)
const showModal = ref(false)

const roleColors: Record<AgentRole, string> = {
  assistant: 'bg-blue-500',
  worker: 'bg-emerald-500',
  supervisor: 'bg-orange-500',
}

const groupedAgents = computed(() => {
  const groups: Record<AgentRole, AgentConfig[]> = {
    assistant: [],
    worker: [],
    supervisor: [],
  }
  for (const id of agentStore.agentList) {
    const agent = agentStore.agents[id]?.config
    if (agent) {
      groups[agent.role].push(agent)
    }
  }
  return groups
})

function openEditModal(agent: AgentConfig) {
  editingAgent.value = agent
  showModal.value = true
}

function closeModal() {
  showModal.value = false
  editingAgent.value = null
}

function handleSave(agent: AgentConfig) {
  agentStore.updateAgent(agent.id, agent)
  toastStore.success(`「${agent.name}」已保存`)
  closeModal()
}

function handleRemove(id: string) {
  const agent = agentStore.agents[id]?.config
  if (agent) {
    agentStore.removeAgent(id)
    toastStore.success(`已移除「${agent.name}」`)
  }
  closeModal()
}
</script>

<template>
  <div class="h-full flex flex-col overflow-hidden">
    <!-- Top Bar -->
    <div class="px-7 py-4 flex justify-between items-center border-b border-gray-700 bg-gray-800">
      <h2 class="text-xl font-bold text-white">员工管理</h2>
      <div class="flex items-center gap-4">
        <div class="flex items-center gap-1.5 text-sm text-gray-500">
          排序
          <span
            class="bg-gray-800 border border-gray-700 px-2.5 py-0.5 rounded-md text-xs text-gray-300 cursor-pointer hover:bg-gray-700"
          >
            默认
          </span>
        </div>
        <button
          class="flex items-center gap-1.5 px-3 py-2 rounded-lg text-sm text-gray-400 border border-gray-700 hover:bg-gray-700 hover:text-gray-200 transition-colors"
        >
          <UserPlus class="w-4 h-4" />
          添加员工
        </button>
      </div>
    </div>

    <!-- Main Content -->
    <div class="flex-1 overflow-y-auto">
      <div class="grid grid-cols-3 gap-5 px-7 py-6 max-lg:grid-cols-1">
        <!-- Assistants Column -->
        <div>
          <div class="flex items-center justify-between mb-3">
            <div class="flex items-center gap-2 text-sm font-semibold text-gray-500">
              <span class="w-2 h-2 rounded-full bg-blue-500"></span>
              助手
            </div>
            <span class="text-xs text-gray-600">{{ groupedAgents.assistant.length }}</span>
          </div>

          <div v-if="groupedAgents.assistant.length === 0" class="py-8 border border-dashed border-gray-700 rounded-xl flex flex-col items-center justify-center gap-2 min-h-[120px]">
            <span class="text-gray-600 text-2xl">😐</span>
            <p class="text-xs text-gray-600">暂无助手</p>
          </div>

          <div v-else class="space-y-3">
            <div
              v-for="agent in groupedAgents.assistant"
              :key="agent.id"
              class="p-3.5 rounded-xl bg-gray-800 border border-gray-700 hover:border-gray-600 transition-colors group"
            >
              <div class="flex items-center justify-between">
                <div class="flex items-center gap-3">
                  <div
                    :class="['w-10 h-10 rounded-full flex items-center justify-center text-white font-bold text-sm relative', roleColors[agent.role]]"
                  >
                    {{ agent.name.charAt(0) }}
                    <div class="absolute -bottom-0.5 -right-0.5 w-2.5 h-2.5 bg-green-500 border-2 border-gray-800 rounded-full"></div>
                  </div>
                  <div>
                    <div class="text-sm font-semibold text-gray-200">{{ agent.name }}</div>
                    <div class="text-xs text-gray-500">已激活</div>
                  </div>
                </div>
                <div class="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                  <button class="w-7 h-7 rounded flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200">
                    <span class="text-sm">💬</span>
                  </button>
                  <button @click="openEditModal(agent)" class="w-7 h-7 rounded flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200">
                    <span class="text-sm">⚙️</span>
                  </button>
                </div>
              </div>
            </div>
          </div>
        </div>

        <!-- Supervisors Column -->
        <div>
          <div class="flex items-center justify-between mb-3">
            <div class="flex items-center gap-2 text-sm font-semibold text-gray-500">
              <span class="w-2 h-2 rounded-full bg-orange-500"></span>
              监工
            </div>
            <span class="text-xs text-gray-600">{{ groupedAgents.supervisor.length }}</span>
          </div>

          <div v-if="groupedAgents.supervisor.length === 0" class="py-8 border border-dashed border-gray-700 rounded-xl flex flex-col items-center justify-center gap-2 min-h-[120px]">
            <span class="text-gray-600 text-2xl">🛡️</span>
            <p class="text-xs text-gray-600">暂无监工</p>
          </div>

          <div v-else class="space-y-3">
            <div
              v-for="agent in groupedAgents.supervisor"
              :key="agent.id"
              class="p-3.5 rounded-xl bg-gray-800 border border-gray-700 hover:border-gray-600 transition-colors group"
            >
              <div class="flex items-center justify-between">
                <div class="flex items-center gap-3">
                  <div :class="['w-10 h-10 rounded-full flex items-center justify-center text-white font-bold text-sm', roleColors[agent.role]]">
                    {{ agent.name.charAt(0) }}
                  </div>
                  <div>
                    <div class="text-sm font-semibold text-gray-200">{{ agent.name }}</div>
                    <div class="text-xs text-gray-500">已激活</div>
                  </div>
                </div>
                <div class="flex gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                  <button @click="openEditModal(agent)" class="w-7 h-7 rounded flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200">
                    <span class="text-sm">⚙️</span>
                  </button>
                </div>
              </div>
            </div>
          </div>
        </div>

        <!-- Custom Experts Column -->
        <div>
          <div class="flex items-center justify-between mb-3">
            <div class="flex items-center gap-2 text-sm font-semibold text-gray-500">
              <span class="w-2 h-2 rounded-full bg-purple-500"></span>
              自定义专家
            </div>
          </div>

          <div
            class="p-5 rounded-xl border border-dashed border-gray-700 flex flex-col items-center justify-center gap-2.5 cursor-pointer hover:border-purple-500 hover:bg-purple-500/10 transition-colors min-h-[140px]"
          >
            <div class="w-11 h-11 rounded-full bg-gray-800 flex items-center justify-center text-gray-500 group-hover:bg-purple-500/20 group-hover:text-purple-400">
              <PenTool class="w-5 h-5" />
            </div>
            <span class="text-xs text-gray-500">写入提示词创建专家</span>
          </div>
        </div>
      </div>

      <!-- Expert Panel -->
      <ExpertPanel />
    </div>

    <!-- Edit Modal -->
    <AgentEditModal
      :show="showModal"
      :agent="editingAgent"
      @close="closeModal"
      @save="handleSave"
      @remove="handleRemove"
    />
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/views/AgentsView.vue
git commit -m "feat(ui): add agents view with three-column layout"
```

---

### 任务 13：技能卡片组件 SkillCard

**文件：**
- 创建：`ui/clawteam-chat/src/components/skill/SkillCard.vue`

- [ ] **步骤 1：创建 SkillCard.vue**

```vue
<!-- ui/clawteam-chat/src/components/skill/SkillCard.vue -->
<script setup lang="ts">
import { Star, RotateCw } from 'lucide-vue-next'
import type { Skill } from '@/types/skill'

defineProps<{ skill: Skill }>()
defineEmits<{ install: [skill: Skill] }>()
</script>

<template>
  <div
    class="bg-gray-800 border border-gray-700 rounded-xl p-5 transition-all hover:border-gray-600 hover:bg-gray-700/50 hover:-translate-y-0.5 hover:shadow-lg flex gap-4 items-start"
  >
    <!-- Icon -->
    <div
      :style="{ background: skill.color }"
      class="w-12 h-12 min-w-12 rounded-xl flex items-center justify-center text-white"
    >
      <span class="text-xl">{{ skill.icon }}</span>
    </div>

    <!-- Info -->
    <div class="flex-1 min-w-0">
      <div class="font-semibold text-sm text-gray-200 mb-1">{{ skill.name }}</div>
      <p class="text-xs text-gray-400 leading-relaxed mb-2.5 line-clamp-2">
        {{ skill.description }}
      </p>
      <div class="flex items-center gap-3 flex-wrap">
        <span
          v-if="skill.tags.includes('official')"
          class="text-xs px-2 py-0.5 rounded bg-emerald-500/10 text-emerald-400"
        >
          官方默认
        </span>
        <div v-if="skill.rating" class="flex items-center gap-1 text-xs text-amber-500">
          <Star class="w-3.5 h-3.5 fill-current" />
          <span class="text-gray-400">{{ skill.rating }}</span>
        </div>
      </div>
    </div>

    <!-- Actions -->
    <div class="flex items-center gap-2 ml-auto">
      <button
        @click="$emit('install', skill)"
        class="px-3 py-1.5 rounded-lg text-xs border border-gray-600 text-gray-400 hover:bg-gray-600 hover:text-gray-200 transition-colors flex items-center gap-1.5"
      >
        <RotateCw class="w-3.5 h-3.5" />
        {{ skill.installed ? '重新安装' : '安装' }}
      </button>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/skill/SkillCard.vue
git commit -m "feat(ui): add skill card component"
```

---

### 任务 14：技能商店视图 SkillView

**文件：**
- 创建：`ui/clawteam-chat/src/views/SkillsView.vue`

- [ ] **步骤 1：创建 SkillsView.vue**

```vue
<!-- ui/clawteam-chat/src/views/SkillsView.vue -->
<script setup lang="ts">
import { ref, computed } from 'vue'
import { Search, ChevronLeft, ChevronRight } from 'lucide-vue-next'
import { useSkillStore } from '@/stores/skill'
import { useToastStore } from '@/stores/toast'
import SkillCard from '@/components/skill/SkillCard.vue'
import type { Skill } from '@/types/skill'

const skillStore = useSkillStore()
const toastStore = useToastStore()

const activeTab = ref('store')
const activeFilter = ref('all')
const searchQuery = ref('')
const currentPage = ref(1)

const tabs = [
  { id: 'store', label: '商店' },
  { id: 'mine', label: '我的技能' },
  { id: 'project', label: '项目技能' },
]

const filters = [
  { id: 'all', label: '全部技能' },
  { id: 'quality', label: '代码质量' },
  { id: 'docs', label: '文档处理' },
]

const filteredSkills = computed(() => {
  let skills = skillStore.skills
  if (searchQuery.value) {
    const q = searchQuery.value.toLowerCase()
    skills = skills.filter(
      (s) =>
        s.name.toLowerCase().includes(q) || s.description.toLowerCase().includes(q)
    )
  }
  return skills
})

function handleInstall(skill: Skill) {
  toastStore.success(`「${skill.name}」安装成功`)
}
</script>

<template>
  <div class="h-full overflow-y-auto p-6">
    <div class="flex items-center gap-3 mb-2">
      <h2 class="text-lg font-bold text-white">技能</h2>
      <p class="text-xs text-gray-500">浏览、安装和管理项目技能</p>
    </div>

    <!-- Search and Tabs -->
    <div class="flex items-center gap-3 mb-4 flex-wrap">
      <div class="relative max-w-[360px]">
        <Search class="absolute left-3.5 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-500" />
        <input
          v-model="searchQuery"
          type="text"
          placeholder="搜索技能..."
          class="w-full bg-gray-800 border border-gray-700 rounded-xl pl-10 pr-4 py-2.5 text-sm text-gray-200 outline-none focus:border-sky-500 focus:ring-2 focus:ring-sky-500/20"
        />
      </div>
      <div class="flex gap-0.5 bg-gray-800 rounded-lg p-0.5 border border-gray-700">
        <button
          v-for="tab in tabs"
          :key="tab.id"
          @click="activeTab = tab.id"
          :class="[
            'px-4 py-1.5 text-sm rounded-md transition-colors',
            activeTab === tab.id
              ? 'bg-gray-700 text-gray-200 font-medium shadow'
              : 'text-gray-500 hover:text-gray-300',
          ]"
        >
          {{ tab.label }}
        </button>
      </div>
    </div>

    <!-- Filters -->
    <div class="flex gap-1.5 mb-5 flex-wrap">
      <button
        v-for="filter in filters"
        :key="filter.id"
        @click="activeFilter = filter.id"
        :class="[
          'px-3.5 py-1.5 rounded-full text-xs border transition-colors',
          activeFilter === filter.id
            ? 'bg-sky-500/20 border-sky-500 text-sky-400'
            : 'border-gray-700 text-gray-500 hover:border-gray-600 hover:text-gray-300',
        ]"
      >
        {{ filter.label }}
      </button>
    </div>

    <!-- Skill Grid -->
    <div class="space-y-3">
      <SkillCard
        v-for="skill in filteredSkills"
        :key="skill.id"
        :skill="skill"
        @install="handleInstall"
      />
    </div>

    <!-- Pagination -->
    <div class="flex items-center justify-center gap-1 mt-6 pt-4 border-t border-gray-700">
      <button class="w-8 h-8 rounded-md border border-gray-700 flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200 transition-colors" disabled>
        <ChevronLeft class="w-3 h-3" />
      </button>
      <button class="w-8 h-8 rounded-md border border-sky-500 bg-sky-500 text-gray-900 font-semibold text-sm">
        1
      </button>
      <button class="w-8 h-8 rounded-md border border-gray-700 flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200 transition-colors">
        2
      </button>
      <button class="w-8 h-8 rounded-md border border-gray-700 flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200 transition-colors">
        <ChevronRight class="w-3 h-3" />
      </button>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/views/SkillsView.vue
git commit -m "feat(ui): add skills view with search and filters"
```

---

### 任务 15：CLI 设置组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/settings/CliSettings.vue`

- [ ] **步骤 1：创建 CliSettings.vue**

```vue
<!-- ui/clawteam-chat/src/components/settings/CliSettings.vue -->
<script setup lang="ts">
import { Plus, CircleCheck, FolderOpen, RefreshCw, List } from 'lucide-vue-next'
import { useSettingsStore } from '@/stores/settings'
import { useToastStore } from '@/stores/toast'
import type { CliConfig } from '@/types/settings'

const settingsStore = useSettingsStore()
const toastStore = useToastStore()

function handleSelectCli(id: string) {
  settingsStore.selectCli(id)
  const tool = settingsStore.cliTools.find((t) => t.id === id)
  toastStore.success(`已选择「${tool?.name || id}」作为默认CLI`)
}

function getIconClass(tool: CliConfig) {
  if (settingsStore.selectedCli === tool.id) return 'bg-sky-500/20 text-sky-400 border-sky-500/40'
  if (tool.installed) return 'bg-amber-500/15 text-amber-400 border-amber-500/30'
  return 'bg-gray-800 text-gray-500'
}
</script>

<template>
  <div>
    <div class="mb-8">
      <h3 class="text-base font-bold text-gray-200 mb-2">CLI配置</h3>
      <p class="text-xs text-gray-500">配置终端调用CLI方式，选择默认的AI助手工具</p>
    </div>

    <!-- CLI Grid -->
    <div class="grid grid-cols-4 gap-3 mb-6 max-xl:grid-cols-3 max-lg:grid-cols-2 max-sm:grid-cols-1">
      <div
        v-for="tool in settingsStore.cliTools"
        :key="tool.id"
        @click="handleSelectCli(tool.id)"
        :class="[
          'bg-gray-800 border rounded-xl p-4 flex flex-col items-center text-center gap-1.5 cursor-pointer relative transition-all hover:bg-gray-700 hover:border-gray-600',
          settingsStore.selectedCli === tool.id ? 'border-sky-500 bg-sky-500/10' : 'border-gray-700',
          tool.custom ? 'border-dashed' : '',
        ]"
      >
        <CircleCheck
          v-if="settingsStore.selectedCli === tool.id"
          class="absolute top-2 right-2 w-3 h-3 text-sky-400"
        />
        <div :class="['w-12 h-12 rounded-xl flex items-center justify-center text-lg border', getIconClass(tool)]">
          ⚡
        </div>
        <div class="font-semibold text-sm text-gray-200">{{ tool.name }}</div>
        <div class="text-xs text-gray-500 font-mono max-w-full truncate">{{ tool.command }}</div>
        <div class="text-xs text-gray-500">{{ tool.custom ? '自定义终端' : '默认终端' }}</div>
        <div v-if="!tool.custom" :class="['text-xs', tool.installed ? 'text-amber-400' : 'text-red-400']">
          {{ tool.installed ? '已安装' : '未安装' }}
        </div>
      </div>

      <!-- Add CLI Card -->
      <div
        class="border border-dashed border-gray-700 rounded-xl p-4 flex flex-col items-center justify-center gap-2 cursor-pointer hover:border-sky-500 hover:bg-sky-500/10 transition-colors min-h-[130px]"
      >
        <div class="w-12 h-12 rounded-xl bg-gray-800 flex items-center justify-center">
          <Plus class="w-5 h-5 text-gray-500" />
        </div>
        <span class="text-sm text-gray-500">自定义 CLI</span>
      </div>
    </div>

    <div class="h-px bg-gray-700 my-6"></div>

    <!-- Shell Selection -->
    <div class="mb-8">
      <div class="flex justify-between items-center mb-3">
        <span class="text-sm text-gray-500">选择终端</span>
        <div class="flex items-center gap-4">
          <div class="flex items-center gap-1.5 text-xs text-gray-500">
            编码
            <select class="bg-gray-800 border border-gray-700 rounded-md px-2 py-1 text-xs text-gray-400 outline-none">
              <option>自动 (推荐)</option>
              <option>UTF-8</option>
              <option>GBK</option>
            </select>
          </div>
          <button class="text-xs text-sky-400 flex items-center gap-1 hover:text-sky-300">
            <RefreshCw class="w-3 h-3" />
            刷新列表
          </button>
        </div>
      </div>

      <div class="grid grid-cols-2 gap-3 max-sm:grid-cols-1">
        <div
          v-for="shell in settingsStore.shells"
          :key="shell.id"
          @click="settingsStore.selectShell(shell.id)"
          :class="[
            'bg-gray-800 border rounded-lg px-4 py-3.5 flex justify-between items-center cursor-pointer transition-all hover:bg-gray-700 hover:border-gray-600 relative',
            settingsStore.selectedShell === shell.id ? 'border-sky-500 bg-sky-500/10' : 'border-gray-700',
          ]"
        >
          <CircleCheck
            v-if="settingsStore.selectedShell === shell.id"
            class="absolute right-8 text-sky-400 w-3 h-3"
          />
          <div>
            <div class="font-semibold text-sm text-gray-200 mb-0.5">{{ shell.name }}</div>
            <div class="text-xs text-gray-500 font-mono max-w-[220px] truncate">{{ shell.path }}</div>
          </div>
        </div>

        <!-- Add Shell Card -->
        <div
          class="border border-dashed border-gray-700 rounded-lg py-5 flex flex-col items-center justify-center gap-2 cursor-pointer hover:border-sky-500 hover:bg-sky-500/10 transition-colors"
        >
          <div class="w-9 h-9 rounded-lg bg-gray-800 flex items-center justify-center">
            <Plus class="w-4 h-4 text-gray-500" />
          </div>
          <span class="text-sm text-gray-500">自定义终端</span>
        </div>
      </div>
    </div>

    <!-- PATH Pool -->
    <div>
      <h3 class="text-base font-bold text-gray-200 mb-2">兜底 PATH 路径池</h3>
      <p class="text-xs text-gray-500 mb-4">仅在命令检测失败或启动终端成员时，用于兜底查找与 PATH 注入。</p>

      <div class="flex gap-2 mb-3">
        <input
          type="text"
          placeholder="每行输入一条路径 (目录或可执行文件路径)"
          class="flex-1 bg-gray-800 border border-gray-700 rounded-lg px-3.5 py-2.5 text-sm text-gray-200 outline-none focus:border-sky-500 focus:ring-2 focus:ring-sky-500/20"
        />
        <button class="px-4 py-2.5 rounded-lg text-sm text-gray-400 border border-gray-700 hover:bg-gray-700 hover:text-gray-200 transition-colors flex items-center gap-1.5">
          <FolderOpen class="w-4 h-4" />
          选择文件夹
        </button>
        <button class="px-4 py-2.5 rounded-lg text-sm bg-sky-500/20 border border-sky-500/40 text-sky-400 hover:bg-sky-500/30 transition-colors flex items-center gap-1.5">
          <Plus class="w-4 h-4" />
          添加
        </button>
      </div>

      <div class="flex justify-between items-center pt-2.5 border-t border-gray-800">
        <span class="text-xs text-gray-500">{{ settingsStore.pathPool.length }} 条路径</span>
        <button class="text-xs text-gray-500 flex items-center gap-1 hover:text-gray-300">
          <List class="w-3 h-3" />
          管理列表
        </button>
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/settings/CliSettings.vue
git commit -m "feat(ui): add CLI settings component"
```

---

### 任务 16：外观设置组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/settings/AppearanceSettings.vue`

- [ ] **步骤 1：创建 AppearanceSettings.vue**

```vue
<!-- ui/clawteam-chat/src/components/settings/AppearanceSettings.vue -->
<script setup lang="ts">
import { useSettingsStore } from '@/stores/settings'
import { useToastStore } from '@/stores/toast'
import type { Theme } from '@/types/settings'

const settingsStore = useSettingsStore()
const toastStore = useToastStore()

const themes: { id: Theme; label: string; preview: string }[] = [
  { id: 'dark', label: '深色', preview: 'linear-gradient(135deg,#0d1117,#1c2333)' },
  { id: 'light', label: '浅色', preview: 'linear-gradient(135deg,#f0f2f5,#fff)' },
  { id: 'system', label: '系统', preview: 'linear-gradient(135deg,#0d1117 50%,#f0f2f5 50%)' },
]

function handleThemeChange(theme: Theme) {
  settingsStore.setTheme(theme)
  toastStore.success(`已切换到「${themes.find(t => t.id === theme)?.label}」主题`)
}
</script>

<template>
  <div>
    <h3 class="text-base font-bold text-gray-200 mb-4 pb-2.5 border-b border-gray-700">外观</h3>
    
    <div class="py-3 border-b border-gray-800/50">
      <div class="font-medium text-sm text-gray-200 mb-4">主题</div>
      <div class="flex gap-2">
        <button
          v-for="theme in themes"
          :key="theme.id"
          @click="handleThemeChange(theme.id)"
          :class="[
            'w-20 py-3 rounded-lg border-2 transition-colors text-center',
            settingsStore.theme === theme.id
              ? 'border-sky-500 bg-sky-500/10'
              : 'border-gray-700 bg-gray-800 hover:border-gray-600',
          ]"
        >
          <div
            :style="{ background: theme.preview }"
            class="w-9 h-7 rounded-md mx-auto mb-2 border border-white/10"
          />
          <span
            :class="[
              'text-xs',
              settingsStore.theme === theme.id ? 'text-sky-400' : 'text-gray-500',
            ]"
          >
            {{ theme.label }}
          </span>
        </button>
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/settings/AppearanceSettings.vue
git commit -m "feat(ui): add appearance settings component"
```

---

### 任务 17：账号设置组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/settings/AccountSettings.vue`

- [ ] **步骤 1：创建 AccountSettings.vue**

```vue
<!-- ui/clawteam-chat/src/components/settings/AccountSettings.vue -->
<script setup lang="ts">
</script>

<template>
  <div>
    <h3 class="text-base font-bold text-gray-200 mb-4 pb-2.5 border-b border-gray-700">我的账号</h3>
    
    <div class="divide-y divide-gray-800/50">
      <div class="py-3 flex items-center justify-between">
        <div>
          <div class="font-medium text-sm text-gray-200">名称</div>
        </div>
        <div class="text-sm text-gray-500">群主</div>
      </div>
      
      <div class="py-3 flex items-center justify-between">
        <div>
          <div class="font-medium text-sm text-gray-200">邮箱</div>
        </div>
        <div class="text-sm text-gray-500">name@example.com</div>
      </div>
      
      <div class="py-3 flex items-center justify-between">
        <div>
          <div class="font-medium text-sm text-gray-200">时区</div>
        </div>
        <select class="bg-gray-800 border border-gray-700 rounded-lg px-2.5 py-1.5 text-sm text-gray-400 outline-none cursor-pointer">
          <option selected>UTC 协调世界时</option>
          <option>Asia/Shanghai (UTC+8)</option>
        </select>
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/settings/AccountSettings.vue
git commit -m "feat(ui): add account settings component"
```

---

### 任务 18：设置视图 SettingsView

**文件：**
- 创建：`ui/clawteam-chat/src/views/SettingsView.vue`

- [ ] **步骤 1：创建 SettingsView.vue**

```vue
<!-- ui/clawteam-chat/src/views/SettingsView.vue -->
<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import AccountSettings from '@/components/settings/AccountSettings.vue'
import AppearanceSettings from '@/components/settings/AppearanceSettings.vue'
import CliSettings from '@/components/settings/CliSettings.vue'

const route = useRoute()

const activeTab = computed(() => {
  const parts = route.path.split('/')
  return parts[2] || 'account'
})
</script>

<template>
  <div class="h-full flex">
    <!-- Settings Body -->
    <div class="flex-1 overflow-y-auto p-8">
      <AccountSettings v-if="activeTab === 'account'" />
      <AppearanceSettings v-else-if="activeTab === 'appearance'" />
      <CliSettings v-else-if="activeTab === 'cli'" />
    </div>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/views/SettingsView.vue
git commit -m "feat(ui): add settings view with sub-tabs"
```

---

### 任务 19：路由配置更新

**文件：**
- 修改：`ui/clawteam-chat/src/router/index.ts`

- [ ] **步骤 1：更新路由配置**

```typescript
// ui/clawteam-chat/src/router/index.ts
import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'
import AppLayout from '@/components/layout/AppLayout.vue'
import ChatView from '@/views/ChatView.vue'
import AgentsView from '@/views/AgentsView.vue'
import SkillsView from '@/views/SkillsView.vue'
import SettingsView from '@/views/SettingsView.vue'

const routes: RouteRecordRaw[] = [
  {
    path: '/',
    component: AppLayout,
    children: [
      { path: '', redirect: '/chat' },
      { path: 'chat', name: 'chat', component: ChatView },
      { path: 'agents', name: 'agents', component: AgentsView },
      { path: 'skills', name: 'skills', component: SkillsView },
      {
        path: 'settings',
        name: 'settings',
        component: SettingsView,
        children: [
          { path: '', redirect: 'account' },
          { path: 'account', name: 'settings-account', component: SettingsView },
          { path: 'appearance', name: 'settings-appearance', component: SettingsView },
          { path: 'cli', name: 'settings-cli', component: SettingsView },
        ],
      },
    ],
  },
]

export const router = createRouter({
  history: createWebHistory(),
  routes,
})
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/router/index.ts
git commit -m "feat(ui): update router with four main views"
```

---

### 任务 20：App.vue 重构

**文件：**
- 修改：`ui/clawteam-chat/src/App.vue`

- [ ] **步骤 1：重构 App.vue**

```vue
<!-- ui/clawteam-chat/src/App.vue -->
<script setup lang="ts">
import { onMounted } from 'vue'
import { RouterView } from 'vue-router'
import { useWorkspaceStore } from '@/stores/workspace'
import { useProjectStore } from '@/stores/project'
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
  <RouterView />
</template>
```

- [ ] **步骤 2：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

- [ ] **步骤 3：运行开发服务器验证**

```bash
cd ui/clawteam-chat && pnpm dev
```

预期：应用启动成功，四视图可正常切换

- [ ] **步骤 4：Commit**

```bash
git add ui/clawteam-chat/src/App.vue
git commit -m "refactor(ui): simplify App.vue to use RouterView"
```

---

### 任务 21：全局样式更新

**文件：**
- 修改：`ui/clawteam-chat/src/index.css`

- [ ] **步骤 1：更新 index.css 添加深色主题基础样式**

```css
/* ui/clawteam-chat/src/index.css */
@import "tailwindcss";

/* 深色主题基础样式 */
body {
  background-color: #0B0F19;
  color: #e5e7eb;
  font-family: 'Inter', system-ui, sans-serif;
}

/* 滚动条样式 */
::-webkit-scrollbar {
  width: 8px;
  height: 8px;
}
::-webkit-scrollbar-track {
  background: #111827;
}
::-webkit-scrollbar-thumb {
  background: #374151;
  border-radius: 4px;
}
::-webkit-scrollbar-thumb:hover {
  background: #4b5563;
}

/* 隐藏滚动条 */
.no-scrollbar::-webkit-scrollbar {
  display: none;
}
.no-scrollbar {
  -ms-overflow-style: none;
  scrollbar-width: none;
}
```

- [ ] **步骤 2：运行开发服务器验证样式**

```bash
cd ui/clawteam-chat && pnpm dev
```

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/index.css
git commit -m "style(ui): add dark theme base styles"
```

---

### 任务 22：最终验证和构建

**文件：**
- 无新建文件

- [ ] **步骤 1：运行完整类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

预期：无错误

- [ ] **步骤 2：运行构建**

```bash
cd ui/clawteam-chat && pnpm build
```

预期：构建成功

- [ ] **步骤 3：运行测试**

```bash
cd ui/clawteam-chat && pnpm test
```

预期：所有测试通过

- [ ] **步骤 4：最终提交**

```bash
git add -A
git commit -m "feat(ui): complete frontend four-view implementation"
```

---

## 验收标准

- [ ] 四视图路由正常工作（聊天、员工管理、技能商店、设置）
- [ ] 三栏布局正确显示（全局导航 64px | 上下文侧边栏 256px | 主内容区）
- [ ] 员工管理视图三列布局（助手、监工、自定义专家）
- [ ] 专家中心面板可正常搜索和筛选
- [ ] 技能商店视图可正常搜索和筛选
- [ ] 设置视图三个子页面可正常切换
- [ ] CLI 配置可选择工具和终端
- [ ] Toast 通知正常显示
- [ ] 深色主题正确应用
- [ ] 类型检查通过
- [ ] 构建成功
- [ ] 测试通过
