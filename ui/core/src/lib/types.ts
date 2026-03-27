// Agent types
export type AgentRole = "assistant" | "worker" | "supervisor";
export type TerminalSessionStatus = "pending" | "online" | "working" | "offline" | "broken";

export interface AgentConfig {
  id: string;
  name: string;
  role: AgentRole;
  cliTool: string;
  args: string[];
  env: Record<string, string>;
  cwd?: string;
  autoStart: boolean;
  skills: string[];
  enabled: boolean;
}

export interface AgentState {
  config: AgentConfig;
  status: TerminalSessionStatus;
  lastOutputAt: number;
  workingSince?: number;
}

// Message types
export type MessageStatus = "pending" | "dispatched" | "processing" | "completed" | "failed";
export type SenderType = "user" | "assistant" | "worker" | "supervisor" | "system";

export interface ChatMessage {
  id: string;
  sessionId: string;
  senderType: SenderType;
  senderId: string;
  senderName: string;
  content: string;
  status: MessageStatus;
  createdAt: number;
  linkedAgent?: string;
}

// Session types
export interface ChatSession {
  id: string;
  workspaceId: string;
  title: string;
  createdAt: number;
  updatedAt: number;
  messageCount: number;
}
