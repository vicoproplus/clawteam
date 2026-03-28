<script setup lang="ts">
defineProps<{
  output?: string
}>()
</script>

<template>
  <div class="w-96 border-l bg-gray-900 text-gray-100 p-4 font-mono text-sm overflow-auto">
    <div class="text-gray-400 mb-2">CLI 输出</div>
    <div v-if="output" class="whitespace-pre-wrap">
      {{ output }}
    </div>
    <div v-else class="text-gray-500">
      等待输出...
    </div>
  </div>
</template>
