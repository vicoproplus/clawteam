import { ref, computed } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { AgentConfig, AgentState, TerminalSessionStatus, AgentRole } from '@/types/agent'

export const useAgentStore = defineStore('agent', () => {
  // State
  const agents = ref<Record<string, AgentState>>({})
  const agentList = ref<string[]>([])
  const selectedAgentId = ref<string | null>(null)
  const loading = ref(false)
  const error = ref<string | null>(null)

  // Getters
  const selectedAgent = computed(() => {
    if (!selectedAgentId.value) return null
    return agents.value[selectedAgentId.value] ?? null
  })

  const groupedByRole = computed(() => {
    const groups: Record<AgentRole, AgentState[]> = {
      assistant: [],
      worker: [],
      supervisor: [],
    }
    for (const id of agentList.value) {
      const agent = agents.value[id]
      if (agent) {
        groups[agent.config.role].push(agent)
      }
    }
    return groups
  })

  // Actions
  const loadAgents = async () => {
    loading.value = true
    error.value = null
    try {
      const response = await fetch('/api/agents')
      if (!response.ok) throw new Error('Failed to fetch agents')
      const configs: AgentConfig[] = await response.json()
      
      agents.value = {}
      agentList.value = []
      
      for (const config of configs) {
        agents.value[config.id] = {
          config,
          status: 'offline',
          lastOutputAt: 0,
        }
        agentList.value.push(config.id)
      }
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
      // 使用默认数据
      const defaultConfig: AgentConfig = {
        id: 'assistant-1',
        name: '主助手',
        role: 'assistant',
        cliTool: 'opencode',
        args: [],
        env: {},
        autoStart: true,
        skills: [],
        enabled: true,
      }
      agents.value = {
        'assistant-1': {
          config: defaultConfig,
          status: 'offline',
          lastOutputAt: 0,
        },
      }
      agentList.value = ['assistant-1']
    } finally {
      loading.value = false
    }
  }

  const updateAgentStatus = (agentId: string, status: TerminalSessionStatus) => {
    const agent = agents.value[agentId]
    if (agent) {
      agent.status = status
      if (status === 'working') {
        agent.workingSince = Date.now()
      } else {
        agent.workingSince = undefined
      }
    }
  }

  const selectAgent = (id: string) => {
    selectedAgentId.value = id
  }

  const updateAgentSession = (agentId: string, sessionId: string) => {
    const agent = agents.value[agentId]
    if (agent) {
      agent.sessionId = sessionId
    }
  }

  return {
    // State
    agents,
    agentList,
    selectedAgentId,
    loading,
    error,
    // Getters
    selectedAgent,
    groupedByRole,
    // Actions
    loadAgents,
    updateAgentStatus,
    selectAgent,
    updateAgentSession,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useAgentStore, import.meta.hot))
}
