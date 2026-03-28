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
  const error = ref<string | null>(null)

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
    error.value = null
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
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
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

  const executeSkill = (skillId: string, variables: Record<string, string>) => {
    const skill = skills.value[skillId]
    if (!skill) return
    console.log('Execute skill:', skill.name, variables)
  }

  return {
    // State
    skills,
    skillList,
    selectedSkillId,
    skillDialogOpen,
    loading,
    error,
    // Getters
    selectedSkill,
    skillOptions,
    // Actions
    loadSkills,
    selectSkill,
    openSkillDialog,
    closeSkillDialog,
    executeSkill,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useSkillStore, import.meta.hot))
}
