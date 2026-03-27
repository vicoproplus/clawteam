import { apiGet, apiPost, apiDelete } from "./client";
import type { AgentConfig, TerminalSessionStatus } from "@/types/agent";

export async function getAgents(): Promise<AgentConfig[]> {
  return apiGet<AgentConfig[]>("/agents");
}

export async function getAgent(id: string): Promise<AgentConfig> {
  return apiGet<AgentConfig>(`/agents/${id}`);
}

export async function createAgent(config: AgentConfig): Promise<AgentConfig> {
  return apiPost<AgentConfig>("/agents", config);
}

export async function updateAgent(config: AgentConfig): Promise<AgentConfig> {
  return apiPost<AgentConfig>(`/agents/${config.id}`, config);
}

export async function deleteAgent(id: string): Promise<void> {
  return apiDelete<void>(`/agents/${id}`);
}

export async function getAgentStatus(id: string): Promise<{ status: TerminalSessionStatus }> {
  return apiGet<{ status: TerminalSessionStatus }>(`/agents/${id}/status`);
}

export async function startAgent(id: string): Promise<void> {
  return apiPost<void>(`/agents/${id}/start`, {});
}

export async function stopAgent(id: string): Promise<void> {
  return apiPost<void>(`/agents/${id}/stop`, {});
}
