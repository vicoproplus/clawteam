/// Logger 类型定义
///
/// 本模块定义审计日志相关的数据结构。

///|
/// 审计日志条目
pub struct AuditLogEntry {
  pub id : String
  pub timestamp : Int
  pub event_type : @types.AuditEventType
  pub session_id : String?
  pub target_id : String?
  pub job_id : String?
  pub run_id : String?
  pub user_id : String?
  pub channel : String?
  pub details : Json
} derive(ToJson, FromJson, Show)
