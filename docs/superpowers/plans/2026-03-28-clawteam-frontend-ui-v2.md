# ClawTeam 前端 UI 组件完善计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 基于规格设计 §6 UI 扩展设计和 Golutra UI 参考，完善 clawteam-chat 前端组件，增加 Agent 配置弹窗、Skill 商店、Feishu/Weixin 渠道配置、审计日志面板，并优化主界面三栏布局。

**架构：** 在现有 Vue 3.5 + Pinia 基础上，扩展 AgentPanel 增加 Agent 配置弹窗、新增 SkillStore 组件、新增 ChannelConfig 渠道配置组件、增强 ChatInterface 为三栏自适应布局（Agent面板 + 聊天 + 输出面板）、增强 ChatHeader 增加设置入口。

**技术栈：** Vue 3.5.12, Pinia 3.0.4, Tailwind CSS 4.1.17, lucide-vue-next, Vite 7.2, Vitest 3.0

---

## 当前状态

clawteam-chat 已完成 Vue 3.5 迁移，具备以下基础：
- Stores: `chat.ts`, `agent.ts`, `skill.ts`
- Components: `ChatInterface.vue`, `AgentPanel.vue`, `AgentCard.vue`, `MessageList.vue`, `ChatInput.vue`, `OutputPanel.vue`
- UI 基础: `Button.vue`, `Card.vue`, `CardHeader.vue`, `CardTitle.vue`, `CardContent.vue`, `Input.vue`
- Composables: `useGateway.ts`, `useWebSocket.ts`
- Types: `agent.ts`, `message.ts`, `skill.ts`, `audit.ts`, `session.ts`

**缺失功能：**
- Agent 配置弹窗（创建/编辑 Agent）
- Skill 商店界面（Skill 卡片 + 执行弹窗）
- Feishu/Weixin 渠道配置面板
- 审计日志面板
- 增强的顶部导航栏
- CLI 工具选择器组件

---

## 文件结构

```
ui/clawteam-chat/src/
├── types/
│   ├── agent.ts                  # 【修改】增加 CLI 工具类型常量
│   ├── channel.ts                # 【新增】渠道类型定义
│   ├── skill.ts                  # 现有，保留
│   ├── message.ts                # 现有，保留
│   ├── audit.ts                  # 现有，保留
│   └── session.ts                # 现有，保留
├── stores/
│   ├── agent.ts                  # 【修改】增加 addAgent/updateAgent/removeAgent actions
│   ├── channel.ts                # 【新增】渠道配置状态
│   ├── audit.ts                  # 【新增】审计日志状态
│   ├── skill.ts                  # 【修改】增加 executeSkill action
│   └── chat.ts                   # 现有，保留
├── components/
│   ├── ChatInterface.vue         # 【修改】增强三栏布局 + Header
│   ├── AgentPanel.vue            # 【修改】增加添加按钮触发配置弹窗
│   ├── AgentCard.vue             # 【修改】增加右键菜单（启动/停止/配置）
│   ├── ChatInput.vue             # 现有，保留
│   ├── MessageList.vue           # 现有，保留
│   ├── OutputPanel.vue           # 【修改】增强为可搜索的输出面板
│   ├── agent/
│   │   └── AgentConfigModal.vue  # 【新增】Agent 配置弹窗
│   ├── skill/
│   │   ├── SkillStore.vue        # 【新增】Skill 商店面板
│   │   ├── SkillCard.vue         # 【新增】Skill 卡片
│   │   └── SkillExecuteModal.vue # 【新增】Skill 执行弹窗
│   ├── channel/
│   │   ├── ChannelConfig.vue     # 【新增】渠道配置面板
│   │   ├── FeishuConfig.vue      # 【新增】飞书渠道配置
│   │   └── WeixinConfig.vue      # 【新增】微信渠道配置
│   ├── audit/
│   │   └── AuditPanel.vue        # 【新增】审计日志面板
│   └── ui/
│       ├── Badge.vue             # 【新增】状态徽章
│       ├── Dialog.vue            # 【新增】对话框
│       ├── Select.vue            # 【新增】选择器
│       ├── Switch.vue            # 【新增】开关
│       ├── Tabs.vue              # 【新增】标签页
│       ├── Textarea.vue          # 【新增】文本域
│       └── index.ts              # 【修改】导出新增组件
├── composables/
│   ├── useGateway.ts             # 【修改】增加渠道和审计 API
│   └── useWebSocket.ts           # 现有，保留
└── views/
    └── ChatView.vue              # 现有，保留
```

---

## 任务 1：基础 UI 组件扩展

**文件：**
- 创建：`ui/clawteam-chat/src/components/ui/Badge.vue`
- 创建：`ui/clawteam-chat/src/components/ui/Dialog.vue`
- 创建：`ui/clawteam-chat/src/components/ui/Select.vue`
- 创建：`ui/clawteam-chat/src/components/ui/Switch.vue`
- 创建：`ui/clawteam-chat/src/components/ui/Tabs.vue`
- 创建：`ui/clawteam-chat/src/components/ui/Textarea.vue`
- 修改：`ui/clawteam-chat/src/components/ui/index.ts`

- [ ] **步骤 1：创建 Badge.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'

interface Props {
  variant?: 'default' | 'secondary' | 'destructive' | 'outline' | 'success' | 'warning' | 'working'
}

const props = withDefaults(defineProps<Props>(), {
  variant: 'default',
})

const variantClasses: Record<string, string> = {
  default: 'bg-gray-100 text-gray-800',
  secondary: 'bg-gray-200 text-gray-600',
  destructive: 'bg-red-100 text-red-700',
  outline: 'border border-gray-300 text-gray-700',
  success: 'bg-green-100 text-green-700',
  warning: 'bg-yellow-100 text-yellow-700',
  working: 'bg-blue-100 text-blue-700',
}

const classes = computed(() =>
  cn(
    'inline-flex items-center rounded-full px-2.5 py-0.5 text-xs font-medium',
    variantClasses[props.variant]
  )
)
</script>

<template>
  <span :class="classes">
    <slot />
  </span>
</template>
```

- [ ] **步骤 2：创建 Dialog.vue**

```vue
<script setup lang="ts">
import { watch } from 'vue'
import { X } from 'lucide-vue-next'
import { cn } from '@/lib/utils'

interface Props {
  open: boolean
  class?: string
}

const props = defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
}>()

const close = () => emit('update:open', false)

watch(() => props.open, (val) => {
  if (val) {
    document.body.style.overflow = 'hidden'
  } else {
    document.body.style.overflow = ''
  }
})
</script>

