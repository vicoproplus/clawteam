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