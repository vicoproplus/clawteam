/// Logger 审计日志存储
///
/// 本模块提供审计日志的存储和查询功能。

///|
/// 审计日志存储
pub struct AuditStore {
  log_path : String
  enabled : Bool
}

///|
pub fn AuditStore::new(config : @config.AuditConfig) -> AuditStore {
  AuditStore::{
    log_path: config.log_path,
    enabled: config.enabled,
  }
}

///|
/// 生成 UUID
fn generate_id() -> String {
  // 使用 chacha8 随机数生成器，失败时使用默认种子
  let rand_gen = try {
    @rand.chacha8()
  } catch {
    _ => @random.Rand::chacha8(seed=Bytes::make(32, 0))
  }
  let gen = @uuid.generator(rand_gen)
  gen.v4().to_string()
}

///|
/// 写入审计日志
pub fn AuditStore::write(
  self : AuditStore,
  event_type : @types.AuditEventType,
  details : Json,
  session_id~ : String? = None,
  target_id~ : String? = None,
) -> Result[String, @errors.ClawTeamError] {
  if !self.enabled {
    return Ok("")
  }
  
  let id = generate_id()
  let timestamp = @clock.epoch.now().ms().to_int()
  let _entry = AuditLogEntry::{
    id,
    timestamp,
    event_type,
    session_id,
    target_id,
    job_id: None,
    run_id: None,
    user_id: None,
    channel: None,
    details,
  }
  
  // 简化实现：仅返回日志 ID
  // TODO: 实际写入到文件系统
  Ok(id)
}

///|
/// 记录 CLI 启动
pub fn AuditStore::log_cli_started(
  self : AuditStore,
  session_id : String,
  target_id : String,
  command : String,
) -> Result[String, @errors.ClawTeamError] {
  let details : Json = { "command": command.to_json() }
  self.write(@types.AuditEventType::CliStarted, details, session_id=Some(session_id), target_id=Some(target_id))
}

///|
/// 记录 CLI 完成
pub fn AuditStore::log_cli_completed(
  self : AuditStore,
  session_id : String,
  target_id : String,
  status : Int,
) -> Result[String, @errors.ClawTeamError] {
  let details : Json = { "status": status.to_json() }
  self.write(@types.AuditEventType::CliCompleted, details, session_id=Some(session_id), target_id=Some(target_id))
}