<template>
  <Teleport to="body">
    <div v-if="open" class="fixed inset-0 z-50 flex items-center justify-center">
      <div class="fixed inset-0 bg-black/50" @click="close" />
      <div
        :class="cn(
          'relative z-50 w-full max-w-lg rounded-lg bg-white p-6 shadow-lg',
          props.class
        )"
      >
        <button
          class="absolute right-4 top-4 rounded-sm text-gray-400 hover:text-gray-600"
          @click="close"
        >
          <X class="h-4 w-4" />
        </button>
        <slot />
      </div>
    </div>
  </Teleport>
</template>
```

- [ ] **步骤 3：创建 Select.vue**

```vue
<script setup lang="ts">
import { cn } from '@/lib/utils'

interface Props {
  modelValue?: string
  placeholder?: string
  disabled?: boolean
}

withDefaults(defineProps<Props>(), {
  modelValue: '',
  placeholder: '请选择',
  disabled: false,
})

const emit = defineEmits<{
  'update:modelValue': [value: string]
}>()

defineOptions({ inheritAttrs: false })
</script>

<template>
  <select
    :value="modelValue"
    :disabled="disabled"
    :class="cn(
      'flex h-10 w-full rounded-md border border-gray-300 bg-white px-3 py-2 text-sm',
      'focus:outline-none focus:ring-2 focus:ring-blue-500',
      'disabled:cursor-not-allowed disabled:opacity-50',
      $attrs.class as string
    )"
    @change="emit('update:modelValue', ($event.target as HTMLSelectElement).value)"
  >
    <option value="" disabled>{{ placeholder }}</option>
    <slot />
  </select>
</template>
```

- [ ] **步骤 4：创建 Switch.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'

interface Props {
  modelValue?: boolean
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  modelValue: false,
  disabled: false,
})

const emit = defineEmits<{
  'update:modelValue': [value: boolean]
}>()

const toggle = () => {
  if (!props.disabled) emit('update:modelValue', !props.modelValue)
}

const trackClasses = computed(() =>
  cn(
    'relative inline-flex h-6 w-11 shrink-0 cursor-pointer rounded-full border-2 border-transparent transition-colors',
    props.modelValue ? 'bg-blue-600' : 'bg-gray-200',
    props.disabled && 'opacity-50 cursor-not-allowed'
  )
)

const thumbClasses = computed(() =>
  cn(
    'pointer-events-none inline-block h-5 w-5 transform rounded-full bg-white shadow ring-0 transition-transform',
    props.modelValue ? 'translate-x-5' : 'translate-x-0'
  )
)
</script>

<template>
  <button type="button" role="switch" :aria-checked="modelValue" :class="trackClasses" @click="toggle">
    <span :class="thumbClasses" />
  </button>
</template>
```

- [ ] **步骤 5：创建 Tabs.vue**

```vue
<script setup lang="ts">
import { ref, provide, computed } from 'vue'

interface Props {
  defaultValue?: string
  modelValue?: string
}

const props = withDefaults(defineProps<Props>(), {
  defaultValue: '',
  modelValue: undefined,
})

const emit = defineEmits<{
  'update:modelValue': [value: string]
}>()

const internalValue = ref(props.defaultValue)
const activeTab = computed(() => props.modelValue ?? internalValue.value)

const setActiveTab = (value: string) => {
  internalValue.value = value
  emit('update:modelValue', value)
}

provide('tabs-active', activeTab)
provide('tabs-set-active', setActiveTab)
</script>

<template>
  <div>
    <slot />
  </div>
</template>
```

- [ ] **步骤 6：创建 Textarea.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'

interface Props {
  modelValue?: string
  placeholder?: string
  rows?: number
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  modelValue: '',
  placeholder: '',
  rows: 3,
  disabled: false,
})

const emit = defineEmits<{
  'update:modelValue': [value: string]
}>()

const classes = computed(() =>
  cn(
    'flex w-full rounded-md border border-gray-300 bg-white px-3 py-2 text-sm',
    'placeholder:text-gray-400 focus:outline-none focus:ring-2 focus:ring-blue-500',
    'disabled:cursor-not-allowed disabled:opacity-50',
    'min-h-[60px] resize-y'
  )
)
</script>

<template>
  <textarea
    :value="modelValue"
    :rows="rows"
    :placeholder="placeholder"
    :disabled="disabled"
    :class="classes"
    @input="emit('update:modelValue', ($event.target as HTMLTextAreaElement).value)"
  />
</template>
```

- [ ] **步骤 7：更新 ui/index.ts 导出**

```typescript
export { default as Button } from './Button.vue'
export { default as Card } from './Card.vue'
export { default as CardHeader } from './CardHeader.vue'
export { default as CardTitle } from './CardTitle.vue'
export { default as CardContent } from './CardContent.vue'
export { default as Input } from './Input.vue'
export { default as Badge } from './Badge.vue'
export { default as Dialog } from './Dialog.vue'
export { default as Select } from './Select.vue'
export { default as Switch } from './Switch.vue'
export { default as Tabs } from './Tabs.vue'
export { default as Textarea } from './Textarea.vue'
```

- [ ] **步骤 8：运行类型检查**

运行：`pnpm check`（在 `ui/clawteam-chat` 目录下）
预期：PASS

- [ ] **步骤 9：Commit**

```bash
git add ui/clawteam-chat/src/components/ui/
git commit -m "feat(ui): add Badge, Dialog, Select, Switch, Tabs, Textarea components"
```

---

## 任务 2：类型定义扩展

**文件：**
- 修改：`ui/clawteam-chat/src/types/agent.ts`
- 创建：`ui/clawteam-chat/src/types/channel.ts`

- [ ] **步骤 1：增强 agent.ts - 增加 CLI 工具常量和角色颜色映射**

在 `ui/clawteam-chat/src/types/agent.ts` 文件末尾追加：

```typescript
export const CLI_TOOLS = [
  { value: 'claude', label: 'Claude Code' },
  { value: 'codex', label: 'Codex' },
  { value: 'gemini', label: 'Gemini' },
  { value: 'opencode', label: 'OpenCode' },
  { value: 'qwen', label: 'Qwen' },
  { value: 'openclaw', label: 'OpenClaw' },
  { value: 'shell', label: 'Shell' },
] as const

export const ROLE_LABELS: Record<AgentRole, string> = {
  assistant: '助手',
  worker: '员工',
  supervisor: '监工',
}

export const ROLE_COLORS: Record<AgentRole, { bg: string; text: string }> = {
  assistant: { bg: 'bg-purple-50', text: 'text-purple-600' },
  worker: { bg: 'bg-blue-50', text: 'text-blue-600' },
  supervisor: { bg: 'bg-green-50', text: 'text-green-600' },
}

export const STATUS_LABELS: Record<TerminalSessionStatus, string> = {
  pending: '启动中',
  online: '在线',
  working: '执行中',
  offline: '离线',
  broken: '异常',
}

