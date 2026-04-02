<script setup lang="ts">
import type { Expert } from '@/types/expert'

const props = defineProps<{ expert: Expert }>()
const emit = defineEmits<{ add: [expert: Expert] }>()

const avatarUrl = `https://picsum.photos/seed/expert${props.expert.id}/112/112.jpg`
</script>

<template>
  <div
    class="bg-white text-gray-800 rounded-xl p-5 pb-4 flex flex-col items-center border border-gray-200 transition-all cursor-pointer hover:-translate-y-1 hover:shadow-lg hover:border-purple-500 group"
    @click="emit('add', expert)"
  >
    <img :src="avatarUrl" :alt="expert.name" class="w-14 h-14 rounded-full border-3 border-gray-100 mb-2.5" />
    <div class="text-sm font-bold text-gray-900 mb-1.5">{{ expert.name }}</div>
    <span class="bg-purple-100 text-purple-700 px-3 py-0.5 rounded-full text-xs font-semibold mb-2">
      {{ expert.role }}
    </span>
    <p class="text-xs text-gray-500 text-center leading-relaxed mb-3.5 flex-1">
      {{ expert.desc }}
    </p>
    <button
      class="w-full py-2 rounded-lg text-xs font-medium bg-gray-100 text-gray-600 border border-gray-200 transition-colors group-hover:bg-purple-500 group-hover:text-white group-hover:border-purple-500"
    >
      {{ expert.isAdded ? '已添加' : '添加专家' }}
    </button>
  </div>
</template>
