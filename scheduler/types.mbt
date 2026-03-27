/// Scheduler 类型定义
///
/// 本模块定义调度器相关的数据结构。

///|
/// CLI 调用参数
pub struct CliCallParams {
  pub cli_tool : String
  pub agent_prompt : String
  pub context : String
  pub variables : Map[String, String]
} derive(ToJson, Show)

///|
/// CLI 调用结果
pub struct CliCallResult {
  pub stdout : String
  pub stderr : String
  pub status : Int
  pub elapsed : Int
} derive(ToJson, Show)

///|
/// 派发上下文
pub struct DispatchContext {
  pub session_id : String
  pub user_id : String?
  pub channel : String?
  pub timestamp : Int
  pub metadata : Map[String, String]
} derive(Show)
