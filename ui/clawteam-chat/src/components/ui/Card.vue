<script setup lang="ts">
import { cn } from '@/lib/utils'

interface Props {
  class?: string
}

const props = defineProps<Props>()
</script>

<template>
  <div :class="cn('rounded-lg border bg-white shadow-sm', props.class)">
    <slot />
  </div>
</template>
