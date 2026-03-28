import { describe, it, expect, beforeEach } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useAgentStore } from '../agent'

describe('Agent Store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
  })

  it('should initialize with empty agents', () => {
    const store = useAgentStore()
    expect(store.agentList).toEqual([])
    expect(store.agents).toEqual({})
  })

  it('should select an agent', () => {
    const store = useAgentStore()
    store.selectAgent('test-id')
    expect(store.selectedAgentId).toBe('test-id')
  })

  it('should update agent status', () => {
    const store = useAgentStore()
    // 模拟一个 agent
    store.agents = {
      'agent-1': {
        config: {
          id: 'agent-1',
          name: 'Test Agent',
          role: 'assistant',
          cliTool: 'test',
          args: [],
          env: {},
          autoStart: true,
          skills: [],
          enabled: true,
        },
        status: 'offline',
        lastOutputAt: 0,
      },
    }

    store.updateAgentStatus('agent-1', 'working')
    expect(store.agents['agent-1'].status).toBe('working')
    expect(store.agents['agent-1'].workingSince).toBeDefined()
  })

  it('should return null for selected agent when none selected', () => {
    const store = useAgentStore()
    expect(store.selectedAgent).toBeNull()
  })

  it('should return null for selected agent when id not found', () => {
    const store = useAgentStore()
    store.selectAgent('non-existent')
    expect(store.selectedAgent).toBeNull()
  })

  it('should update agent session', () => {
    const store = useAgentStore()
    store.agents = {
      'agent-1': {
        config: {
          id: 'agent-1',
          name: 'Test Agent',
          role: 'assistant',
          cliTool: 'test',
          args: [],
          env: {},
          autoStart: true,
          skills: [],
          enabled: true,
        },
        status: 'offline',
        lastOutputAt: 0,
      },
    }

    store.updateAgentSession('agent-1', 'session-123')
    expect(store.agents['agent-1'].sessionId).toBe('session-123')
  })

  it('should clear workingSince when status changes from working', () => {
    const store = useAgentStore()
    store.agents = {
      'agent-1': {
        config: {
          id: 'agent-1',
          name: 'Test Agent',
          role: 'assistant',
          cliTool: 'test',
          args: [],
          env: {},
          autoStart: true,
          skills: [],
          enabled: true,
        },
        status: 'working',
        lastOutputAt: 0,
        workingSince: Date.now(),
      },
    }

    store.updateAgentStatus('agent-1', 'online')
    expect(store.agents['agent-1'].workingSince).toBeUndefined()
  })

  it('should group agents by role', () => {
    const store = useAgentStore()
    store.agents = {
      'agent-1': {
        config: {
          id: 'agent-1',
          name: 'Assistant 1',
          role: 'assistant',
          cliTool: 'test',
          args: [],
          env: {},
          autoStart: true,
          skills: [],
          enabled: true,
        },
        status: 'online',
        lastOutputAt: 0,
      },
      'agent-2': {
        config: {
          id: 'agent-2',
          name: 'Worker 1',
          role: 'worker',
          cliTool: 'test',
          args: [],
          env: {},
          autoStart: true,
          skills: [],
          enabled: true,
        },
        status: 'offline',
        lastOutputAt: 0,
      },
    }
    store.agentList = ['agent-1', 'agent-2']

    const groups = store.groupedByRole
    expect(groups.assistant).toHaveLength(1)
    expect(groups.worker).toHaveLength(1)
    expect(groups.supervisor).toHaveLength(0)
  })

  it('should add a new agent', () => {
    const store = useAgentStore()
    store.addAgent({
      id: 'worker-1',
      name: '测试员工',
      role: 'worker',
      cliTool: 'codex',
      args: [],
      env: {},
      autoStart: false,
      skills: [],
      enabled: true,
    })
    expect(store.agents['worker-1']).toBeDefined()
    expect(store.agents['worker-1'].config.name).toBe('测试员工')
    expect(store.agentList).toContain('worker-1')
  })

  it('should update an agent config', () => {
    const store = useAgentStore()
    store.addAgent({
      id: 'agent-x',
      name: 'Original',
      role: 'assistant',
      cliTool: 'opencode',
      args: [],
      env: {},
      autoStart: false,
      skills: [],
      enabled: true,
    })
    store.updateAgent('agent-x', { name: 'Updated', cliTool: 'claude' })
    expect(store.agents['agent-x'].config.name).toBe('Updated')
    expect(store.agents['agent-x'].config.cliTool).toBe('claude')
  })

  it('should remove an agent', () => {
    const store = useAgentStore()
    store.addAgent({
      id: 'to-remove',
      name: 'Remove Me',
      role: 'worker',
      cliTool: 'opencode',
      args: [],
      env: {},
      autoStart: false,
      skills: [],
      enabled: true,
    })
    store.selectAgent('to-remove')
    store.removeAgent('to-remove')
    expect(store.agents['to-remove']).toBeUndefined()
    expect(store.agentList).not.toContain('to-remove')
    expect(store.selectedAgentId).toBeNull()
  })
})
