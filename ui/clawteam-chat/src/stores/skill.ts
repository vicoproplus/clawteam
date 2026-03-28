import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { Skill } from '@/types/skill'

export const useSkillStore = defineStore('skill', () => {
  // State
  const skills = ref<Record<string, Skill>>({})
  const skillList = ref<string[]>([])
  const selectedSkillId = ref<string | null>(null)
  const skillDialogOpen = ref(false)
  const loading = ref(false)

  // Getters
  const selectedSkill = computed(() => {
    if (!selectedSkillId.value) return null
    return skills.value[selectedSkillId.value] ?? null
  })

  const skillOptions = computed(() => {
    return skillList.value.map(id => ({
      id,
      name: skills.value[id]?.name ?? id,
    }))
  })

  // Actions
  const loadSkills = async () => {
    loading.value = true
    try {
      const response = await fetch('/api/skills')
      if (!response.ok) throw new Error('Failed to fetch skills')
      const items: Skill[] = await response.json()
      
      skills.value = {}
      skillList.value = []
      
      for (const skill of items) {
        skills.value[skill.id] = skill
        skillList.value.push(skill.id)
      }
    } catch {
      // 使用默认空数据
      skills.value = {}
      skillList.value = []
    } finally {
      loading.value = false
    }
  }

  const selectSkill = (id: string | null) => {
    selectedSkillId.value = id
  }

  const openSkillDialog = () => {
    skillDialogOpen.value = true
  }

  const closeSkillDialog = () => {
    skillDialogOpen.value = false
  }

  return {
    // State
    skills,
    skillList,
    selectedSkillId,
    skillDialogOpen,
    loading,
    // Getters
    selectedSkill,
    skillOptions,
    // Actions
    loadSkills,
    selectSkill,
    openSkillDialog,
    closeSkillDialog,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useSkillStore, import.meta.hot))
}
