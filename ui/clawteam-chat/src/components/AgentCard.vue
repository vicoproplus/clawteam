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
  required?: boolean
  selected?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  required: false,
  selected: false,
})

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
