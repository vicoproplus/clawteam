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
