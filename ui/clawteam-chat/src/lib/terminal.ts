// src/lib/terminal.ts
import type { TerminalType } from '@/types/terminal'

export interface CreateSessionOptions {
  cwd?: string
  workspaceId?: string
  terminalType?: TerminalType
  memberId?: string
}

export async function createTerminalSession(options?: CreateSessionOptions): Promise<string> {
  const response = await fetch('/api/terminal/sessions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(options ?? {}),
  })
  if (!response.ok) throw new Error('Failed to create terminal session')
  const { id } = await response.json()
  return id
}

export async function closeTerminalSession(sessionId: string): Promise<void> {
  const response = await fetch(`/api/terminal/sessions/${sessionId}`, { method: 'DELETE' })
  if (!response.ok) {
    throw new Error(`Failed to close terminal session: ${response.statusText}`)
  }
}

export async function writeToTerminal(sessionId: string, data: string): Promise<void> {
  const response = await fetch(`/api/terminal/sessions/${sessionId}/input`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ data }),
  })
  if (!response.ok) {
    throw new Error(`Failed to write to terminal: ${response.statusText}`)
  }
}

export async function resizeTerminal(sessionId: string, cols: number, rows: number): Promise<void> {
  const response = await fetch(`/api/terminal/sessions/${sessionId}/resize`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ cols, rows }),
  })
  if (!response.ok) {
    throw new Error(`Failed to resize terminal: ${response.statusText}`)
  }
}

export async function getTerminalOutput(sessionId: string): Promise<string> {
  const response = await fetch(`/api/terminal/sessions/${sessionId}/output`)
  if (!response.ok) return ''
  const { output } = await response.json()
  return output ?? ''
}