export const STATUS_COLORS: Record<TerminalSessionStatus, string> = {
  pending: 'text-yellow-500',
  online: 'text-green-500',
  working: 'text-blue-500',
  offline: 'text-gray-400',
  broken: 'text-red-500',
}
```

- [ ] **步骤 2：创建 channel.ts**

```typescript
export type ChannelType = 'feishu' | 'weixin' | 'web'

export interface FeishuConfig {
  appId: string
  appSecret: string
  encryptKey: string
  verificationToken: string
  enabled: boolean
}

export interface WeixinConfig {
  appId: string
  appSecret: string
  token: string
  encodingAESKey: string
  enabled: boolean
}

export interface ChannelConfigs {
  feishu: FeishuConfig
  weixin: WeixinConfig
  web: { enabled: boolean }
}

export interface ChannelStatus {
  type: ChannelType
  connected: boolean
  lastConnectedAt?: number
  error?: string
}
```

- [ ] **步骤 3：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add ui/clawteam-chat/src/types/
git commit -m "feat(types): add CLI tools constants, channel types, and role/status color mappings"
```

---

## 任务 3：Store 扩展

**文件：**
- 修改：`ui/clawteam-chat/src/stores/agent.ts`
- 修改：`ui/clawteam-chat/src/stores/skill.ts`
- 创建：`ui/clawteam-chat/src/stores/channel.ts`
- 创建：`ui/clawteam-chat/src/stores/audit.ts`

- [ ] **步骤 1：增强 agent.ts store - 增加 CRUD actions**

在 `ui/clawteam-chat/src/stores/agent.ts` 的 return 之前，添加以下 actions：

```typescript
  const addAgent = (config: AgentConfig) => {
    agents.value[config.id] = {
      config,
      status: 'offline',
      lastOutputAt: 0,
    }
    if (!agentList.value.includes(config.id)) {
      agentList.value.push(config.id)
    }
  }

  const updateAgent = (id: string, updates: Partial<AgentConfig>) => {
    const agent = agents.value[id]
    if (agent) {
      agent.config = { ...agent.config, ...updates }
    }
  }

  const removeAgent = (id: string) => {
    delete agents.value[id]
    agentList.value = agentList.value.filter(aid => aid !== id)
    if (selectedAgentId.value === id) {
      selectedAgentId.value = null
    }
  }
```

并在 return 中导出：

```typescript
    addAgent,
    updateAgent,
    removeAgent,
```

- [ ] **步骤 2：增强 skill.ts store - 增加 executeSkill action**

在 `ui/clawteam-chat/src/stores/skill.ts` 的 return 之前，添加：

```typescript
  const executeSkill = (skillId: string, variables: Record<string, string>) => {
    const skill = skills.value[skillId]
    if (!skill) return
    console.log('Execute skill:', skill.name, variables)
  }
```

并在 return 中导出 `executeSkill`。

- [ ] **步骤 3：创建 channel.ts store**

```typescript
import { ref } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { FeishuConfig, WeixinConfig, ChannelStatus, ChannelType } from '@/types/channel'

export const useChannelStore = defineStore('channel', () => {
  const feishu = ref<FeishuConfig>({
    appId: '',
    appSecret: '',
    encryptKey: '',
    verificationToken: '',
    enabled: false,
  })

  const weixin = ref<WeixinConfig>({
    appId: '',
    appSecret: '',
    token: '',
    encodingAESKey: '',
    enabled: false,
  })

  const statuses = ref<Record<ChannelType, ChannelStatus>>({
    feishu: { type: 'feishu', connected: false },
    weixin: { type: 'weixin', connected: false },
    web: { type: 'web', connected: true },
  })

  const updateFeishuConfig = (updates: Partial<FeishuConfig>) => {
    feishu.value = { ...feishu.value, ...updates }
  }

  const updateWeixinConfig = (updates: Partial<WeixinConfig>) => {
    weixin.value = { ...weixin.value, ...updates }
  }

  const updateChannelStatus = (type: ChannelType, status: Partial<ChannelStatus>) => {
    statuses.value[type] = { ...statuses.value[type], ...status }
  }

  const loadConfigs = async () => {
    try {
      const response = await fetch('/api/channels')
      if (!response.ok) return
      const data = await response.json()
      if (data.feishu) feishu.value = { ...feishu.value, ...data.feishu }
      if (data.weixin) weixin.value = { ...weixin.value, ...data.weixin }
    } catch {
      // 使用默认值
    }
  }

  const saveConfigs = async () => {
    try {
      await fetch('/api/channels', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ feishu: feishu.value, weixin: weixin.value }),
      })
    } catch (e) {
      console.error('Failed to save channel configs:', e)
    }
  }

  return {
    feishu,
    weixin,
    statuses,
    updateFeishuConfig,
    updateWeixinConfig,
    updateChannelStatus,
    loadConfigs,
    saveConfigs,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useChannelStore, import.meta.hot))
}
```

- [ ] **步骤 4：创建 audit.ts store**

```typescript
import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { AuditLogEntry } from '@/types/audit'

export const useAuditStore = defineStore('audit', () => {
  const entries = ref<AuditLogEntry[]>([])
  const loading = ref(false)
  const filter = ref<string>('all')

  const filteredEntries = computed(() => {
    if (filter.value === 'all') return entries.value
    return entries.value.filter(e => e.eventType === filter.value)
  })

  const loadEntries = async () => {
    loading.value = true
    try {
      const response = await fetch('/api/audit')
      if (!response.ok) return
      entries.value = await response.json()
    } catch {
      entries.value = []
    } finally {
      loading.value = false
    }
  }

  const addEntry = (entry: AuditLogEntry) => {
    entries.value.unshift(entry)
  }

  const setFilter = (f: string) => {
    filter.value = f
  }

  return {
    entries,
    loading,
    filter,
    filteredEntries,
    loadEntries,
    addEntry,
    setFilter,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useAuditStore, import.meta.hot))
}
```

- [ ] **步骤 5：编写 agent store 增强测试**

在 `ui/clawteam-chat/src/stores/__tests__/agent.spec.ts` 末尾追加测试：

```typescript
  it('should add a new agent', () => {
    const store = useAgentStore()
    store.addAgent({
      id: 'worker-1',
      name: '测试员工',
      role: 'worker',
      cliTool: 'codex',
      args: [],
      env: {},
      autoStart: false,
      skills: [],
      enabled: true,
    })
    expect(store.agents['worker-1']).toBeDefined()
    expect(store.agents['worker-1'].config.name).toBe('测试员工')
    expect(store.agentList).toContain('worker-1')
  })

  it('should update an agent config', () => {
    const store = useAgentStore()
    store.addAgent({
      id: 'agent-x',
      name: 'Original',
      role: 'assistant',
      cliTool: 'opencode',
      args: [],
      env: {},
      autoStart: false,
      skills: [],
      enabled: true,
    })
    store.updateAgent('agent-x', { name: 'Updated', cliTool: 'claude' })
    expect(store.agents['agent-x'].config.name).toBe('Updated')
    expect(store.agents['agent-x'].config.cliTool).toBe('claude')
  })

  it('should remove an agent', () => {
    const store = useAgentStore()
    store.addAgent({
      id: 'to-remove',
      name: 'Remove Me',
      role: 'worker',
      cliTool: 'opencode',
      args: [],
      env: {},
      autoStart: false,
      skills: [],
      enabled: true,
    })
    store.selectAgent('to-remove')
    store.removeAgent('to-remove')
    expect(store.agents['to-remove']).toBeUndefined()
    expect(store.agentList).not.toContain('to-remove')
    expect(store.selectedAgentId).toBeNull()
  })
```

