<script setup lang="ts">
import { computed } from 'vue'
import { useProjectStore } from '@/stores/project'
import { cn } from '@/lib/utils'
import { Bot, User, Crown, Circle } from 'lucide-vue-next'
import type { MemberStatus } from '@/types/workspace'

const projectStore = useProjectStore()
const members = computed(() => projectStore.members)

const statusColors: Record<MemberStatus, string> = {
  online: 'text-green-500',
  working: 'text-blue-500',
  dnd: 'text-red-500',
  offline: 'text-gray-400',
}

const roleIcons = {
  owner: Crown,
  admin: User,
  assistant: Bot,
  member: User,
}
</script>

<template>
  <div class="w-56 border-l bg-white flex flex-col">
    <div class="px-3 py-2 border-b">
      <h3 class="text-sm font-medium text-gray-700">成员</h3>
    </div>
    
    <div class="flex-1 overflow-auto p-2 space-y-1">
      <button
        v-for="member in members"
        :key="member.id"
        class="w-full px-2 py-2 rounded-md flex items-center gap-2 hover:bg-gray-50 text-left"
      >
        <div class="relative">
          <div class="w-8 h-8 rounded-full bg-gray-200 flex items-center justify-center">
            <component :is="roleIcons[member.roleType]" class="h-4 w-4 text-gray-500" />
          </div>
          <Circle
            :class="cn('absolute -bottom-0.5 -right-0.5 w-3 h-3 fill-current', statusColors[member.status])"
          />
        </div>
        <div class="flex-1 min-w-0">
          <div class="text-sm font-medium truncate">{{ member.name }}</div>
          <div class="text-xs text-gray-500 truncate">{{ member.role }}</div>
        </div>
      </button>
      
      <div v-if="members.length === 0" class="text-center text-gray-400 py-4 text-sm">
        暂无成员
      </div>
    </div>
  </div>
</template>
