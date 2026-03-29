<script setup lang="ts">
import { computed } from 'vue'
import { useChatStore } from '@/stores/chat'
import { cn } from '@/lib/utils'
import { MessageSquare, Plus } from 'lucide-vue-next'

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
