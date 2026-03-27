import { apiGet, apiPost } from "./client";
import type { ChatMessage } from "@/types/message";
import type { ChatSession } from "@/types/session";

export async function getSessions(): Promise<ChatSession[]> {
  return apiGet<ChatSession[]>("/sessions");
}

export async function getSession(id: string): Promise<ChatSession> {
  return apiGet<ChatSession>(`/sessions/${id}`);
}

export async function createSession(title: string): Promise<ChatSession> {
  return apiPost<ChatSession>("/sessions", { title });
}

export async function getMessages(sessionId: string): Promise<ChatMessage[]> {
  return apiGet<ChatMessage[]>(`/sessions/${sessionId}/messages`);
}

export async function sendMessage(
  sessionId: string,
  content: string
): Promise<ChatMessage> {
  return apiPost<ChatMessage>(`/sessions/${sessionId}/messages`, { content });
}
