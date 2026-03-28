<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'

interface Props {
  modelValue?: boolean
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  modelValue: false,
  disabled: false,
})

const emit = defineEmits<{
  'update:modelValue': [value: boolean]
}>()

const toggle = () => {
  if (!props.disabled) emit('update:modelValue', !props.modelValue)
}

const trackClasses = computed(() =>
  cn(
    'relative inline-flex h-6 w-11 shrink-0 cursor-pointer rounded-full border-2 border-transparent transition-colors',
    props.modelValue ? 'bg-blue-600' : 'bg-gray-200',
    props.disabled && 'opacity-50 cursor-not-allowed'
  )
)

const thumbClasses = computed(() =>
  cn(
    'pointer-events-none inline-block h-5 w-5 transform rounded-full bg-white shadow ring-0 transition-transform',
    props.modelValue ? 'translate-x-5' : 'translate-x-0'
  )
)
</script>

<template>
  <button type="button" role="switch" :aria-checked="modelValue" :class="trackClasses" @click="toggle">
    <span :class="thumbClasses" />
  </button>
</template>
