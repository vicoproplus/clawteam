<script setup lang="ts">
import { onMounted } from 'vue'
import ChatInterface from '@/components/ChatInterface.vue'
import ChatSidebar from '@/components/ChatSidebar.vue'
import MembersSidebar from '@/components/MembersSidebar.vue'
import { useChatStore } from '@/stores/chat'

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