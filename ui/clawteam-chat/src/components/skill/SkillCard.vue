<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'
import { Code } from 'lucide-vue-next'
import type { Skill } from '@/types/skill'

interface Props {
  skill: Skill
  selected?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  selected: false,
})

const emit = defineEmits<{ click: [] }>()

const cardClasses = computed(() =>
  cn(
    'p-3 rounded-lg border cursor-pointer transition-all',
    props.selected
      ? 'bg-blue-50 border-blue-300 ring-2 ring-blue-200'
      : 'bg-white border-gray-200 hover:border-gray-300 hover:shadow-sm'
  )
)
</script>

<template>
  <div :class="cardClasses" @click="emit('click')">
    <div class="flex items-center gap-3">
      <div
        :class="cn('w-10 h-10 rounded-lg flex items-center justify-center', skill.bg)"
        :style="{ color: skill.color }"
      >
        <Code class="h-5 w-5" />
      </div>
      <div class="flex-1 min-w-0">
        <div class="font-medium text-sm truncate">{{ skill.name }}</div>
        <div class="text-xs text-gray-500 truncate">{{ skill.description }}</div>
      </div>
    </div>
    <div class="mt-2 flex gap-1">
      <span
        v-for="tag in skill.tags.slice(0, 2)"
        :key="tag"
        class="text-xs px-1.5 py-0.5 rounded bg-gray-100 text-gray-600"
      >
        {{ tag }}
      </span>
    </div>
  </div>
</template>
