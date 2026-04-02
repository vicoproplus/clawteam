<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { GitBranch, Clock, User, Palette, Terminal, Users } from 'lucide-vue-next'

const route = useRoute()

const currentView = computed(() => {
  if (route.path.startsWith('/chat')) return 'chat'
  if (route.path.startsWith('/agents')) return 'agents'
  if (route.path.startsWith('/skills')) return 'skills'
  if (route.path.startsWith('/settings')) return 'settings'
  return 'chat'
})

const settingsNavItems = [
  { id: 'account', label: '我的账号', icon: User },
  { id: 'appearance', label: '外观', icon: Palette },
  { id: 'cli', label: 'CLI配置', icon: Terminal },
]
</script>

<template>
  <div class="w-64 bg-gray-900/50 flex flex-col border-r border-gray-800 backdrop-blur-sm">
    <!-- Chat Sidebar -->
    <div v-if="currentView === 'chat'" class="p-4 h-full flex flex-col">
      <div class="flex items-center gap-3 mb-4">
        <div class="w-8 h-8 rounded-lg bg-sky-900/30 flex items-center justify-center text-sky-400 border border-sky-900/50">
          <GitBranch class="w-4 h-4" />
        </div>
        <h2 class="text-base font-bold text-white">docs</h2>
      </div>
      <div class="text-xs font-semibold text-gray-500 uppercase tracking-wider mb-2">频道</div>
      <div class="space-y-1 mb-6">
        <div class="flex items-center gap-3 px-3 py-2 rounded-lg bg-gray-800 border border-gray-700 cursor-pointer">
          <div class="w-6 h-6 rounded-full bg-gradient-to-br from-purple-500 to-blue-600"></div>
          <div class="flex-1 min-w-0">
            <div class="text-sm font-medium text-white truncate">docs</div>
            <p class="text-xs text-gray-500 truncate">WorkBuddy: 我来看看...</p>
          </div>
        </div>
      </div>
      <div class="text-xs font-semibold text-gray-500 uppercase tracking-wider mb-2">会话记录</div>
      <div class="space-y-1">
        <div class="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-gray-800/50 cursor-pointer text-gray-400">
          <Clock class="w-3 h-3" />
          <span class="text-sm">昨天</span>
        </div>
        <div class="flex items-center gap-3 px-3 py-2 rounded-lg hover:bg-gray-800/50 cursor-pointer text-gray-400">
          <Clock class="w-3 h-3" />
          <span class="text-sm">上周</span>
        </div>
      </div>
    </div>

    <!-- Agents Sidebar -->
    <div v-else-if="currentView === 'agents'" class="p-4 h-full overflow-y-auto">
      <div class="flex items-center gap-3 mb-6">
        <div class="w-10 h-10 rounded-xl bg-sky-900/30 flex items-center justify-center text-sky-400 border border-sky-900/50">
          <Users class="w-5 h-5" />
        </div>
        <div>
          <h2 class="text-lg font-bold text-white">员工管理</h2>
          <p class="text-xs text-gray-500">管理助手与成员</p>
        </div>
      </div>
    </div>

    <!-- Settings Sidebar -->
    <div v-else-if="currentView === 'settings'" class="p-4 h-full overflow-y-auto">
      <h2 class="text-xl font-bold text-white mb-6">设置</h2>
      <div class="space-y-1">
        <div class="text-xs font-semibold text-gray-500 uppercase tracking-wider mb-2 ml-2">用户设置</div>
        <router-link v-for="item in settingsNavItems" :key="item.id" :to="`/settings/${item.id}`"
          class="flex items-center gap-3 px-3 py-2.5 rounded-lg text-gray-400 hover:text-white w-full transition-colors"
          :class="{ 'bg-sky-500/20 text-white': route.path === `/settings/${item.id}` }">
          <component :is="item.icon" class="w-4 h-4" />
          <span class="text-sm">{{ item.label }}</span>
        </router-link>
      </div>
    </div>

    <!-- Skills Sidebar -->
    <div v-else-if="currentView === 'skills'" class="p-4 h-full">
      <h2 class="text-lg font-bold text-white mb-1">商店</h2>
      <p class="text-xs text-gray-500 mb-6">统一管理技能、模板与插件</p>
    </div>
  </div>
</template>
