import { configureStore } from "@reduxjs/toolkit";
import chatReducer from "./chatSlice";
import agentReducer from "./agentSlice";

export const store = configureStore({
  reducer: {
    chat: chatReducer,
    agent: agentReducer,
  },
});

export type RootState = ReturnType<typeof store.getState>;
export type AppDispatch = typeof store.dispatch;
