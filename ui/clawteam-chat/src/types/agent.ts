// Agent 角色定义
export type AgentRole = "assistant" | "worker" | "supervisor";

// 终端会话状态
export type TerminalSessionStatus = "pending" | "online" | "working" | "offline" | "broken";

// 派发策略
export type DispatchStrategy = "round-robin" | "least-loaded" | "capability-match" | "manual";

// 角色配置
export interface AssistantConfig {
  availableWorkers: string[];
  availableSupervisors: string[];
  dispatchStrategy: DispatchStrategy;
}

export interface WorkerConfig {
  assistantId?: string;
  capabilities: string[];
}

export interface SupervisorConfig {
  assistantId?: string;
  reviewCriteria: string[];
}

export type RoleConfig =
  | { type: "assistant"; config: AssistantConfig }
  | { type: "worker"; config: WorkerConfig }
  | { type: "supervisor"; config: SupervisorConfig };

// Agent 配置
export interface AgentConfig {
  id: string;
  name: string;
  role: AgentRole;
  cliTool: string;
  args: string[];
  env: Record<string, string>;
  cwd?: string;
  autoStart: boolean;
  roleConfig?: RoleConfig;
  skills: string[];
  enabled: boolean;
}

// Agent 运行时状态
export interface AgentState {
  config: AgentConfig;
  status: TerminalSessionStatus;
  lastOutputAt: number;
  workingSince?: number;
  sessionId?: string;
}