import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { AuditLogEntry } from '@/types/audit'

export const useAuditStore = defineStore('audit', () => {
  const entries = ref<AuditLogEntry[]>([])
  const loading = ref(false)
  const filter = ref<string>('all')

  const filteredEntries = computed(() => {
    if (filter.value === 'all') return entries.value
    return entries.value.filter(e => e.eventType === filter.value)
  })

  const loadEntries = async () => {
    loading.value = true
    try {
      const response = await fetch('/api/audit')
      if (!response.ok) return
      entries.value = await response.json()
    } catch {
      entries.value = []
    } finally {
      loading.value = false
    }
  }

  const addEntry = (entry: AuditLogEntry) => {
    entries.value.unshift(entry)
  }

  const setFilter = (f: string) => {
    filter.value = f
  }

  return {
    entries,
    loading,
    filter,
    filteredEntries,
    loadEntries,
    addEntry,
    setFilter,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useAuditStore, import.meta.hot))
}
