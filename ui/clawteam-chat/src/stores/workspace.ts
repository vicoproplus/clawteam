// src/stores/workspace.ts
import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { WorkspaceEntry } from '@/types/workspace'

export const useWorkspaceStore = defineStore('workspace', () => {
  const workspaces = ref<WorkspaceEntry[]>([])
  const currentWorkspaceId = ref<string | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  const currentWorkspace = computed(() => {
    return workspaces.value.find(w => w.id === currentWorkspaceId.value) ?? null
  })

  const loadWorkspaces = async () => {
    loading.value = true
    error.value = null
    try {
      const response = await fetch('/api/workspaces')
      if (!response.ok) throw new Error('Failed to fetch workspaces')
      workspaces.value = await response.json()
      if (workspaces.value.length > 0 && !currentWorkspaceId.value) {
        currentWorkspaceId.value = workspaces.value[0].id
      }
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    } finally {
      loading.value = false
    }
  }

  const selectWorkspace = (id: string) => {
    currentWorkspaceId.value = id
  }

  const addWorkspace = async (path: string) => {
    try {
      const response = await fetch('/api/workspaces', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path }),
      })
      if (!response.ok) throw new Error('Failed to add workspace')
      const workspace = await response.json()
      workspaces.value.push(workspace)
      currentWorkspaceId.value = workspace.id
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    }
  }

  const removeWorkspace = async (id: string) => {
    try {
      await fetch(`/api/workspaces/${id}`, { method: 'DELETE' })
      workspaces.value = workspaces.value.filter(w => w.id !== id)
      if (currentWorkspaceId.value === id) {
        currentWorkspaceId.value = workspaces.value[0]?.id ?? null
      }
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    }
  }

  return {
    workspaces,
    currentWorkspaceId,
    currentWorkspace,
    loading,
    error,
    loadWorkspaces,
    selectWorkspace,
    addWorkspace,
    removeWorkspace,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useWorkspaceStore, import.meta.hot))
}
