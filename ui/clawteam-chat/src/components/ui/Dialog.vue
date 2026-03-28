<script setup lang="ts">
import { watch } from 'vue'
import { X } from 'lucide-vue-next'
import { cn } from '@/lib/utils'

interface Props {
  open: boolean
  class?: string
}

const props = defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
}>()

const close = () => emit('update:open', false)

watch(() => props.open, (val) => {
  if (val) {
    document.body.style.overflow = 'hidden'
  } else {
    document.body.style.overflow = ''
  }
})
</script>

<template>
  <Teleport to="body">
    <div v-if="open" class="fixed inset-0 z-50 flex items-center justify-center">
      <div class="fixed inset-0 bg-black/50" @click="close" />
      <div
        :class="cn(
          'relative z-50 w-full max-w-lg rounded-lg bg-white p-6 shadow-lg',
          props.class
        )"
      >
        <button
          class="absolute right-4 top-4 rounded-sm text-gray-400 hover:text-gray-600"
          @click="close"
        >
          <X class="h-4 w-4" />
        </button>
        <slot />
      </div>
    </div>
  </Teleport>
</template>
