import { useAppDispatch, useAppSelector } from "./useAppDispatch";
import {
  setAgents,
  updateAgentStatus,
  setActiveAgent,
  addAgent,
  removeAgent,
} from "@/store/agentSlice";
import type { AgentConfig, TerminalSessionStatus } from "@/types/agent";

export function useAgent() {
  const dispatch = useAppDispatch();
  const agents = useAppSelector((s) => s.agent.agents);
  const agentList = useAppSelector((s) => s.agent.agentList);
  const activeAgentId = useAppSelector((s) => s.agent.activeAgentId);

  return {
    agents,
    agentList,
    activeAgentId,
    activeAgent: activeAgentId ? agents[activeAgentId] : null,
    setAgents: (configs: AgentConfig[]) => dispatch(setAgents(configs)),
    updateStatus: (id: string, status: TerminalSessionStatus) =>
      dispatch(updateAgentStatus({ id, status })),
    setActive: (id: string | null) => dispatch(setActiveAgent(id)),
    add: (config: AgentConfig) => dispatch(addAgent(config)),
    remove: (id: string) => dispatch(removeAgent(id)),
  };
}
