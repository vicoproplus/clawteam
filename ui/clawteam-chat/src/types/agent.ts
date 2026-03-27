export type AgentRole = "assistant" | "worker" | "supervisor";
export type TerminalSessionStatus = "pending" | "online" | "working" | "offline" | "broken";

export interface AgentConfig {
  id: string;
  name: string;
  role: AgentRole;
  cliTool: string;
  args: string[];
  env: Record<string, string>;
  cwd?: string;
  autoStart: boolean;
  skills: string[];
  enabled: boolean;
}

export interface AgentState {
  config: AgentConfig;
  status: TerminalSessionStatus;
  lastOutputAt: number;
  workingSince?: number;
}
