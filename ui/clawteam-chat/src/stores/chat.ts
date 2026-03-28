import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { ChatMessage, Session, MessageStatus } from '@/types/message'

export const useChatStore = defineStore('chat', () => {
  // State
  const sessions = ref<Record<string, Session>>({})
  const currentSessionId = ref<string | null>(null)
  const messages = ref<Record<string, ChatMessage[]>>({})
  const pendingMessages = ref<string[]>([])

  // Getters
  const currentSession = computed(() => {
    if (!currentSessionId.value) return null
    return sessions.value[currentSessionId.value] ?? null
  })

  const currentMessages = computed(() => {
    const id = currentSessionId.value
    if (!id) return []
    return messages.value[id] ?? []
  })

  // Actions
  const createSession = (session: Session) => {
    sessions.value[session.id] = session
    // 同步 session.messages 到 messages ref
    if (session.messages && session.messages.length > 0) {
      messages.value[session.id] = [...session.messages]
    } else if (!messages.value[session.id]) {
      messages.value[session.id] = []
    }
    if (!currentSessionId.value) {
      currentSessionId.value = session.id
    }
  }

  const setCurrentSession = (id: string) => {
    currentSessionId.value = id
  }

  const addMessage = (message: ChatMessage) => {
    const sessionId = message.sessionId
    if (!messages.value[sessionId]) {
      messages.value[sessionId] = []
    }
    messages.value[sessionId].push(message)
    if (sessions.value[sessionId]) {
      sessions.value[sessionId].updatedAt = Date.now()
    }
  }

  const updateMessageStatus = (messageId: string, status: MessageStatus) => {
    for (const sessionId in messages.value) {
      const msg = messages.value[sessionId].find(m => m.id === messageId)
      if (msg) {
        msg.status = status
        break
      }
    }
  }

  const addPendingMessage = (id: string) => {
    pendingMessages.value.push(id)
  }

  const removePendingMessage = (id: string) => {
    pendingMessages.value = pendingMessages.value.filter(i => i !== id)
  }

  return {
    // State
    sessions,
    currentSessionId,
    messages,
    pendingMessages,
    // Getters
    currentSession,
    currentMessages,
    // Actions
    createSession,
    setCurrentSession,
    addMessage,
    updateMessageStatus,
    addPendingMessage,
    removePendingMessage,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useChatStore, import.meta.hot))
}
