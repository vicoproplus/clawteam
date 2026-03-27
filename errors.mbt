/// ClawTeam 统一错误类型
pub enum ClawTeamError {
  // 配置错误
  ConfigNotFound(String)
  ConfigInvalid(String)
  AgentNotFound(String)
  SkillNotFound(String)
  CliToolNotFound(String)
  
  // 进程错误
  ProcessStartFailed(String)
  ProcessTimeout(String)
  ProcessCrashed(Int)
  
  // 调度错误
  NoAvailableAgent
  AgentBusy(String)
  DispatchFailed(String)
  
  // 渠道错误
  ChannelError(String)
  WebhookInvalid(String)
  
  // 审计错误
  AuditLogWriteFailed(String)
  
  // 通用错误
  InvalidRole
  TemplateRenderFailed(String)
} derive(ToJson, Show)

impl @json.FromJson for ClawTeamError with from_json(_ : Json) -> ClawTeamError {
  ClawTeamError::DispatchFailed("unknown")
}

pub fn ClawTeamError::to_message(self : ClawTeamError) -> String {
  match self {
    ConfigNotFound(path) => "配置文件未找到: \{path}"
    ConfigInvalid(msg) => "配置文件无效: \{msg}"
    AgentNotFound(id) => "Agent 未找到: \{id}"
    SkillNotFound(id) => "Skill 未找到: \{id}"
    CliToolNotFound(name) => "CLI 工具未找到: \{name}"
    ProcessStartFailed(cmd) => "进程启动失败: \{cmd}"
    ProcessTimeout(timeout) => "进程超时 (\{timeout}ms)"
    ProcessCrashed(code) => "进程崩溃，退出码: \{code}"
    NoAvailableAgent => "无可用 Agent"
    AgentBusy(id) => "Agent 忙碌: \{id}"
    DispatchFailed(msg) => "派发失败: \{msg}"
    ChannelError(msg) => "渠道错误: \{msg}"
    WebhookInvalid(msg) => "Webhook 无效: \{msg}"
    AuditLogWriteFailed(msg) => "审计日志写入失败: \{msg}"
    InvalidRole => "无效的角色类型"
    TemplateRenderFailed(msg) => "模板渲染失败: \{msg}"
  }
}

impl Show for ClawTeamError with output(self : ClawTeamError, logger : Logger) -> Unit {
  logger.write_string(self.to_message())
}
