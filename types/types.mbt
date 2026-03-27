/// ClawTeam 基础类型定义
/// 
/// 本模块定义所有共享的基础类型，包括：
/// - CLI 工具类型
/// - Agent 角色
/// - 终端会话状态
/// - 派发策略
/// - Skill 变量类型
/// - 消息状态
/// - 审计事件类型

///|
/// 支持的 CLI 工具类型
pub(all) enum CliToolType {
  Claude
  Codex
  Gemini
  OpenCode
  Qwen
  OpenClaw
  Shell
} derive(ToJson, FromJson, Show, Eq)

///|
/// Agent 角色定义
pub(all) enum AgentRole {
  Assistant // 助手：理解需求、决策、分配任务（必须，默认）
  Worker // 员工：执行任务、反馈给助手
  Supervisor // 监工：核查任务完成情况、反馈给助手
} derive(ToJson, FromJson, Show, Eq)

///|
/// 终端会话状态
pub(all) enum TerminalSessionStatus {
  Pending // 创建中
  Online // 就绪，等待输入
  Working // 执行中
  Offline // 已退出
  Broken // 启动失败
} derive(ToJson, FromJson, Show, Eq)

///|
/// 派发策略
pub(all) enum DispatchStrategy {
  RoundRobin // 轮询
  LeastLoaded // 最少负载
  CapabilityMatch // 能力匹配
  Manual // 手动指定
} derive(ToJson, FromJson, Show, Eq)

///|
/// Skill 变量类型
pub(all) enum SkillVarType {
  Text
  Number
  File
  Directory
  Select(Array[String])
} derive(ToJson, FromJson, Show)

///|
/// 消息状态
pub(all) enum MessageStatus {
  Pending
  Dispatched
  Processing
  Completed
  Failed
} derive(ToJson, FromJson, Show, Eq)

///|
/// 审计事件类型
pub(all) enum AuditEventType {
  // 调度审计
  JobTriggered
  JobCompleted
  // 进程审计
  CliStarted
  CliCompleted
  CliFailed
  CliKilled
  // 输出审计
  OutputCaptured
  // 干预审计
  UserPaused
  UserResumed
  UserCancelled
  // 异常审计
  ProcessTimeout
  OutputError
} derive(ToJson, FromJson, Show)
