<script setup lang="ts">
import { useRoute, useRouter } from 'vue-router'
import { cn } from '@/lib/utils'
import { 
  MessageSquare, 
  Terminal, 
  Settings, 
  FolderOpen
} from 'lucide-vue-next'
import WorkspaceSelection from './WorkspaceSelection.vue'

const route = useRoute()
const router = useRouter()

const navItems = [
  { path: '/', icon: MessageSquare, label: '聊天' },
  { path: '/terminal', icon: Terminal, label: '终端' },
  { path: '/settings', icon: Settings, label: '设置' },
]

const isActive = (path: string) => {
  if (path === '/') return route.path === '/'
  return route.path.startsWith(path)
}
</script>

<template>
  <div class="w-14 bg-gray-900 flex flex-col items-center py-3">
    <!-- Logo -->
    <div class="w-10 h-10 rounded-lg bg-blue-600 flex items-center justify-center mb-6">
      <span class="text-white font-bold text-lg">C</span>
    </div>

    <!-- Workspace Selector -->
    <div class="mb-6">
      <WorkspaceSelection />
    </div>

    <!-- Navigation Items -->
    <nav class="flex-1 flex flex-col items-center gap-2">
      <button
        v-for="item in navItems"
        :key="item.path"
        :class="cn(
          'w-10 h-10 rounded-lg flex items-center justify-center transition-colors',
          isActive(item.path)
            ? 'bg-gray-700 text-white'
            : 'text-gray-400 hover:bg-gray-800 hover:text-gray-200'
        )"
        :title="item.label"
        @click="router.push(item.path)"
      >
        <component :is="item.icon" class="h-5 w-5" />
      </button>
    </nav>

    <!-- Bottom Actions -->
    <div class="mt-auto flex flex-col items-center gap-2">
      <button
        :class="cn(
          'w-10 h-10 rounded-lg flex items-center justify-center text-gray-400 hover:bg-gray-800 hover:text-gray-200'
        )"
        title="打开文件夹"
      >
        <FolderOpen class="h-5 w-5" />
      </button>
    </div>
  </div>
</template>
