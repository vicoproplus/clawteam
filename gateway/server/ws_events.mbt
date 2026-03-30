/// WebSocket 事件广播
///
/// 本模块提供 WebSocket 事件广播功能：
/// - 事件订阅管理
/// - 状态同步
/// - 实时推送

///|
/// WebSocket 事件订阅管理器
pub struct WsEventSubscription {
  pub client_id : String
  pub subscriptions : Set[String]
  pub created_at : Int
}

///|
/// WebSocket 事件广播器
pub struct WsEventBroadcaster {
  pub subscriptions : Map[String, WsEventSubscription]
  pub event_queue : @broadcast.Broadcast[WsEvent]
}

///|
/// WebSocket 事件
pub struct WsEvent {
  pub event_type : String
  pub payload : Json
  pub timestamp : Int
}

///|
pub fn WsEventBroadcaster::new() -> WsEventBroadcaster {
  WsEventBroadcaster::{
    subscriptions: {},
    event_queue: @broadcast.Broadcast::new(),
  }
}

///|
/// 添加订阅
pub fn WsEventBroadcaster::subscribe(
  self : WsEventBroadcaster,
  client_id : String,
  event_types : Array[String],
) -> Unit {
  let existing = self.subscriptions.get(client_id)
  let subs = match existing {
    Some(sub) => sub.subscriptions
    None => Set::new()
  }
  for event_type in event_types {
    subs.add(event_type)
  }
  self.subscriptions[client_id] = WsEventSubscription::{
    client_id,
    subscriptions: subs,
    created_at: @clock.epoch.now().ms().to_int(),
  }
}

///|
/// 取消订阅
pub fn WsEventBroadcaster::unsubscribe(
  self : WsEventBroadcaster,
  client_id : String,
  event_types : Array[String],
) -> Unit {
  match self.subscriptions.get(client_id) {
    Some(sub) =>
      for event_type in event_types {
        sub.subscriptions.remove(event_type)
      }
    None => ()
  }
}

///|
/// 移除客户端
pub fn WsEventBroadcaster::remove_client(
  self : WsEventBroadcaster,
  client_id : String,
) -> Unit {
  self.subscriptions.remove(client_id) |> ignore()
}

///|
/// 广播事件
pub fn WsEventBroadcaster::broadcast(
  self : WsEventBroadcaster,
  event_type : String,
  payload : Json,
) -> Unit {
  let event = WsEvent::{
    event_type,
    payload,
    timestamp: @clock.epoch.now().ms().to_int(),
  }
  self.event_queue.put(event)
}

///|
/// 广播 Agent 状态变更
pub fn WsEventBroadcaster::broadcast_agent_status(
  self : WsEventBroadcaster,
  agent_id : String,
  status : String,
  details : Json?,
) -> Unit {
  let payload : Json = {
    "agentId": agent_id.to_json(),
    "status": status.to_json(),
    "details": details,
  }
  self.broadcast("agent.status", payload)
}

///|
/// 广播消息状态变更
pub fn WsEventBroadcaster::broadcast_message_status(
  self : WsEventBroadcaster,
  message_id : String,
  session_id : String,
  status : String,
) -> Unit {
  let payload : Json = {
    "messageId": message_id.to_json(),
    "sessionId": session_id.to_json(),
    "status": status.to_json(),
  }
  self.broadcast("message.status", payload)
}

///|
/// 广播 Skill 执行结果
pub fn WsEventBroadcaster::broadcast_skill_result(
  self : WsEventBroadcaster,
  skill_id : String,
  agent_id : String,
  success : Bool,
  result : Json?,
) -> Unit {
  let payload : Json = {
    "skillId": skill_id.to_json(),
    "agentId": agent_id.to_json(),
    "success": success.to_json(),
    "result": result,
  }
  self.broadcast("skill.result", payload)
}

///|
/// 广播会话创建
pub fn WsEventBroadcaster::broadcast_session_created(
  self : WsEventBroadcaster,
  session_id : String,
  agent_id : String,
) -> Unit {
  let payload : Json = {
    "sessionId": session_id.to_json(),
    "agentId": agent_id.to_json(),
  }
  self.broadcast("session.created", payload)
}

///|
/// 广播会话销毁
pub fn WsEventBroadcaster::broadcast_session_destroyed(
  self : WsEventBroadcaster,
  session_id : String,
) -> Unit {
  let payload : Json = { "sessionId": session_id.to_json() }
  self.broadcast("session.destroyed", payload)
}

///|
/// 获取事件队列（用于 WebSocket 连接读取）
pub fn WsEventBroadcaster::event_queue(
  self : WsEventBroadcaster,
) -> @broadcast.Broadcast[WsEvent] {
  self.event_queue
}

///|
/// 检查客户端是否订阅了特定事件类型
pub fn WsEventBroadcaster::is_subscribed(
  self : WsEventBroadcaster,
  client_id : String,
  event_type : String,
) -> Bool {
  match self.subscriptions.get(client_id) {
    Some(sub) => sub.subscriptions.contains(event_type)
    None => false
  }
}

///|
/// 将 WsEvent 转换为 JSON
pub fn WsEvent::to_json(self : WsEvent) -> Json {
  {
    "type": "evt".to_json(),
    "event": self.event_type.to_json(),
    "payload": self.payload,
    "timestamp": self.timestamp.to_json(),
  }
}

///|
/// 将 WsEvent 转换为 JSON 字符串
pub fn WsEvent::to_string(self : WsEvent) -> String {
  self.to_json().stringify()
}
