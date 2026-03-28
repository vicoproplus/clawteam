<script setup lang="ts">
import { reactive, watch } from 'vue'
import { Dialog, Select, Switch, Input, Button } from '@/components/ui'
import { CLI_TOOLS, ROLE_LABELS } from '@/types/agent'
import type { AgentConfig, AgentRole } from '@/types/agent'

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
