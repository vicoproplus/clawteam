<script setup lang="ts">
import { ref } from 'vue'
import { Send } from 'lucide-vue-next'
import { Button, Input } from './ui'

interface Props {
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  disabled: false,
})

const emit = defineEmits<{
  send: [content: string]
}>()

const input = ref('')

const handleSend = () => {
  if (input.value.trim() && !props.disabled) {
    emit('send', input.value.trim())
    input.value = ''
  }
}

const handleKeyDown = (e: KeyboardEvent) => {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault()
    handleSend()
  }
}
</script>

<template>
  <div class="border-t bg-white p-4">
    <div class="flex gap-2">
      <Input
        v-model="input"
        :disabled="disabled"
        placeholder="输入消息..."
        class="flex-1"
        @keydown="handleKeyDown"
      />
      
      <Button :disabled="disabled || !input.trim()" @click="handleSend">
        <Send class="h-4 w-4" />
      </Button>
    </div>
  </div>
</template>
