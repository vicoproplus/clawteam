import { useCallback } from "react";
import { useAppDispatch, useAppSelector } from "./useAppDispatch";
import {
  setCurrentSession,
  setInputText,
  addMessage,
  updateMessageStatus,
  setLoading,
} from "@/store/chatSlice";
import type { ChatMessage } from "@/types/message";

export function useChat() {
  const dispatch = useAppDispatch();
  const currentSessionId = useAppSelector((s) => s.chat.currentSessionId);
  const inputText = useAppSelector((s) => s.chat.inputText);
  const isLoading = useAppSelector((s) => s.chat.isLoading);
  const messages = useAppSelector((s) =>
    currentSessionId ? s.chat.messages[currentSessionId] ?? [] : []
  );

  const sendMessage = useCallback(
    async (content: string) => {
      if (!currentSessionId || !content.trim()) return;

      const msg: ChatMessage = {
        id: `${Date.now()}-${Math.random().toString(36).slice(2, 9)}`,
        sessionId: currentSessionId,
        senderType: "user",
        senderId: "user",
        senderName: "用户",
        content: content.trim(),
        status: "pending",
        createdAt: Date.now(),
      };

      dispatch(addMessage(msg));
      dispatch(setInputText(""));
      dispatch(setLoading(true));

      // TODO: 实际发送到后端
      // 这里模拟一个回复
      setTimeout(() => {
        const replyMsg: ChatMessage = {
          id: `${Date.now()}-${Math.random().toString(36).slice(2, 9)}`,
          sessionId: currentSessionId,
          senderType: "assistant",
          senderId: "assistant",
          senderName: "助手",
          content: "收到消息，正在处理中...",
          status: "completed",
          createdAt: Date.now(),
        };
        dispatch(addMessage(replyMsg));
        dispatch(setLoading(false));
      }, 1000);
    },
    [currentSessionId, dispatch]
  );

  return {
    currentSessionId,
    inputText,
    isLoading,
    messages,
    setCurrentSession: (id: string) => dispatch(setCurrentSession(id)),
    setInputText: (text: string) => dispatch(setInputText(text)),
    sendMessage,
    updateMessageStatus: (id: string, status: ChatMessage["status"]) =>
      dispatch(updateMessageStatus({ id, status })),
  };
}
