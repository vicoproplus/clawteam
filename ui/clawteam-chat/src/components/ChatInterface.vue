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
