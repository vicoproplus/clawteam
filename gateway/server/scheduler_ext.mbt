/// Scheduler 扩展（挂载到 Gateway）
///
/// 本模块提供 Scheduler 与 Gateway 的集成：
/// - Agent 路由
/// - Skill 执行
/// - 审计日志
/// - WebSocket 事件广播

///|
/// Scheduler 扩展（挂载到 Gateway）
pub struct SchedulerExtension {
  pub config : @config.ClawTeamConfig
  pub router : @scheduler.AgentRouter
  pub audit : @logger.AuditStore
  pub events : @broadcast.Broadcast[SchedulerEvent]
}

///|
/// Scheduler 事件类型
pub enum SchedulerEvent {
  AgentDispatched(AgentDispatchedEvent)
  SkillExecuted(SkillExecutedEvent)
  MessageReceived(MessageReceivedEvent)
  StatusChanged(StatusChangedEvent)
}

///|
/// Agent 派发事件
pub struct AgentDispatchedEvent {
  pub session_id : String
  pub agent_id : String
  pub target_id : String?
  pub timestamp : Int
}

///|
/// Skill 执行事件
pub struct SkillExecutedEvent {
  pub skill_id : String
  pub agent_id : String
  pub success : Bool
  pub elapsed_ms : Int
  pub timestamp : Int
}

///|
/// 消息接收事件
pub struct MessageReceivedEvent {
  pub session_id : String
  pub channel : String?
  pub content_preview : String
  pub timestamp : Int
}

///|
/// 状态变更事件
pub struct StatusChangedEvent {
  pub entity_type : String
  pub entity_id : String
  pub old_status : String
  pub new_status : String
  pub timestamp : Int
}

///|
pub fn SchedulerExtension::new(
  config : @config.ClawTeamConfig,
) -> SchedulerExtension {
  SchedulerExtension::{
    config,
    router: @scheduler.AgentRouter::new(config),
    audit: @logger.AuditStore::new(config.audit),
    events: @broadcast.Broadcast::new(),
  }
}

///|
/// 获取默认助手
pub fn SchedulerExtension::get_default_assistant(
  self : SchedulerExtension,
) -> Result[@config.AgentConfig, @errors.ClawTeamError] {
  self.router.get_default_assistant()
}

///|
/// 获取 Agent 配置
pub fn SchedulerExtension::get_agent(
  self : SchedulerExtension,
  agent_id : String,
) -> Result[@config.AgentConfig, @errors.ClawTeamError] {
  self.router.get_agent(agent_id)
}

///|
/// 获取所有启用的 Agent 列表
pub fn SchedulerExtension::list_agents(
  self : SchedulerExtension,
) -> Array[AgentInfo] {
  self.config.agents
  .iter()
  .filter(fn(agent) { agent.enabled })
  .map(fn(agent) {
    AgentInfo::{
      id: agent.id,
      name: agent.name,
      role: agent.role,
      cli_tool: agent.cli_tool,
      enabled: agent.enabled,
      skills: agent.skills,
    }
  })
  .to_array()
}

///|
/// 获取所有 Skill 列表
pub fn SchedulerExtension::list_skills(
  self : SchedulerExtension,
) -> Array[SkillInfo] {
  self.config.skills
  .iter()
  .map(fn(skill) {
    SkillInfo::{
      id: skill.id,
      name: skill.name,
      name_key: skill.name_key,
      description: skill.description,
      icon: skill.icon,
      color: skill.color,
      tags: skill.tags,
      applicable_roles: skill.applicable_roles,
    }
  })
  .to_array()
}

///|
/// 获取可用员工列表
pub fn SchedulerExtension::get_available_workers(
  self : SchedulerExtension,
  assistant_id : String,
) -> Array[String] {
  self.router.get_available_workers(assistant_id)
}

///|
/// 记录消息发送审计日志
pub fn SchedulerExtension::log_message_sent(
  self : SchedulerExtension,
  session_id : String,
  content : String,
) -> Result[String, @errors.ClawTeamError] {
  let preview = if content.length() > 100 {
    content[0:100].to_string() + "..."
  } else {
    content.to_string()
  }
  let details : Json = { "event": "message_sent", "preview": preview }
  self.audit.write(
    @types.AuditEventType::OutputCaptured,
    details,
    session_id=Some(session_id),
  )
}

///|
/// 发送状态变更事件
pub fn SchedulerExtension::emit_status_changed(
  self : SchedulerExtension,
  entity_type : String,
  entity_id : String,
  old_status : String,
  new_status : String,
) -> Unit {
  let event = StatusChangedEvent::{
    entity_type,
    entity_id,
    old_status,
    new_status,
    timestamp: @clock.epoch.now().ms().to_int(),
  }
  self.events.put(SchedulerEvent::StatusChanged(event))
}

///|
/// 发送 Agent 派发事件
pub fn SchedulerExtension::emit_agent_dispatched(
  self : SchedulerExtension,
  session_id : String,
  agent_id : String,
  target_id : String?,
) -> Unit {
  let event = AgentDispatchedEvent::{
    session_id,
    agent_id,
    target_id,
    timestamp: @clock.epoch.now().ms().to_int(),
  }
  self.events.put(SchedulerEvent::AgentDispatched(event))
}

///|
/// Agent 信息（用于 API 返回）
pub struct AgentInfo {
  pub id : String
  pub name : String
  pub role : @types.AgentRole
  pub cli_tool : String
  pub enabled : Bool
  pub skills : Array[String]
} derive(ToJson, Show)

///|
/// Skill 信息（用于 API 返回）
pub struct SkillInfo {
  pub id : String
  pub name : String
  pub name_key : String
  pub description : String
  pub icon : String
  pub color : String
  pub tags : Array[String]
  pub applicable_roles : Array[@types.AgentRole]
} derive(ToJson, Show)

///|
/// 将 AgentInfo 转换为 JSON
pub fn AgentInfo::to_json(self : AgentInfo) -> Json {
  {
    "id": self.id.to_json(),
    "name": self.name.to_json(),
    "role": self.role.to_json(),
    "cliTool": self.cli_tool.to_json(),
    "enabled": self.enabled.to_json(),
    "skills": self.skills.to_json(),
  }
}

///|
/// 将 SkillInfo 转换为 JSON
pub fn SkillInfo::to_json(self : SkillInfo) -> Json {
  {
    "id": self.id.to_json(),
    "name": self.name.to_json(),
    "nameKey": self.name_key.to_json(),
    "description": self.description.to_json(),
    "icon": self.icon.to_json(),
    "color": self.color.to_json(),
    "tags": self.tags.to_json(),
    "applicableRoles": self.applicable_roles.to_json(),
  }
}
