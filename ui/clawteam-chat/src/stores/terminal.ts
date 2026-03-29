// src/stores/terminal.ts
import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { TerminalSession, TerminalSessionStatus } from '@/types/terminal'

export type LayoutMode = 'single' | 'split-vertical' | 'split-horizontal' | 'grid'

export const useTerminalStore = defineStore('terminal', () => {
  const sessions = ref<Record<string, TerminalSession>>({})
  const sessionOrder = ref<string[]>([])
  const activeSessionId = ref<string | null>(null)
  const layoutMode = ref<LayoutMode>('single')
  const outputs = ref<Record<string, string>>({})

  const activeSession = computed(() => {
    return activeSessionId.value ? sessions.value[activeSessionId.value] ?? null : null
  })

  const sessionsList = computed(() => {
    return sessionOrder.value.map(id => sessions.value[id]).filter(Boolean)
  })

  const createSession = (session: TerminalSession) => {
    sessions.value[session.id] = session
    if (!sessionOrder.value.includes(session.id)) {
      sessionOrder.value.push(session.id)
    }
    if (!activeSessionId.value) {
      activeSessionId.value = session.id
    }
  }

  const closeSession = (id: string) => {
    delete sessions.value[id]
    sessionOrder.value = sessionOrder.value.filter(sid => sid !== id)
    if (activeSessionId.value === id) {
      activeSessionId.value = sessionOrder.value[0] ?? null
    }
  }

  const updateSessionStatus = (id: string, status: TerminalSessionStatus) => {
    const session = sessions.value[id]
    if (session) {
      session.status = status
    }
  }

  const setActiveSession = (id: string) => {
    if (sessions.value[id]) {
      activeSessionId.value = id
    }
  }

  const appendOutput = (sessionId: string, data: string) => {
    if (!outputs.value[sessionId]) {
      outputs.value[sessionId] = ''
    }
    outputs.value[sessionId] += data
  }

  return {
    sessions,
    sessionOrder,
    activeSessionId,
    activeSession,
    sessionsList,
    layoutMode,
    outputs,
    createSession,
    closeSession,
    updateSessionStatus,
    setActiveSession,
    appendOutput,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useTerminalStore, import.meta.hot))
}
