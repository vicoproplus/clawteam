<script setup lang="ts">
import { ref, provide, computed } from 'vue'

interface Props {
  defaultValue?: string
  modelValue?: string
}

const props = withDefaults(defineProps<Props>(), {
  defaultValue: '',
  modelValue: undefined,
})

const emit = defineEmits<{
  'update:modelValue': [value: string]
}>()

const internalValue = ref(props.defaultValue)
const activeTab = computed(() => props.modelValue ?? internalValue.value)

const setActiveTab = (value: string) => {
  internalValue.value = value
  emit('update:modelValue', value)
}

provide('tabs-active', activeTab)
provide('tabs-set-active', setActiveTab)
</script>

<template>
  <div>
    <slot />
  </div>
</template>