- [ ] **步骤 6：运行测试**

运行：`pnpm test`
预期：所有测试通过

- [ ] **步骤 7：Commit**

```bash
git add ui/clawteam-chat/src/stores/
git commit -m "feat(stores): add channel/audit stores, enhance agent CRUD and skill execute"
```

---

## 任务 4：Agent 配置弹窗

**文件：**
- 创建：`ui/clawteam-chat/src/components/agent/AgentConfigModal.vue`
- 修改：`ui/clawteam-chat/src/components/AgentPanel.vue`

- [ ] **步骤 1：创建 AgentConfigModal.vue**

```vue
<script setup lang="ts">
import { reactive, watch } from 'vue'
import { Dialog, Select, Switch, Input, Button } from '@/components/ui'
import { CLI_TOOLS, ROLE_LABELS } from '@/types/agent'
import type { AgentConfig, AgentRole, DispatchStrategy } from '@/types/agent'

interface Props {
  open: boolean
  agent?: AgentConfig | null
}

const props = defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
  save: [config: AgentConfig]
}>()

const form = reactive<AgentConfig>({
  id: '',
  name: '',
  role: 'worker',
  cliTool: 'opencode',
  args: [],
  env: {},
  cwd: undefined,
  autoStart: false,
  enabled: true,
  roleConfig: undefined,
  skills: [],
})

watch(() => props.agent, (agent) => {
  if (agent) {
    Object.assign(form, { ...agent })
  } else {
    Object.assign(form, {
      id: `${Date.now()}`,
      name: '',
      role: 'worker' as AgentRole,
      cliTool: 'opencode',
      args: [],
      env: {},
      cwd: undefined,
      autoStart: false,
      enabled: true,
      roleConfig: undefined,
      skills: [],
    })
  }
}, { immediate: true })

const handleSave = () => {
  emit('save', { ...form })
  emit('update:open', false)
}

const isEditing = () => !!props.agent
</script>

<template>
  <Dialog :open="open" class="max-w-md" @update:open="emit('update:open', $event)">
    <div class="space-y-4">
      <h2 class="text-lg font-semibold">
        {{ isEditing() ? `配置 ${agent?.name}` : '添加 Agent' }}
      </h2>

      <div class="space-y-3">
        <div>
          <label class="block text-sm font-medium text-gray-700 mb-1">名称</label>
          <Input v-model="form.name" placeholder="Agent 名称" />
        </div>

        <div>
          <label class="block text-sm font-medium text-gray-700 mb-1">角色</label>
          <Select v-model="form.role" placeholder="选择角色">
            <option v-for="(label, key) in ROLE_LABELS" :key="key" :value="key">
              {{ label }}
            </option>
          </Select>
        </div>

        <div>
          <label class="block text-sm font-medium text-gray-700 mb-1">CLI 工具</label>
          <Select v-model="form.cliTool" placeholder="选择 CLI 工具">
            <option v-for="tool in CLI_TOOLS" :key="tool.value" :value="tool.value">
              {{ tool.label }}
            </option>
          </Select>
        </div>

        <div>
          <label class="block text-sm font-medium text-gray-700 mb-1">工作目录</label>
          <Input v-model="form.cwd" placeholder="可选，留空使用默认" />
        </div>

        <div class="flex items-center justify-between">
          <label class="text-sm font-medium text-gray-700">自动启动</label>
          <Switch v-model="form.autoStart" />
        </div>

        <div class="flex items-center justify-between">
          <label class="text-sm font-medium text-gray-700">启用</label>
          <Switch v-model="form.enabled" />
        </div>
      </div>

      <div class="flex justify-end gap-2 pt-4 border-t">
        <Button variant="outline" @click="emit('update:open', false)">取消</Button>
        <Button @click="handleSave">保存</Button>
      </div>
    </div>
  </Dialog>
</template>
```

- [ ] **步骤 2：修改 AgentPanel.vue - 增加添加/编辑功能**

替换 `ui/clawteam-chat/src/components/AgentPanel.vue` 为：

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { storeToRefs } from 'pinia'
import { useAgentStore } from '@/stores/agent'
import AgentCard from './AgentCard.vue'
import AgentConfigModal from './agent/AgentConfigModal.vue'
import { Button } from './ui'
import { Plus, Settings } from 'lucide-vue-next'
import type { AgentRole, AgentConfig } from '@/types/agent'

const agentStore = useAgentStore()
const { selectedAgentId, groupedByRole } = storeToRefs(agentStore)

const roleOrder: AgentRole[] = ['assistant', 'worker', 'supervisor']
const roleLabels: Record<AgentRole, string> = {
  assistant: '助手',
  worker: '员工',
  supervisor: '监工',
}

const configModalOpen = ref(false)
const editingAgent = ref<AgentConfig | null>(null)

const handleAddAgent = (role: AgentRole) => {
  editingAgent.value = null
  configModalOpen.value = true
}
  configModalOpen.value = false
  editingAgent.value = null
}

```

</content>
const handleEditAgent = (id: string) => {
  const agent = agentStore.agents[id]
  if (agent) {
    editingAgent.value = { ...agent.config }
    configModalOpen.value = true
  }
}

const handleSaveConfig = (config: AgentConfig) => {
  agentStore.updateAgent(config.id, config)
  configModalOpen.value = false
  editingAgent.value = null
}

const handleCardAction = (action: string, id: string) => {
  if (action === 'config') handleEditAgent(id)
  else if (action === 'start') console.log('Start:', id)
  else if (action === 'stop') console.log('Stop:', id)
}
</script>

<template>
  <div class="w-64 border-r bg-gray-50 flex flex-col">
    <div class="p-4 border-b flex items-center justify-between">
      <h2 class="font-semibold text-sm">Agent 成员</h2>
    </div>

    <div class="flex-1 overflow-auto p-3 space-y-4">
      <template v-for="role in roleOrder" :key="role">
        <div v-if="groupedByRole[role]?.length">
          <div class="text-xs font-medium text-gray-500 mb-2 uppercase tracking-wide">
            {{ roleLabels[role] }}
          </div>
          <div class="space-y-1.5">
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
              @action="handleCardAction($event, agent.config.id)"
            />
          </div>
        </div>
      </template>
    </div>

    <div class="p-3 border-t space-y-1.5">
      <Button variant="outline" size="sm" class="w-full text-xs" @click="handleAddAgent('worker')">
        <Plus class="h-3 w-3 mr-1" />
        添加 Agent
      </Button>
    </div>

    <AgentConfigModal
      :open="configModalOpen"
      :agent="editingAgent"
      @update:open="configModalOpen = $event"
      @save="handleSaveConfig"
    />
  </div>
</template>
```

