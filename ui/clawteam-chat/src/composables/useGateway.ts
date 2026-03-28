import type { AgentConfig } from '@/types/agent'
import type { Skill } from '@/types/skill'

const API_BASE = '/api'

// 获取 Agents 列表
export async function fetchAgents(): Promise<AgentConfig[]> {
  const response = await fetch(`${API_BASE}/agents`)
  if (!response.ok) throw new Error('Failed to fetch agents')
  return response.json()
}

// 获取 Skills 列表
export async function fetchSkills(): Promise<Skill[]> {
  const response = await fetch(`${API_BASE}/skills`)
  if (!response.ok) throw new Error('Failed to fetch skills')
  return response.json()
}

// 发送消息
export async function sendMessage(
  sessionId: string,
  content: string,
  skillId?: string
): Promise<{ messageId: string }> {
  const response = await fetch(`${API_BASE}/sessions/${sessionId}/messages`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ content, skillId }),
  })
  if (!response.ok) throw new Error('Failed to send message')
  return response.json()
}

// 获取会话消息
export async function fetchSessionMessages(sessionId: string): Promise<unknown[]> {
  const response = await fetch(`${API_BASE}/sessions/${sessionId}/messages`)
  if (!response.ok) throw new Error('Failed to fetch messages')
  return response.json()
}

export async function fetchChannelConfigs(): Promise<{
  feishu: Record<string, unknown>
  weixin: Record<string, unknown>
}> {
  const response = await fetch(`${API_BASE}/channels`)
  if (!response.ok) throw new Error('Failed to fetch channel configs')
  return response.json()
}

export async function saveChannelConfigs(configs: {
  feishu: Record<string, unknown>
  weixin: Record<string, unknown>
}): Promise<void> {
  const response = await fetch(`${API_BASE}/channels`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(configs),
  })
  if (!response.ok) throw new Error('Failed to save channel configs')
}

export async function fetchAuditLogs(): Promise<unknown[]> {
  const response = await fetch(`${API_BASE}/audit`)
  if (!response.ok) throw new Error('Failed to fetch audit logs')
  return response.json()
}
