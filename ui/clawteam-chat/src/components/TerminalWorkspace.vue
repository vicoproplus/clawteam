<script setup lang="ts">
import { computed } from 'vue'
import { useTerminalStore } from '@/stores/terminal'
import TerminalPane from './TerminalPane.vue'
import { Plus, X } from 'lucide-vue-next'
import { cn } from '@/lib/utils'

const terminalStore = useTerminalStore()

const sessions = computed(() => terminalStore.sessionsList)
const activeId = computed(() => terminalStore.activeSessionId)

const handleAddTerminal = () => {
  // TODO: 创建新终端
  console.log('Add terminal')
}

const handleCloseTerminal = (id: string) => {
  terminalStore.closeSession(id)
}

const handleSelectTerminal = (id: string) => {
  terminalStore.setActiveSession(id)
}
</script>

<template>
  <div class="h-full flex flex-col bg-gray-900">
    <!-- Tab Bar -->
    <div class="flex items-center bg-gray-800 border-b border-gray-700">
      <div class="flex-1 flex items-center overflow-x-auto">
        <button
          v-for="session in sessions"
          :key="session.id"
          :class="cn(
            'px-3 py-2 text-sm flex items-center gap-2 border-r border-gray-700 min-w-[120px] max-w-[200px]',
            session.id === activeId
              ? 'bg-gray-900 text-white'
              : 'bg-gray-800 text-gray-400 hover:bg-gray-750'
          )"
          @click="handleSelectTerminal(session.id)"
        >
          <span class="truncate flex-1">{{ session.terminalType }}</span>
          <span
            class="hover:bg-gray-700 rounded p-0.5 cursor-pointer"
            @click.stop="handleCloseTerminal(session.id)"
          >
            <X class="h-3 w-3" />
          </span>
        </button>
      </div>
      
      <button
        class="px-3 py-2 text-gray-400 hover:text-white hover:bg-gray-700"
        @click="handleAddTerminal"
      >
        <Plus class="h-4 w-4" />
      </button>
    </div>

    <!-- Terminal Content -->
    <div class="flex-1 relative">
      <div
        v-for="session in sessions"
        :key="session.id"
        v-show="session.id === activeId"
        class="absolute inset-0"
      >
        <TerminalPane
          :session="session"
          :active="session.id === activeId"
        />
      </div>
      
      <div
        v-if="sessions.length === 0"
        class="h-full flex items-center justify-center text-gray-500"
      >
        <div class="text-center">
          <p class="text-sm">暂无终端会话</p>
          <button
            class="mt-2 text-blue-400 hover:text-blue-300 text-sm"
            @click="handleAddTerminal"
          >
            + 创建新终端
          </button>
        </div>
      </div>
    </div>
  </div>
</template>