- [ ] **步骤 3：修改 AgentCard.vue - 增加 action 菜单**

在 `ui/clawteam-chat/src/components/AgentCard.vue` 中：

1. 在 emits 中增加 `action`：

```typescript
const emit = defineEmits<{
  click: []
  action: [action: string]
}>()
```

2. 在 Props 接口中增加 `showMenu`：

```typescript
interface Props {
  id: string
  name: string
  role: AgentRole
  status: TerminalSessionStatus
  cliTool: string
  required?: boolean
  selected?: boolean
  showMenu?: boolean
}
```

3. 在 template 中，将 `</div>` 闭合标签（卡片根元素）之前增加操作按钮：

```html
      <div class="flex items-center gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
        <button
          v-if="status === 'online' || status === 'working'"
          class="p-1 rounded hover:bg-gray-200 text-gray-500"
          @click.stop="emit('action', 'stop')"
          title="停止"
        >
          <Square class="h-3 w-3" />
        </button>
        <button
          v-else
          class="p-1 rounded hover:bg-gray-200 text-gray-500"
          @click.stop="emit('action', 'start')"
          title="启动"
        >
          <Play class="h-3 w-3" />
        </button>
        <button
          class="p-1 rounded hover:bg-gray-200 text-gray-500"
          @click.stop="emit('action', 'config')"
          title="配置"
        >
          <Settings class="h-3 w-3" />
        </button>
      </div>
```

需要增加导入 `Play`, `Square`, `Settings` 从 `lucide-vue-next`。

同时在卡片根 div 的 class 中加入 `group`：

```typescript
const cardClasses = computed(() =>
  cn(
    'group p-3 rounded-lg border cursor-pointer transition-all',
    // ...
  )
)
```

- [ ] **步骤 4：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/components/agent/ ui/clawteam-chat/src/components/AgentPanel.vue ui/clawteam-chat/src/components/AgentCard.vue
git commit -m "feat(agent): add AgentConfigModal and enhance AgentPanel/AgentCard with actions"
```

---

## 任务 5：Skill 商店组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/skill/SkillCard.vue`
- 创建：`ui/clawteam-chat/src/components/skill/SkillStore.vue`
- 创建：`ui/clawteam-chat/src/components/skill/SkillExecuteModal.vue`

- [ ] **步骤 1：创建 SkillCard.vue**

```vue
<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'
import { Code } from 'lucide-vue-next'
import type { Skill } from '@/types/skill'

interface Props {
  skill: Skill
  selected?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  selected: false,
})

const emit = defineEmits<{ click: [] }>()

const cardClasses = computed(() =>
  cn(
    'p-3 rounded-lg border cursor-pointer transition-all',
    props.selected
      ? 'bg-blue-50 border-blue-300 ring-2 ring-blue-200'
      : 'bg-white border-gray-200 hover:border-gray-300 hover:shadow-sm'
  )
)
</script>

<template>
  <div :class="cardClasses" @click="emit('click')">
    <div class="flex items-center gap-3">
      <div
        :class="cn('w-10 h-10 rounded-lg flex items-center justify-center', skill.bg)"
        :style="{ color: skill.color }"
      >
        <Code class="h-5 w-5" />
      </div>
      <div class="flex-1 min-w-0">
        <div class="font-medium text-sm truncate">{{ skill.name }}</div>
        <div class="text-xs text-gray-500 truncate">{{ skill.description }}</div>
      </div>
    </div>
    <div class="mt-2 flex gap-1">
      <span
        v-for="tag in skill.tags.slice(0, 2)"
        :key="tag"
        class="text-xs px-1.5 py-0.5 rounded bg-gray-100 text-gray-600"
      >
        {{ tag }}
      </span>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：创建 SkillExecuteModal.vue**

```vue
<script setup lang="ts">
import { reactive, watch } from 'vue'
import { Dialog, Input, Select, Button } from '@/components/ui'
import type { Skill, SkillVariable } from '@/types/skill'

interface Props {
  open: boolean
  skill: Skill | null
}

const props = defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
  execute: [skillId: string, variables: Record<string, string>]
}>()

const variables = reactive<Record<string, string>>({})

watch(() => props.skill, (skill) => {
  if (skill) {
    for (const v of skill.template.variables) {
      variables[v.name] = v.default ?? ''
    }
  }
}, { immediate: true })

const handleExecute = () => {
  if (props.skill) {
    emit('execute', props.skill.id, { ...variables })
    emit('update:open', false)
  }
}

const renderVariableLabel = (variable: SkillVariable) => {
  return variable.required ? `${variable.label} *` : variable.label
}
</script>

<template>
  <Dialog :open="open" class="max-w-md" @update:open="emit('update:open', $event)">
    <div v-if="skill" class="space-y-4">
      <div>
        <h2 class="text-lg font-semibold">{{ skill.name }}</h2>
        <p class="text-sm text-gray-500 mt-1">{{ skill.description }}</p>
      </div>

      <div class="space-y-3">
        <div v-for="variable in skill.template.variables" :key="variable.name">
          <label class="block text-sm font-medium text-gray-700 mb-1">
            {{ renderVariableLabel(variable) }}
          </label>
          <Select
            v-if="variable.varType === 'select' && variable.options"
            v-model="variables[variable.name]"
            :placeholder="variable.label"
          >
            <option v-for="opt in variable.options" :key="opt" :value="opt">
              {{ opt }}
            </option>
          </Select>
          <Input
            v-else
            v-model="variables[variable.name]"
            :type="variable.varType === 'number' ? 'number' : 'text'"
            :placeholder="variable.label"
          />
        </div>
      </div>

      <div class="flex justify-end gap-2 pt-4 border-t">
        <Button variant="outline" @click="emit('update:open', false)">取消</Button>
        <Button @click="handleExecute">执行</Button>
      </div>
    </div>
  </Dialog>
</template>
```

- [ ] **步骤 3：创建 SkillStore.vue**

```vue
<script setup lang="ts">
import { ref, computed } from 'vue'
import { storeToRefs } from 'pinia'
import { useSkillStore } from '@/stores/skill'
import SkillCard from './SkillCard.vue'
import SkillExecuteModal from './SkillExecuteModal.vue'
import { Input } from '@/components/ui'
import { Search, Package } from 'lucide-vue-next'
import type { Skill } from '@/types/skill'

