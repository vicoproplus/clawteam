// src/stores/project.ts
import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { ProjectData, Member } from '@/types/workspace'

export const useProjectStore = defineStore('project', () => {
  const projectData = ref<ProjectData | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  const members = computed(() => projectData.value?.members ?? [])
  const roadmap = computed(() => projectData.value?.roadmap ?? null)
  const skills = computed(() => projectData.value?.skills?.current ?? [])

  const loadProject = async (workspaceId: string) => {
    loading.value = true
    error.value = null
    try {
      const response = await fetch(`/api/workspaces/${workspaceId}/project`)
      if (!response.ok) throw new Error('Failed to fetch project')
      projectData.value = await response.json()
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    } finally {
      loading.value = false
    }
  }

  const updateMember = async (memberId: string, updates: Partial<Member>) => {
    if (!projectData.value) return false
    const index = projectData.value.members.findIndex(m => m.id === memberId)
    if (index === -1) return false

    try {
      const response = await fetch(`/api/members/${memberId}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(updates),
      })
      if (!response.ok) throw new Error('Failed to update member')

      // API 成功后再更新本地状态
      projectData.value.members[index] = { ...projectData.value.members[index], ...updates }
      return true
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
      return false
    }
  }

  return {
    projectData,
    members,
    roadmap,
    skills,
    loading,
    error,
    loadProject,
    updateMember,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useProjectStore, import.meta.hot))
}
