<script setup lang="ts">
import { Star, RotateCw } from 'lucide-vue-next'
import type { Skill } from '@/types/skill'

defineProps<{ skill: Skill }>()
defineEmits<{ install: [skill: Skill] }>()
</script>

<template>
  <div class="bg-gray-800 border border-gray-700 rounded-xl p-5 transition-all hover:border-gray-600 hover:bg-gray-700/50 hover:-translate-y-0.5 hover:shadow-lg flex gap-4 items-start">
    <div :style="{ background: skill.color }" class="w-12 h-12 min-w-12 rounded-xl flex items-center justify-center text-white">
      <span class="text-xl">{{ skill.icon }}</span>
    </div>
    <div class="flex-1 min-w-0">
      <div class="font-semibold text-sm text-gray-200 mb-1">{{ skill.name }}</div>
      <p class="text-xs text-gray-400 leading-relaxed mb-2.5 line-clamp-2">{{ skill.description }}</p>
      <div class="flex items-center gap-3 flex-wrap">
        <span v-if="skill.tags.includes('official')" class="text-xs px-2 py-0.5 rounded bg-emerald-500/10 text-emerald-400">官方默认</span>
        <div v-if="skill.rating" class="flex items-center gap-1 text-xs text-amber-500">
          <Star class="w-3.5 h-3.5 fill-current" />
          <span class="text-gray-400">{{ skill.rating }}</span>
        </div>
      </div>
    </div>
    <div class="flex items-center gap-2 ml-auto">
      <button @click="$emit('install', skill)"
        class="px-3 py-1.5 rounded-lg text-xs border border-gray-600 text-gray-400 hover:bg-gray-600 hover:text-gray-200 transition-colors flex items-center gap-1.5">
        <RotateCw class="w-3.5 h-3.5" />
        {{ skill.installed ? '重新安装' : '安装' }}
      </button>
    </div>
  </div>
</template>