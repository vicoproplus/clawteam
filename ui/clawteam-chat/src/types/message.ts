// 消息状态
export type MessageStatus = "pending" | "dispatched" | "processing" | "completed" | "failed";

// 消息发送者类型
export type SenderType = "user" | "assistant" | "worker" | "supervisor" | "system";

// 聊天消息
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
  metadata?: Record<string, unknown>;
}

// 会话
export interface Session {
  id: string;
  name: string;
  createdAt: number;
  updatedAt: number;
  messages: ChatMessage[];
}

// 扩展会话类型
export interface Conversation {
  id: string;
  workspaceId: string;
  title: string;
  createdAt: number;
  updatedAt: number;
  unreadCount: number;
  lastMessage?: ChatMessage;
}

// 扩展消息元数据
export interface MessageMetadata {
  skillId?: string;
  linkedAgentId?: string;
  dispatchOutcome?: 'dispatched' | 'queued' | 'duplicate' | 'skipped';
  terminalSessionId?: string;
}