/// Agent 路由器
///
/// 本模块负责 Agent 的查找和路由。

///|
/// Agent 路由器
pub struct AgentRouter {
  config : @config.ClawTeamConfig
}

///|
pub fn AgentRouter::new(config : @config.ClawTeamConfig) -> AgentRouter {
  AgentRouter::{ config }
}

///|
/// 获取 Agent 配置
pub fn AgentRouter::get_agent(self : AgentRouter, agent_id : String) -> Result[@config.AgentConfig, @errors.ClawTeamError] {
  for agent in self.config.agents {
    if agent.id == agent_id {
      return Ok(agent)
    }
  }
  Err(@errors.ClawTeamError::AgentNotFound(agent_id))
}

///|
/// 获取默认助手
pub fn AgentRouter::get_default_assistant(self : AgentRouter) -> Result[@config.AgentConfig, @errors.ClawTeamError] {
  for agent in self.config.agents {
    if agent.role == @types.AgentRole::Assistant && agent.enabled {
      return Ok(agent)
    }
  }
  Err(@errors.ClawTeamError::NoAvailableAgent)
}

///|
/// 获取可用员工列表
pub fn AgentRouter::get_available_workers(self : AgentRouter, _assistant_id : String) -> Array[String] {
  self.config.agents.iter()
    .filter(fn(agent) { agent.role == @types.AgentRole::Worker && agent.enabled })
    .map(fn(agent) { agent.id })
    .to_array()
}
