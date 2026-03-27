import { createSlice, type PayloadAction } from "@reduxjs/toolkit";
import type { AgentConfig, TerminalSessionStatus } from "@/types/agent";

interface AgentEntry {
  config: AgentConfig;
  status: TerminalSessionStatus;
}

interface AgentState {
  agents: Record<string, AgentEntry>;
  agentList: string[];
  activeAgentId: string | null;
}

const initialState: AgentState = {
  agents: {},
  agentList: [],
  activeAgentId: null,
};

const agentSlice = createSlice({
  name: "agent",
  initialState,
  reducers: {
    setAgents: (state, action: PayloadAction<AgentConfig[]>) => {
      state.agents = {};
      state.agentList = [];
      for (const config of action.payload) {
        state.agents[config.id] = { config, status: "offline" };
        state.agentList.push(config.id);
      }
    },
    updateAgentStatus: (
      state,
      action: PayloadAction<{ id: string; status: TerminalSessionStatus }>
    ) => {
      const agent = state.agents[action.payload.id];
      if (agent) {
        agent.status = action.payload.status;
      }
    },
    setActiveAgent: (state, action: PayloadAction<string | null>) => {
      state.activeAgentId = action.payload;
    },
    addAgent: (state, action: PayloadAction<AgentConfig>) => {
      const config = action.payload;
      if (!state.agents[config.id]) {
        state.agents[config.id] = { config, status: "offline" };
        state.agentList.push(config.id);
      }
    },
    removeAgent: (state, action: PayloadAction<string>) => {
      delete state.agents[action.payload];
      state.agentList = state.agentList.filter((id) => id !== action.payload);
      if (state.activeAgentId === action.payload) {
        state.activeAgentId = null;
      }
    },
  },
});

export const {
  setAgents,
  updateAgentStatus,
  setActiveAgent,
  addAgent,
  removeAgent,
} = agentSlice.actions;

export default agentSlice.reducer;
