import { createSlice, type PayloadAction } from "@reduxjs/toolkit";
import type { ChatMessage } from "@/types/message";

interface ChatState {
  currentSessionId: string | null;
  messages: Record<string, ChatMessage[]>;
  inputText: string;
  isLoading: boolean;
}

const initialState: ChatState = {
  currentSessionId: null,
  messages: {},
  inputText: "",
  isLoading: false,
};

const chatSlice = createSlice({
  name: "chat",
  initialState,
  reducers: {
    setCurrentSession: (state, action: PayloadAction<string>) => {
      state.currentSessionId = action.payload;
    },
    setInputText: (state, action: PayloadAction<string>) => {
      state.inputText = action.payload;
    },
    addMessage: (state, action: PayloadAction<ChatMessage>) => {
      const sessionId = action.payload.sessionId;
      if (!state.messages[sessionId]) {
        state.messages[sessionId] = [];
      }
      state.messages[sessionId].push(action.payload);
    },
    updateMessageStatus: (
      state,
      action: PayloadAction<{ id: string; status: ChatMessage["status"] }>
    ) => {
      const sessionId = state.currentSessionId;
      if (sessionId && state.messages[sessionId]) {
        const msg = state.messages[sessionId].find((m) => m.id === action.payload.id);
        if (msg) {
          msg.status = action.payload.status;
        }
      }
    },
    setLoading: (state, action: PayloadAction<boolean>) => {
      state.isLoading = action.payload;
    },
    clearMessages: (state, action: PayloadAction<string>) => {
      state.messages[action.payload] = [];
    },
  },
});

export const {
  setCurrentSession,
  setInputText,
  addMessage,
  updateMessageStatus,
  setLoading,
  clearMessages,
} = chatSlice.actions;

export default chatSlice.reducer;
