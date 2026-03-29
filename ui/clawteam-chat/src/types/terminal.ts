// src/types/terminal.ts
export type TerminalSessionStatus = 'pending' | 'online' | 'working' | 'offline' | 'broken';

export interface TerminalSession {
  id: string;
  memberId?: string;
  workspaceId?: string;
  terminalType: TerminalType;
  status: TerminalSessionStatus;
  cwd?: string;
  createdAt: number;
  lastOutputAt: number;
}

export interface TerminalOutput {
  sessionId: string;
  data: string;
  timestamp: number;
}

export interface TerminalSnapshot {
  sessionId: string;
  lines: string[];
  cursorPosition: { x: number; y: number };
  capturedAt: number;
}

export type TerminalType = 'claude' | 'gemini' | 'codex' | 'opencode' | 'qwen' | 'shell';

export type TerminalConnectionStatus = 'connected' | 'disconnected' | 'reconnecting';