const skillStore = useSkillStore()
const { skills, skillList } = storeToRefs(skillStore)

const searchQuery = ref('')
const executeModalOpen = ref(false)
const selectedSkill = ref<Skill | null>(null)

const filteredSkills = computed(() => {
  const q = searchQuery.value.toLowerCase()
  if (!q) return skillList.value.map(id => skills.value[id]).filter(Boolean)
  return skillList.value
    .map(id => skills.value[id])
    .filter(s => s.name.toLowerCase().includes(q) ||
      s.description.toLowerCase().includes(q) ||
      s.tags.some(t => t.toLowerCase().includes(q))
    )
  })
  return filtered
})
```

- [ ] **步骤 4：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/components/skill/
git commit -m "feat(skill): add SkillStore, SkillCard, and SkillExecuteModal components"
```

---

## 任务 6：Feishu/Weixin 渠道配置

**文件：**
- 创建：`ui/clawteam-chat/src/components/channel/FeishuConfig.vue`
- 创建：`ui/clawteam-chat/src/components/channel/WeixinConfig.vue`
- 创建：`ui/clawteam-chat/src/components/channel/ChannelConfig.vue`

- [ ] **步骤 1：创建 FeishuConfig.vue**

```vue
<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useChannelStore } from '@/stores/channel'
import { Input, Switch, Button } from '@/components/ui'
import { Save } from 'lucide-vue-next'

const channelStore = useChannelStore()
const { feishu, statuses } = storeToRefs(channelStore)

const handleSave = () => {
  channelStore.saveConfigs()
}
</script>

<template>
  <div class="space-y-4">
    <div class="flex items-center justify-between">
      <h3 class="font-medium">飞书渠道配置</h3>
      <div class="flex items-center gap-2">
        <span
          :class="statuses.feishu.connected ? 'text-green-500' : 'text-gray-400'"
          class="text-xs"
        >
          {{ statuses.feishu.connected ? '已连接' : '未连接' }}
        </span>
        <Switch :model-value="feishu.enabled" @update:model-value="channelStore.updateFeishuConfig({ enabled: $event })" />
      </div>
    </div>

    <div class="space-y-3">
      <div>
        <label class="block text-sm text-gray-600 mb-1">App ID</label>
        <Input :model-value="feishu.appId" placeholder="cli_xxxxxxxx" @update:model-value="channelStore.updateFeishuConfig({ appId: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">App Secret</label>
        <Input :model-value="feishu.appSecret" type="password" placeholder="飞书应用密钥" @update:model-value="channelStore.updateFeishuConfig({ appSecret: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">Encrypt Key</label>
        <Input :model-value="feishu.encryptKey" placeholder="可选" @update:model-value="channelStore.updateFeishuConfig({ encryptKey: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">Verification Token</label>
        <Input :model-value="feishu.verificationToken" placeholder="可选" @update:model-value="channelStore.updateFeishuConfig({ verificationToken: $event })" />
      </div>
    </div>

    <div class="pt-3 border-t">
      <Button size="sm" @click="handleSave">
        <Save class="h-3.5 w-3.5 mr-1.5" />
        保存配置
      </Button>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：创建 WeixinConfig.vue**

```vue
<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useChannelStore } from '@/stores/channel'
import { Input, Switch, Button } from '@/components/ui'
import { Save } from 'lucide-vue-next'

const channelStore = useChannelStore()
const { weixin, statuses } = storeToRefs(channelStore)

const handleSave = () => {
  channelStore.saveConfigs()
}
</script>

<template>
  <div class="space-y-4">
    <div class="flex items-center justify-between">
      <h3 class="font-medium">微信渠道配置</h3>
      <div class="flex items-center gap-2">
        <span
          :class="statuses.weixin.connected ? 'text-green-500' : 'text-gray-400'"
          class="text-xs"
        >
          {{ statuses.weixin.connected ? '已连接' : '未连接' }}
        </span>
        <Switch :model-value="weixin.enabled" @update:model-value="channelStore.updateWeixinConfig({ enabled: $event })" />
      </div>
    </div>

    <div class="space-y-3">
      <div>
        <label class="block text-sm text-gray-600 mb-1">App ID</label>
        <Input :model-value="weixin.appId" placeholder="wxaaaaaaaaxxxxxxxx" @update:model-value="channelStore.updateWeixinConfig({ appId: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">App Secret</label>
        <Input :model-value="weixin.appSecret" type="password" placeholder="微信应用密钥" @update:model-value="channelStore.updateWeixinConfig({ appSecret: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">Token</label>
        <Input :model-value="weixin.token" placeholder="消息校验 Token" @update:model-value="channelStore.updateWeixinConfig({ token: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">EncodingAESKey</label>
        <Input :model-value="weixin.encodingAESKey" placeholder="消息加解密密钥" @update:model-value="channelStore.updateWeixinConfig({ encodingAESKey: $event })" />
      </div>
    </div>

    <div class="pt-3 border-t">
      <Button size="sm" @click="handleSave">
        <Save class="h-3.5 w-3.5 mr-1.5" />
        保存配置
      </Button>
    </div>
  </div>
</template>
```

- [ ] **步骤 3：创建 ChannelConfig.vue**

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { Dialog } from '@/components/ui'
import { Button } from '@/components/ui'
import FeishuConfig from './FeishuConfig.vue'
import WeixinConfig from './WeixinConfig.vue'
import { MessageSquare, MessageCircle } from 'lucide-vue-next'

interface Props {
  open: boolean
}

defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
}>()

const activeTab = ref<'feishu' | 'weixin'>('feishu')
</script>

<template>
  <Dialog :open="open" class="max-w-lg" @update:open="emit('update:open', $event)">
    <div class="space-y-4">
      <h2 class="text-lg font-semibold">渠道配置</h2>

      <div class="flex border-b">
        <button
          :class="activeTab === 'feishu' ? 'border-blue-500 text-blue-600' : 'border-transparent text-gray-500 hover:text-gray-700'"
          class="px-4 py-2 text-sm font-medium border-b-2 transition-colors"
          @click="activeTab = 'feishu'"
        >
          <MessageSquare class="h-4 w-4 inline mr-1.5" />
          飞书
        </button>
        <button
          :class="activeTab === 'weixin' ? 'border-blue-500 text-blue-600' : 'border-transparent text-gray-500 hover:text-gray-700'"
          class="px-4 py-2 text-sm font-medium border-b-2 transition-colors"
          @click="activeTab = 'weixin'"
        >
          <MessageCircle class="h-4 w-4 inline mr-1.5" />
          微信
        </button>
      </div>

      <FeishuConfig v-if="activeTab === 'feishu'" />
      <WeixinConfig v-if="activeTab === 'weixin'" />
    </div>
  </Dialog>
</template>
```

