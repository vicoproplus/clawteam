<script setup lang="ts">
import { ref } from 'vue'
import { Send, ChevronDown } from 'lucide-vue-next'
import { Button, Input } from './ui'
import { useSkillStore } from '@/stores/skill'
import { storeToRefs } from 'pinia'

interface Props {
  disabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  disabled: false,
})

const emit = defineEmits<{
  send: [content: string, skillId?: string]
}>()

const skillStore = useSkillStore()
const { skillOptions } = storeToRefs(skillStore)

const input = ref('')
const showSkillMenu = ref(false)
const selectedSkillId = ref<string | null>(null)

const handleSend = () => {
  if (input.value.trim() && !props.disabled) {
    emit('send', input.value.trim(), selectedSkillId.value ?? undefined)
    input.value = ''
    selectedSkillId.value = null
  }
}

const handleKeyDown = (e: KeyboardEvent) => {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault()
    handleSend()
  }
}

const selectSkill = (id: string) => {
  selectedSkillId.value = id
  showSkillMenu.value = false
}

const selectedSkillName = () => {
  if (!selectedSkillId.value) return '选择 Skill'
  return skillOptions.value.find(s => s.id === selectedSkillId.value)?.name ?? '选择 Skill'
}
</script>

<template>
  <div class="border-t bg-white p-4">
    <div class="flex gap-2">
      <div class="relative">
        <Button
          variant="outline"
          size="sm"
          :disabled="disabled"
          @click="showSkillMenu = !showSkillMenu"
        >
          {{ selectedSkillName() }}
          <ChevronDown class="h-4 w-4 ml-1" />
        </Button>
        
        <div
          v-if="showSkillMenu"
          class="absolute bottom-full left-0 mb-1 w-48 bg-white border rounded-lg shadow-lg z-10"
        >
          <button
            v-for="skill in skillOptions"
            :key="skill.id"
            class="w-full px-3 py-2 text-left text-sm hover:bg-gray-50"
            @click="selectSkill(skill.id)"
          >
            {{ skill.name }}
          </button>
          <div v-if="skillOptions.length === 0" class="px-3 py-2 text-sm text-gray-400">
            暂无可用 Skill
          </div>
        </div>
      </div>
      
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
