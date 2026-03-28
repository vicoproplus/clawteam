<script setup lang="ts">
import { ref } from 'vue'
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
