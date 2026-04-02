<script setup lang="ts">
import { useSettingsStore } from '@/stores/settings'
import { useToastStore } from '@/stores/toast'
import type { Theme } from '@/types/settings'

const settingsStore = useSettingsStore()
const toastStore = useToastStore()

const themes: { id: Theme; label: string; preview: string }[] = [
  { id: 'dark', label: '深色', preview: 'linear-gradient(135deg,#0d1117,#1c2333)' },
  { id: 'light', label: '浅色', preview: 'linear-gradient(135deg,#f0f2f5,#fff)' },
  { id: 'system', label: '系统', preview: 'linear-gradient(135deg,#0d1117 50%,#f0f2f5 50%)' },
]

function handleThemeChange(theme: Theme) {
  settingsStore.setTheme(theme)
  toastStore.success(`已切换到「${themes.find(t => t.id === theme)?.label}」主题`)
}
</script>

<template>
  <div>
    <h3 class="text-base font-bold text-gray-200 mb-4 pb-2.5 border-b border-gray-700">外观</h3>
    <div class="py-3 border-b border-gray-800/50">
      <div class="font-medium text-sm text-gray-200 mb-4">主题</div>
      <div class="flex gap-2">
        <button v-for="theme in themes" :key="theme.id" @click="handleThemeChange(theme.id)"
          :class="['w-20 py-3 rounded-lg border-2 transition-colors text-center',
            settingsStore.theme === theme.id ? 'border-sky-500 bg-sky-500/10' : 'border-gray-700 bg-gray-800 hover:border-gray-600']">
          <div :style="{ background: theme.preview }" class="w-9 h-7 rounded-md mx-auto mb-2 border border-white/10" />
          <span :class="['text-xs', settingsStore.theme === theme.id ? 'text-sky-400' : 'text-gray-500']">{{ theme.label }}</span>
        </button>
      </div>
    </div>
  </div>
</template>
