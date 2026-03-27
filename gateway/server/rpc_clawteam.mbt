/// RPC 扩展：ClawTeam 专用方法
///
/// 本模块提供 ClawTeam 特有的 RPC 方法：
/// - agents.list - 获取 Agent 列表
/// - skills.list - 获取 Skill 列表
/// - messages.send - 发送消息

///|
/// RPC: 获取 Agent 列表
pub fn handle_agents_list(scheduler_ext : SchedulerExtension) -> Json {
  let agents = scheduler_ext.list_agents()
  let items : Array[Json] = agents.map(fn(a) { a.to_json() })
  { "agents": items.to_json() }
}

///|
/// RPC: 获取 Skill 列表
pub fn handle_skills_list(scheduler_ext : SchedulerExtension) -> Json {
  let skills = scheduler_ext.list_skills()
  let items : Array[Json] = skills.map(fn(s) { s.to_json() })
  { "skills": items.to_json() }
}

///|
/// RPC: 获取单个 Agent 详情
pub fn handle_agent_get(
  scheduler_ext : SchedulerExtension,
  agent_id : String,
) -> Result[Json, @errors.ClawTeamError] {
  match scheduler_ext.get_agent(agent_id) {
    Ok(agent) => {
      let info = AgentInfo::{
        id: agent.id,
        name: agent.name,
        role: agent.role,
        cli_tool: agent.cli_tool,
        enabled: agent.enabled,
        skills: agent.skills,
      }
      Ok(info.to_json())
    }
    Err(e) => Err(e)
  }
}

///|
/// RPC: 获取默认助手
pub fn handle_agent_default(
  scheduler_ext : SchedulerExtension,
) -> Result[Json, @errors.ClawTeamError] {
  match scheduler_ext.get_default_assistant() {
    Ok(agent) => {
      let info = AgentInfo::{
        id: agent.id,
        name: agent.name,
        role: agent.role,
        cli_tool: agent.cli_tool,
        enabled: agent.enabled,
        skills: agent.skills,
      }
      Ok(info.to_json())
    }
    Err(e) => Err(e)
  }
}

///|
/// RPC: 获取可用员工列表
pub fn handle_workers_list(
  scheduler_ext : SchedulerExtension,
  assistant_id : String,
) -> Json {
  let workers = scheduler_ext.get_available_workers(assistant_id)
  { "workers": workers.to_json() }
}

///|
/// RPC: 发送消息
pub fn handle_message_send(
  scheduler_ext : SchedulerExtension,
  session_id : String,
  content : String,
) -> Result[Json, @errors.ClawTeamError] {
  // 记录审计日志
  match scheduler_ext.log_message_sent(session_id, content) {
    Ok(_) => {
      // 生成消息 ID
      let message_id = "msg-\{generate_uuid()}"
      
      // 发送事件
      scheduler_ext.emit_agent_dispatched(
        session_id,
        "default",  // TODO: 根据路由策略选择 Agent
        None,
      )
      
      Ok({
        "messageId": message_id.to_json(),
        "status": "dispatched".to_json(),
        "timestamp": @clock.epoch.now().ms().to_int().to_json(),
      })
    }
    Err(e) => Err(e)
  }
}

///|
/// RPC: 订阅事件
pub fn handle_events_subscribe(
  broadcaster : WsEventBroadcaster,
  client_id : String,
  event_types : Array[String],
) -> Json {
  broadcaster.subscribe(client_id, event_types)
  { 
    "subscribed": true.to_json(),
    "eventTypes": event_types.to_json(),
  }
}

///|
/// RPC: 取消订阅事件
pub fn handle_events_unsubscribe(
  broadcaster : WsEventBroadcaster,
  client_id : String,
  event_types : Array[String],
) -> Json {
  broadcaster.unsubscribe(client_id, event_types)
  { 
    "unsubscribed": true.to_json(),
    "eventTypes": event_types.to_json(),
  }
}

///|
/// 生成 UUID
fn generate_uuid() -> String {
  let rand_gen = try {
    @rand.chacha8()
  } catch {
    _ => @random.Rand::chacha8(seed=Bytes::make(32, 0))
  }
  let gen = @uuid.generator(rand_gen)
  gen.v4().to_string()
}

///|
/// 构建 RPC 响应（成功）
pub fn rpc_success(id : String, payload : Json) -> Json {
  {
    "type": "res".to_json(),
    "id": id.to_json(),
    "ok": true.to_json(),
    "payload": payload,
  }
}

///|
/// 构建 RPC 响应（失败）
pub fn rpc_error(id : String, code : String, message : String) -> Json {
  {
    "type": "res".to_json(),
    "id": id.to_json(),
    "ok": false.to_json(),
    "error": {
      "code": code.to_json(),
      "message": message.to_json(),
    }.to_json(),
  }
}
