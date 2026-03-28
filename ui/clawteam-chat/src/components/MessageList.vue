<script setup lang="ts">
import { User, Bot, Wrench, Eye } from 'lucide-vue-next'
import { cn } from '@/lib/utils'
import type { ChatMessage, SenderType } from '@/types/message'

interface Props {
  messages: ChatMessage[]
}

defineProps<Props>()

const senderIcons: Record<SenderType, typeof User> = {
  user: User,
  assistant: Bot,
  worker: Wrench,
  supervisor: Eye,
  system: User,
}

const senderColors: Record<SenderType, string> = {
  user: 'bg-blue-100',
  assistant: 'bg-purple-100',
  worker: 'bg-green-100',
  supervisor: 'bg-orange-100',
  system: 'bg-gray-100',
}

const formatTime = (timestamp: number) => {
  return new Date(timestamp).toLocaleTimeString()
}
</script>

<template>
  <div class="flex-1 overflow-auto p-4 space-y-4">
    <div
      v-for="message in messages"
      :key="message.id"
      :class="cn(
        'flex gap-3 p-3 rounded-lg',
        message.senderType === 'user' ? 'flex-row-reverse' : 'flex-row'
      )"
    >
      <div
        :class="cn(
          'flex-shrink-0 w-8 h-8 rounded-full flex items-center justify-center',
          senderColors[message.senderType]
        )"
      >
        <component :is="senderIcons[message.senderType]" class="h-4 w-4" />
      </div>
      
      <div :class="cn('flex-1', message.senderType === 'user' && 'text-right')">
        <div class="flex items-center gap-2 mb-1" :class="message.senderType === 'user' && 'justify-end'">
          <span class="font-medium text-sm">{{ message.senderName }}</span>
          <span class="text-xs text-gray-400">
            {{ formatTime(message.createdAt) }}
          </span>
        </div>
        <div
          :class="cn(
            'inline-block px-3 py-2 rounded-lg text-sm',
            message.senderType === 'user'
              ? 'bg-blue-500 text-white'
              : 'bg-gray-100 text-gray-900'
          )"
        >
          {{ message.content }}
        </div>
        <div v-if="message.status === 'processing'" class="text-xs text-gray-400 mt-1">
          处理中...
        </div>
        <div v-if="message.linkedAgent" class="text-xs text-gray-400 mt-1">
          → {{ message.linkedAgent }}
        </div>
      </div>
    </div>
  </div>
</template>
