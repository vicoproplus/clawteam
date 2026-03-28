import { ref, onUnmounted } from 'vue'
import { useChatStore } from '@/stores/chat'
import { useAgentStore } from '@/stores/agent'

interface WebSocketMessage {
  type: 'message' | 'status' | 'output' | 'agent_status'
  payload: unknown
}

export function useWebSocket(url: string) {
  const wsRef = ref<WebSocket | null>(null)
  const connected = ref(false)
  const chatStore = useChatStore()
  const agentStore = useAgentStore()

  const handleMessage = (data: WebSocketMessage) => {
    switch (data.type) {
      case 'message':
        chatStore.addMessage(data.payload as Parameters<typeof chatStore.addMessage>[0])
        break
      case 'status':
        chatStore.updateMessageStatus(
          (data.payload as { messageId: string; status: string }).messageId,
          (data.payload as { messageId: string; status: string }).status as Parameters<typeof chatStore.updateMessageStatus>[1]
        )
        break
      case 'agent_status':
        agentStore.updateAgentStatus(
          (data.payload as { agentId: string; status: string }).agentId,
          (data.payload as { agentId: string; status: string }).status as Parameters<typeof agentStore.updateAgentStatus>[1]
        )
        break
    }
  }

  const connect = () => {
    wsRef.value = new WebSocket(url)

    wsRef.value.onopen = () => {
      console.log('WebSocket connected')
      connected.value = true
    }

    wsRef.value.onmessage = (event) => {
      try {
        const data: WebSocketMessage = JSON.parse(event.data)
        handleMessage(data)
      } catch (e) {
        console.error('Failed to parse WebSocket message:', e)
      }
    }

    wsRef.value.onclose = () => {
      console.log('WebSocket disconnected, reconnecting...')
      connected.value = false
      setTimeout(connect, 3000)
    }

    wsRef.value.onerror = (error) => {
      console.error('WebSocket error:', error)
    }
  }

  const disconnect = () => {
    wsRef.value?.close()
    wsRef.value = null
    connected.value = false
  }

  const send = (message: unknown) => {
    if (wsRef.value?.readyState === WebSocket.OPEN) {
      wsRef.value.send(JSON.stringify(message))
    }
  }

  // 自动连接
  connect()

  // 自动断开
  onUnmounted(() => {
    disconnect()
  })

  return {
    connected,
    send,
    connect,
    disconnect,
  }
}
