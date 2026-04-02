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

  // 返回数组形式的技能列表，用于商店视图
  const skillsArray = computed(() => {
    return skillList.value.map(id => skills.value[id]).filter(Boolean)
  })

  // 默认技能数据
  const defaultSkills: Skill[] = [
    {
      id: 'code-review',
      name: '代码审查',
      nameKey: 'skills.codeReview',
      description: '自动审查代码质量，检测潜在问题和改进建议',
      icon: '🔍',
      color: '#3B82F6',
      bg: '#1E3A5F',
      ring: '#60A5FA',
      version: '1.0.0',
      tags: ['official', 'quality'],
      applicableRoles: ['assistant', 'worker'],
      template: { messageTemplate: '', variables: [] },
      installed: true,
      rating: 4.8,
    },
    {
      id: 'test-gen',
      name: '测试生成器',
      nameKey: 'skills.testGen',
      description: '自动为代码生成单元测试，提高测试覆盖率',
      icon: '🧪',
      color: '#10B981',
      bg: '#064E3B',
      ring: '#34D399',
      version: '1.0.0',
      tags: ['official', 'testing'],
      applicableRoles: ['assistant', 'worker'],
      template: { messageTemplate: '', variables: [] },
      installed: false,
      rating: 4.5,
    },
    {
      id: 'doc-writer',
      name: '文档生成',
      nameKey: 'skills.docWriter',
      description: '根据代码自动生成 API 文档和使用说明',
      icon: '📝',
      color: '#8B5CF6',
      bg: '#2E1065',
      ring: '#A78BFA',
      version: '1.0.0',
      tags: ['official', 'docs'],
      applicableRoles: ['assistant'],
      template: { messageTemplate: '', variables: [] },
      installed: true,
      rating: 4.6,
    },
    {
      id: 'refactor',
      name: '重构助手',
      nameKey: 'skills.refactor',
      description: '识别代码异味并提供重构建议',
      icon: '🔧',
      color: '#F59E0B',
      bg: '#78350F',
      ring: '#FBBF24',
      version: '1.0.0',
      tags: ['quality'],
      applicableRoles: ['assistant', 'worker'],
      template: { messageTemplate: '', variables: [] },
      installed: false,
      rating: 4.3,
    },
    {
      id: 'security-scan',
      name: '安全扫描',
      nameKey: 'skills.securityScan',
      description: '检测代码中的安全漏洞和风险',
      icon: '🛡️',
      color: '#EF4444',
      bg: '#7F1D1D',
      ring: '#F87171',
      version: '1.0.0',
      tags: ['official', 'security'],
      applicableRoles: ['assistant', 'supervisor'],
      template: { messageTemplate: '', variables: [] },
      installed: false,
      rating: 4.7,
    },
  ]

  // Actions
  const loadSkills = async () => {
    loading.value = true
    error.value = null
    try {
      const response = await fetch('/api/skills')
      if (!response.ok) throw new Error('Failed to fetch skills')
      const items: Skill[] = await response.json()
      
      // 如果 API 返回空数组，使用默认数据
      const skillsToUse = items.length > 0 ? items : defaultSkills
      
      skills.value = {}
      skillList.value = []
      
      for (const skill of skillsToUse) {
        skills.value[skill.id] = skill
        skillList.value.push(skill.id)
      }
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
      // 使用默认数据
      skills.value = {}
      skillList.value = []
      for (const skill of defaultSkills) {
        skills.value[skill.id] = skill
        skillList.value.push(skill.id)
      }
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
    skillsArray,
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
