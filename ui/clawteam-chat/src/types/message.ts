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
