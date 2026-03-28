<script setup lang="ts">
import { ref, computed } from 'vue'
import { storeToRefs } from 'pinia'
import { useSkillStore } from '@/stores/skill'
import SkillCard from './SkillCard.vue'
import SkillExecuteModal from './SkillExecuteModal.vue'
import { Input } from '@/components/ui'
import { Search, Package } from 'lucide-vue-next'
import type { Skill } from '@/types/skill'

const skillStore = useSkillStore()
const { skills, skillList } = storeToRefs(skillStore)

const searchQuery = ref('')
const executeModalOpen = ref(false)
const selectedSkill = ref<Skill | null>(null)

const filteredSkills = computed(() => {
  const q = searchQuery.value.toLowerCase()
  if (!q) return skillList.value.map(id => skills.value[id]).filter(Boolean)
  return skillList.value
    .map(id => skills.value[id])
    .filter(s => s.name.toLowerCase().includes(q) ||
      s.description.toLowerCase().includes(q) ||
      s.tags.some(t => t.toLowerCase().includes(q))
    )
})
</script>

<template>
  <div class="flex flex-col h-full">
    <div class="p-4 border-b">
      <div class="flex items-center gap-2">
        <Package class="h-5 w-5 text-gray-500" />
        <h2 class="text-lg font-semibold">Skill 商店</h2>
      </div>
      <div class="mt-3 relative">
        <Search class="absolute left-3 top-1/2 -translate-y-1/2 h-4 w-4 text-gray-400" />
        <Input
          v-model="searchQuery"
          placeholder="搜索 Skill..."
          class="pl-9"
        />
      </div>
    </div>

    <div class="flex-1 overflow-y-auto p-4">
      <div class="grid grid-cols-1 gap-3">
        <SkillCard
          v-for="skill in filteredSkills"
          :key="skill.id"
          :skill="skill"
          :selected="selectedSkill?.id === skill.id"
          @click="selectedSkill = skill"
        />
      </div>

      <div v-if="filteredSkills.length === 0" class="text-center py-8 text-gray-500">
        未找到匹配的 Skill
      </div>
    </div>

    <div class="p-4 border-t flex justify-between items-center">
      <span class="text-sm text-gray-500">
        {{ filteredSkills.length }} 个 Skill
      </span>
      <button
        class="px-4 py-2 bg-blue-500 text-white rounded-lg text-sm hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed"
        :disabled="!selectedSkill"
        @click="executeModalOpen = true"
      >
        执行
      </button>
    </div>

    <SkillExecuteModal
      :open="executeModalOpen"
      :skill="selectedSkill"
      @update:open="executeModalOpen = $event"
      @execute="(skillId: string, variables: Record<string, string>) => skillStore.executeSkill(skillId, variables)"
    />
  </div>
</template>
