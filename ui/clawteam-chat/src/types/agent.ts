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

export const CLI_TOOLS = [
  { value: 'claude', label: 'Claude Code' },
  { value: 'codex', label: 'Codex' },
  { value: 'gemini', label: 'Gemini' },
  { value: 'opencode', label: 'OpenCode' },
  { value: 'qwen', label: 'Qwen' },
  { value: 'openclaw', label: 'OpenClaw' },
  { value: 'shell', label: 'Shell' },
] as const

export const ROLE_LABELS: Record<AgentRole, string> = {
  assistant: '助手',
  worker: '员工',
  supervisor: '监工',
}

export const ROLE_COLORS: Record<AgentRole, { bg: string; text: string }> = {
  assistant: { bg: 'bg-purple-50', text: 'text-purple-600' },
  worker: { bg: 'bg-blue-50', text: 'text-blue-600' },
  supervisor: { bg: 'bg-green-50', text: 'text-green-600' },
}

export const STATUS_LABELS: Record<TerminalSessionStatus, string> = {
  pending: '启动中',
  online: '在线',
  working: '执行中',
  offline: '离线',
  broken: '异常',
}

export const STATUS_COLORS: Record<TerminalSessionStatus, string> = {
  pending: 'text-yellow-500',
  online: 'text-green-500',
  working: 'text-blue-500',
  offline: 'text-gray-400',
  broken: 'text-red-500',
}