<script setup lang="ts">
import { onMounted, computed } from 'vue'
import { storeToRefs } from 'pinia'
import { useAuditStore } from '@/stores/audit'
import { Badge } from '@/components/ui'
import { ScrollText, Filter } from 'lucide-vue-next'


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

const entries = computed(() => auditStore.filteredEntries)

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
