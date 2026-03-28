<script setup lang="ts">
import { reactive, watch } from 'vue'
import { Dialog, Input, Select, Button } from '@/components/ui'
import type { Skill, SkillVariable } from '@/types/skill'

interface Props {
  open: boolean
  skill: Skill | null
}

const props = defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
  execute: [skillId: string, variables: Record<string, string>]
}>()

const variables = reactive<Record<string, string>>({})

watch(() => props.skill, (skill) => {
  if (skill) {
    for (const v of skill.template.variables) {
      variables[v.name] = v.default ?? ''
    }
  }
}, { immediate: true })

const handleExecute = () => {
  if (props.skill) {
    emit('execute', props.skill.id, { ...variables })
    emit('update:open', false)
  }
}

const renderVariableLabel = (variable: SkillVariable) => {
  return variable.required ? `${variable.label} *` : variable.label
}
</script>

<template>
  <Dialog :open="open" class="max-w-md" @update:open="emit('update:open', $event)">
    <div v-if="skill" class="space-y-4">
      <div>
        <h2 class="text-lg font-semibold">{{ skill.name }}</h2>
        <p class="text-sm text-gray-500 mt-1">{{ skill.description }}</p>
      </div>

      <div class="space-y-3">
        <div v-for="variable in skill.template.variables" :key="variable.name">
          <label class="block text-sm font-medium text-gray-700 mb-1">
            {{ renderVariableLabel(variable) }}
          </label>
          <Select
            v-if="variable.varType === 'select' && variable.options"
            v-model="variables[variable.name]"
            :placeholder="variable.label"
          >
            <option v-for="opt in variable.options" :key="opt" :value="opt">
              {{ opt }}
            </option>
          </Select>
          <Input
            v-else
            v-model="variables[variable.name]"
            :type="variable.varType === 'number' ? 'number' : 'text'"
            :placeholder="variable.label"
          />
        </div>
      </div>

      <div class="flex justify-end gap-2 pt-4 border-t">
        <Button variant="outline" @click="emit('update:open', false)">取消</Button>
        <Button @click="handleExecute">执行</Button>
      </div>
    </div>
  </Dialog>
</template>