- [ ] **步骤 4：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/components/channel/
git commit -m "feat(channel): add Feishu/Weixin channel config components"
```

---

## 任务 7：审计日志面板

**文件：**
- 创建：`ui/clawteam-chat/src/components/audit/AuditPanel.vue`

- [ ] **步骤 1：创建 AuditPanel.vue**

```vue
<script setup lang="ts">
import { onMounted, computed } from 'vue'
import { storeToRefs } from 'pinia'
import { useAuditStore } from '@/stores/audit'
import { Badge } from '@/components/ui'
import { ScrollText, Filter } from 'lucide-vue-next'
import type { AuditEventType, AuditLogEntry } from '@/types/audit'

const auditStore = useAuditStore()
const { filter } = storeToRefs(auditStore)

onMounted(() => {
  auditStore.loadEntries()
})

const eventTypeLabels: Record<string, string> = {
  'job-triggered': 'Job 触发',
  'job-completed': 'Job 完成',
  'cli-started': 'CLI 启动',
  'cli-completed': 'CLI 完成',
  'cli-failed': 'CLI 失败',
  'cli-killed': 'CLI 终止',
  'output-captured': '输出捕获',
  'user-paused': '用户暂停',
  'user-resumed': '用户恢复',
  'user-cancelled': '用户取消',
  'process-timeout': '进程超时',
  'output-error': '输出错误',
}

const eventTypeVariants: Record<string, 'default' | 'success' | 'destructive' | 'warning' | 'working'> = {
  'job-triggered': 'working',
  'job-completed': 'success',
  'cli-started': 'working',
  'cli-completed': 'success',
  'cli-failed': 'destructive',
  'cli-killed': 'destructive',
  'output-captured': 'default',
  'user-paused': 'warning',
  'user-resumed': 'default',
  'user-cancelled': 'warning',
  'process-timeout': 'destructive',
  'output-error': 'destructive',
}

const formatTime = (timestamp: number) => {
  return new Date(timestamp).toLocaleString()
}

const entries = computed(() => auditStore.filteredEntries())

const filterOptions = [
  { value: 'all', label: '全部' },
  { value: 'cli-started', label: 'CLI 启动' },
  { value: 'cli-completed', label: 'CLI 完成' },
  { value: 'cli-failed', label: 'CLI 失败' },
  { value: 'job-triggered', label: 'Job 触发' },
  { value: 'job-completed', label: 'Job 完成' },
]
</script>

<template>
  <div class="flex flex-col h-full">
    <div class="p-3 border-b">
      <div class="flex items-center justify-between mb-2">
        <div class="flex items-center gap-2">
          <ScrollText class="h-4 w-4 text-gray-600" />
          <h2 class="font-semibold text-sm">审计日志</h2>
        </div>
        <div class="flex items-center gap-1">
          <Filter class="h-3 w-3 text-gray-400" />
          <select
            :value="filter"
            class="text-xs border rounded px-2 py-1 bg-white"
            @change="auditStore.setFilter(($event.target as HTMLSelectElement).value)"
          >
            <option v-for="opt in filterOptions" :key="opt.value" :value="opt.value">
              {{ opt.label }}
            </option>
          </select>
        </div>
      </div>
    </div>

    <div class="flex-1 overflow-auto">
      <div
        v-for="entry in entries"
        :key="entry.id"
        class="px-3 py-2 border-b hover:bg-gray-50"
      >
        <div class="flex items-center gap-2 mb-1">
          <Badge :variant="eventTypeVariants[entry.eventType] ?? 'default'">
            {{ eventTypeLabels[entry.eventType] ?? entry.eventType }}
          </Badge>
          <span class="text-xs text-gray-400">{{ formatTime(entry.timestamp) }}</span>
        </div>
        <div v-if="entry.sessionId" class="text-xs text-gray-500">
          Session: {{ entry.sessionId }}
        </div>
        <div v-if="entry.targetId" class="text-xs text-gray-500">
          Target: {{ entry.targetId }}
        </div>
      </div>
      <div v-if="entries.length === 0" class="text-center text-gray-400 py-8 text-sm">
        暂无审计日志
      </div>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/components/audit/
git commit -m "feat(audit): add AuditPanel component with filter and event display"
```

---

## 任务 8：增强 ChatInterface 主布局

**文件：**
- 修改：`ui/clawteam-chat/src/components/ChatInterface.vue`
- 修改：`ui/clawteam-chat/src/components/OutputPanel.vue`

- [ ] **步骤 1：增强 OutputPanel.vue - 增加搜索和操作按钮**

替换 `ui/clawteam-chat/src/components/OutputPanel.vue` 为：

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { Input, Button } from '@/components/ui'
import { Search, Pause, Square, Copy } from 'lucide-vue-next'

interface Props {
  output?: string
}

defineProps<Props>()

const emit = defineEmits<{
  pause: []
  stop: []
  search: [query: string]
}>()

const searchQuery = ref('')
</script>

<template>
  <div class="w-96 border-l bg-gray-900 text-gray-100 flex flex-col">
    <div class="p-3 border-b border-gray-700 flex items-center gap-2">
      <span class="text-sm font-medium text-gray-400">CLI 输出</span>
      <div class="flex-1" />
      <div class="relative">
        <Search class="absolute left-2 top-1/2 -translate-y-1/2 h-3 w-3 text-gray-500" />
        <Input
          v-model="searchQuery"
          placeholder="搜索..."
          class="pl-7 h-7 text-xs bg-gray-800 border-gray-700 text-gray-300 placeholder:text-gray-600"
          @update:model-value="emit('search', $event)"
        />
      </div>
    </div>

    <div class="flex-1 overflow-auto p-3 font-mono text-sm">
      <div v-if="output" class="whitespace-pre-wrap text-gray-300">
        {{ output }}
      </div>
      <div v-else class="text-gray-600 text-center py-8">
        等待输出...
      </div>
    </div>

    <div class="p-2 border-t border-gray-700 flex items-center gap-1.5">
      <Button variant="ghost" size="sm" class="text-gray-400 hover:text-gray-200 h-7 text-xs" @click="emit('pause')">
        <Pause class="h-3 w-3 mr-1" />
        暂停
      </Button>
      <Button variant="ghost" size="sm" class="text-gray-400 hover:text-gray-200 h-7 text-xs" @click="emit('stop')">
        <Square class="h-3 w-3 mr-1" />
        终止
      </Button>
      <div class="flex-1" />
      <Button variant="ghost" size="sm" class="text-gray-400 hover:text-gray-200 h-7 text-xs">
        <Copy class="h-3 w-3 mr-1" />
        复制
      </Button>
    </div>
  </div>
</template>
```

