// moonbit-core/src/domain/common/enums.mbt

// === Company ===

// TS: COMPANY_STATUSES = ["active", "paused", "archived"]
pub enum CompanyStatus {
  Active
  Paused
  Archived
}

pub fn company_status_to_string(s: CompanyStatus) -> String {
  match s {
    Active => "active"
    Paused => "paused"
    Archived => "archived"
  }
}

// === Agent ===

// TS: AGENT_STATUSES = ["active", "paused", "idle", "running", "error", "pending_approval", "terminated"]
// 注意: "active" 是独立枚举值 (从 STATE-MACHINE-CONTRACTS.md 调研结果)
pub enum AgentStatus {
  Active
  Paused
  Idle
  Running
  Error
  PendingApproval
  Terminated
}

pub fn agent_status_to_string(s: AgentStatus) -> String {
  match s {
    Active => "active"
    Paused => "paused"
    Idle => "idle"
    Running => "running"
    Error => "error"
    PendingApproval => "pending_approval"
    Terminated => "terminated"
  }
}

// TS: AGENT_ROLES = ["ceo", "cto", "cmo", "cfo", "engineer", "designer", "pm", "qa", "devops", "researcher", "general"]
pub enum AgentRole {
  CEO
  CTO
  CMO
  CFO
  Engineer
  Designer
  PM
  QA
  DevOps
  Researcher
  General
}

pub fn agent_role_to_string(r: AgentRole) -> String {
  match r {
    CEO => "ceo"
    CTO => "cto"
    CMO => "cmo"
    CFO => "cfo"
    Engineer => "engineer"
    Designer => "designer"
    PM => "pm"
    QA => "qa"
    DevOps => "devops"
    Researcher => "researcher"
    General => "general"
  }
}

// TS: AGENT_ADAPTER_TYPES = ["process", "http", "claude_local", "codex_local", "gemini_local", "opencode_local", "pi_local", "cursor", "openclaw_gateway"]
pub enum AgentAdapterType {
  Process
  Http
  ClaudeLocal
  CodexLocal
  GeminiLocal
  OpenCodeLocal
  PiLocal
  Cursor
  OpenclawGateway
}

pub fn adapter_type_to_string(t: AgentAdapterType) -> String {
  match t {
    Process => "process"
    Http => "http"
    ClaudeLocal => "claude_local"
    CodexLocal => "codex_local"
    GeminiLocal => "gemini_local"
    OpenCodeLocal => "opencode_local"
    PiLocal => "pi_local"
    Cursor => "cursor"
    OpenclawGateway => "openclaw_gateway"
  }
}

// === Issue ===

// TS: ISSUE_STATUSES = ["backlog", "todo", "in_progress", "in_review", "done", "blocked", "cancelled"]
pub enum IssueStatus {
  Backlog
  Todo
  InProgress
  InReview
  Done
  Blocked
  Cancelled
}

pub fn issue_status_to_string(s: IssueStatus) -> String {
  match s {
    Backlog => "backlog"
    Todo => "todo"
    InProgress => "in_progress"
    InReview => "in_review"
    Done => "done"
    Blocked => "blocked"
    Cancelled => "cancelled"
  }
}

// TS: ISSUE_PRIORITIES = ["critical", "high", "medium", "low"]
pub enum IssuePriority {
  Critical
  High
  Medium
  Low
}

pub fn issue_priority_to_string(p: IssuePriority) -> String {
  match p {
    Critical => "critical"
    High => "high"
    Medium => "medium"
    Low => "low"
  }
}

// === Goal ===

// TS: GOAL_LEVELS = ["company", "team", "agent", "task"]
pub enum GoalLevel {
  Company
  Team
  Agent
  Task
}

pub fn goal_level_to_string(l: GoalLevel) -> String {
  match l {
    Company => "company"
    Team => "team"
    Agent => "agent"
    Task => "task"
  }
}

// TS: GOAL_STATUSES = ["planned", "active", "achieved", "cancelled"]
pub enum GoalStatus {
  Planned
  Active
  Achieved
  Cancelled
}

pub fn goal_status_to_string(s: GoalStatus) -> String {
  match s {
    Planned => "planned"
    Active => "active"
    Achieved => "achieved"
    Cancelled => "cancelled"
  }
}

// === Project ===

// TS: PROJECT_STATUSES = ["backlog", "planned", "in_progress", "completed", "cancelled"]
pub enum ProjectStatus {
  Backlog
  Planned
  InProgress
  Completed
  Cancelled
}

pub fn project_status_to_string(s: ProjectStatus) -> String {
  match s {
    Backlog => "backlog"
    Planned => "planned"
    InProgress => "in_progress"
    Completed => "completed"
    Cancelled => "cancelled"
  }
}
