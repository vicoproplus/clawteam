/// ClawTeam 配置类型定义
/// 
/// 本模块定义所有配置相关的数据结构，包括：
/// - CLI 工具配置
/// - Agent 配置
/// - Skill 配置
/// - 渠道配置
/// - Gateway 配置
/// - 审计配置

///|
/// CLI 工具配置
pub(all) struct CliToolConfig {
  tool_type : @types.CliToolType
  command : String
  args : Array[String]
  env : Map[String, String]
  cwd : String?
  ready_patterns : Array[String]
  working_patterns : Array[String]
  output_patterns : OutputPatterns?
  timeout : Int?
} derive(ToJson, FromJson, Show)

///|
/// 输出解析模式
pub(all) struct OutputPatterns {
  success_pattern : String?
  error_pattern : String?
  progress_pattern : String?
} derive(ToJson, FromJson, Show)

///|
/// 角色特定配置
pub(all) enum RoleConfig {
  Assistant(AssistantConfig)
  Worker(WorkerConfig)
  Supervisor(SupervisorConfig)
} derive(ToJson, FromJson, Show)

///|
/// 助手配置
pub(all) struct AssistantConfig {
  available_workers : Array[String]
  available_supervisors : Array[String]
  dispatch_strategy : @types.DispatchStrategy
} derive(ToJson, FromJson, Show)

///|
/// 员工配置
pub(all) struct WorkerConfig {
  assistant_id : String?
  capabilities : Array[String]
} derive(ToJson, FromJson, Show)

///|
/// 监工配置
pub(all) struct SupervisorConfig {
  assistant_id : String?
  review_criteria : Array[String]
} derive(ToJson, FromJson, Show)

///|
/// Agent 配置
pub(all) struct AgentConfig {
  id : String
  name : String
  role : @types.AgentRole
  cli_tool : String
  args : Array[String]
  env : Map[String, String]
  cwd : String?
  auto_start : Bool
  role_config : RoleConfig?
  skills : Array[String]
  enabled : Bool
} derive(ToJson, FromJson, Show)

///|
/// Skill 变量定义
pub(all) struct SkillVariable {
  name : String
  label : String
  var_type : @types.SkillVarType
  required : Bool
  default : String?
} derive(ToJson, FromJson, Show)

///|
/// Skill 执行模板
pub(all) struct SkillTemplate {
  message_template : String
  variables : Array[SkillVariable]
  timeout_ms : Int?
} derive(ToJson, FromJson, Show)

///|
/// Skill 定义
pub(all) struct Skill {
  id : String
  name : String
  name_key : String
  description : String
  icon : String
  color : String
  bg : String
  ring : String
  version : String
  tags : Array[String]
  applicable_roles : Array[@types.AgentRole]
  template : SkillTemplate
} derive(ToJson, FromJson, Show)

///|
/// 渠道配置
pub(all) struct ChannelConfig {
  enabled : Bool
  app_id : String?
  app_secret : String?
} derive(ToJson, FromJson, Show)

///|
/// 审计配置
pub(all) struct AuditConfig {
  enabled : Bool
  retention_days : Int
  log_path : String
} derive(ToJson, FromJson, Show)

///|
/// Gateway 配置
pub(all) struct GatewayConfig {
  port : Int
  host : String
} derive(ToJson, FromJson, Show)

///|
/// 工作空间配置
pub(all) struct WorkspaceConfig {
  path : String
  name : String
} derive(ToJson, FromJson, Show)

///|
/// ClawTeam 主配置
pub(all) struct ClawTeamConfig {
  version : String
  gateway : GatewayConfig
  workspace : WorkspaceConfig?
  cli_tools : Map[String, CliToolConfig]
  agents : Array[AgentConfig]
  skills : Array[Skill]
  channels : Map[String, ChannelConfig]
  audit : AuditConfig
} derive(ToJson, FromJson, Show)