- [ ] **步骤 2：增强 ChatInterface.vue - 三栏布局 + 顶部导航**

替换 `ui/clawteam-chat/src/components/ChatInterface.vue` 为：

```vue
<script setup lang="ts">
import { ref } from 'vue'
import { storeToRefs } from 'pinia'
import AgentPanel from './AgentPanel.vue'
import MessageList from './MessageList.vue'
import ChatInput from './ChatInput.vue'
import OutputPanel from './OutputPanel.vue'
import SkillStore from './skill/SkillStore.vue'
import ChannelConfig from './channel/ChannelConfig.vue'
import AuditPanel from './audit/AuditPanel.vue'
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
  <div class="flex h-full">
    <AgentPanel />

    <SkillStore v-if="showSkillStore" />

    <div class="flex-1 flex flex-col">
      <div class="border-b px-4 py-2 flex items-center justify-between bg-white">
        <h1 class="font-semibold text-sm">ClawTeam</h1>
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
            设置
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

      <MessageList :messages="currentMessages" />
      <ChatInput @send="handleSend" />
    </div>

    <OutputPanel v-if="showOutput" />

    <div v-if="showAuditPanel" class="w-80 border-l bg-white">
      <AuditPanel />
    </div>

    <ChannelConfig
      :open="channelConfigOpen"
      @update:open="channelConfigOpen = $event"
    />
  </div>
</template>
```

- [ ] **步骤 3：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 4：运行测试**

运行：`pnpm test`
预期：所有测试通过

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/components/ChatInterface.vue ui/clawteam-chat/src/components/OutputPanel.vue
git commit -m "feat(layout): enhance ChatInterface with skill store, audit panel, channel config, and output panel"
```

---

## 任务 9：增强 Gateway API

**文件：**
- 修改：`ui/clawteam-chat/src/composables/useGateway.ts`

- [ ] **步骤 1：增加渠道和审计 API 函数**

在 `ui/clawteam-chat/src/composables/useGateway.ts` 文件末尾追加：

```typescript
export async function fetchChannelConfigs(): Promise<{
  feishu: Record<string, unknown>
  weixin: Record<string, unknown>
}> {
  const response = await fetch(`${API_BASE}/channels`)
  if (!response.ok) throw new Error('Failed to fetch channel configs')
  return response.json()
}

export async function saveChannelConfigs(configs: {
  feishu: Record<string, unknown>
  weixin: Record<string, unknown>
}): Promise<void> {
  const response = await fetch(`${API_BASE}/channels`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(configs),
  })
  if (!response.ok) throw new Error('Failed to save channel configs')
}

export async function fetchAuditLogs(): Promise<unknown[]> {
  const response = await fetch(`${API_BASE}/audit`)
  if (!response.ok) throw new Error('Failed to fetch audit logs')
  return response.json()
}
```

- [ ] **步骤 2：运行类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 3：Commit**

```bash
git add ui/clawteam-chat/src/composables/useGateway.ts
git commit -m "feat(api): add channel config and audit log API functions"
```

---

## 任务 10：最终验证

- [ ] **步骤 1：运行完整类型检查**

运行：`pnpm check`
预期：PASS

- [ ] **步骤 2：运行所有测试**

运行：`pnpm test`
预期：所有测试通过

- [ ] **步骤 3：运行开发服务器**

运行：`pnpm dev`
预期：服务器启动成功，访问 http://localhost:5174

- [ ] **步骤 4：运行构建**

运行：`pnpm build`
预期：构建成功

- [ ] **步骤 5：Final Commit**

```bash
git add ui/
git commit -m "feat(ui): complete ClawTeam frontend with agent config, skill store, channel config, and audit panel"
```

---

## 验证清单

- [ ] `pnpm check` 通过
- [ ] `pnpm test` 通过
- [ ] `pnpm build` 通过
- [ ] `pnpm dev` 启动成功
- [ ] AgentPanel 可按角色分组展示、可添加/配置 Agent
- [ ] AgentConfigModal 可配置名称、角色、CLI 工具、工作目录等
- [ ] SkillStore 面板可展示和搜索 Skill
- [ ] SkillExecuteModal 可输入变量并执行 Skill
- [ ] ChannelConfig 包含飞书和微信 Tab，可配置各渠道参数
- [ ] FeishuConfig 包含 App ID、App Secret、Encrypt Key、Verification Token
- [ ] WeixinConfig 包含 App ID、App Secret、Token、EncodingAESKey
- [ ] AuditPanel 可展示和过滤审计日志
- [ ] ChatInterface 顶部导航包含 Skill、审计、设置、输出按钮
- [ ] OutputPanel 增加搜索和操作按钮（暂停/终止/复制）

---

## 最终目录结构

```
ui/clawteam-chat/src/
├── main.ts
├── App.vue
├── index.css
├── router/
│   └── index.ts
├── stores/
│   ├── index.ts
│   ├── agent.ts           # 增强 CRUD
│   ├── chat.ts
│   ├── skill.ts            # 增强 executeSkill
│   ├── channel.ts          # 【新增】
│   └── audit.ts            # 【新增】
├── composables/
│   ├── useGateway.ts       # 增强 API
│   └── useWebSocket.ts
├── components/
│   ├── ChatInterface.vue   # 增强
│   ├── AgentPanel.vue      # 增强
│   ├── AgentCard.vue       # 增强
│   ├── MessageList.vue
│   ├── ChatInput.vue
│   ├── OutputPanel.vue     # 增强
│   ├── agent/
│   │   └── AgentConfigModal.vue  # 【新增】
│   ├── skill/
│   │   ├── SkillStore.vue        # 【新增】
│   │   ├── SkillCard.vue         # 【新增】
│   │   └── SkillExecuteModal.vue # 【新增】
│   ├── channel/
│   │   ├── ChannelConfig.vue     # 【新增】
│   │   ├── FeishuConfig.vue      # 【新增】
│   │   └── WeixinConfig.vue      # 【新增】
│   ├── audit/
│   │   └── AuditPanel.vue        # 【新增】
│   └── ui/
│       ├── index.ts               # 增强
│       ├── Button.vue
│       ├── Card.vue
│       ├── CardHeader.vue
│       ├── CardTitle.vue
│       ├── CardContent.vue
│       ├── Input.vue
│       ├── Badge.vue              # 【新增】
│       ├── Dialog.vue             # 【新增】
│       ├── Select.vue             # 【新增】
│       ├── Switch.vue             # 【新增】
│       ├── Tabs.vue               # 【新增】
│       └── Textarea.vue           # 【新增】
├── views/
│   └── ChatView.vue
├── lib/
│   └── utils.ts
└── types/
    ├── agent.ts           # 增强
    ├── message.ts
    ├── skill.ts
    ├── audit.ts
    ├── session.ts
    └── channel.ts         # 【新增】
```
