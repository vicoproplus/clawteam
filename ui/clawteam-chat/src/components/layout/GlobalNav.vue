<script setup lang="ts">
import { useRoute, useRouter } from 'vue-router'
import { MessageSquare, Users, Store, Folder, HelpCircle, Settings } from 'lucide-vue-next'

const route = useRoute()
const router = useRouter()

const navItems = [
  { path: '/chat', icon: MessageSquare, label: '聊天' },
  { path: '/agents', icon: Users, label: '员工' },
  { path: '/skills', icon: Store, label: '商店' },
  { path: '/files', icon: Folder, label: '文件' },
]

const bottomItems = [
  { path: '/help', icon: HelpCircle, label: '帮助' },
  { path: '/settings/account', icon: Settings, label: '设置' },
]

const isActive = (path: string) => route.path.startsWith(path)

function navigate(path: string) {
  router.push(path)
}
</script>

<template>
  <div class="w-16 bg-gray-950 flex flex-col items-center py-4 gap-4 border-r border-gray-800 z-20">
    <div class="relative group cursor-pointer">
      <div class="w-10 h-10 rounded-full border-2 border-gray-800 group-hover:border-sky-500 transition-colors bg-gradient-to-br from-purple-500 to-blue-600 flex items-center justify-center text-white font-bold">
        C
      </div>
      <div class="absolute top-0 right-0 w-3 h-3 bg-green-500 border-2 border-gray-950 rounded-full"></div>
    </div>
    <div class="w-8 h-px bg-gray-800 my-2"></div>
    <button v-for="item in navItems" :key="item.path" @click="navigate(item.path)"
      :class="['w-10 h-10 rounded-xl flex items-center justify-center transition-all', isActive(item.path) ? 'bg-sky-500/20 text-sky-400' : 'text-gray-400 hover:text-white hover:bg-gray-800']"
      :title="item.label">
      <component :is="item.icon" class="w-5 h-5" />
    </button>
    <div class="flex-1"></div>
    <button v-for="item in bottomItems" :key="item.path" @click="navigate(item.path)"
      :class="['w-10 h-10 rounded-xl flex items-center justify-center transition-all', isActive(item.path) ? 'bg-sky-500/20 text-sky-400' : 'text-gray-400 hover:text-white hover:bg-gray-800']"
      :title="item.label">
      <component :is="item.icon" class="w-5 h-5" />
    </button>
  </div>
</template>
