// moonbit-core/src/domain/common/agent_state_machine.mbt

// Agent 状态机实现
// 权威来源: STATE-MACHINE-CONTRACTS.md
// 基于 server/src/services/agents.ts 的业务逻辑分析

pub fn can_transition_agent(from: AgentStatus, to: AgentStatus) -> Bool {
  match (from, to) {
    // 终止状态不可逆
    (Terminated, _) => false
    
    // 同状态转换无意义 (explicitly list all)
    (Active, Active) => false
    (Paused, Paused) => false
    (Idle, Idle) => false
    (Running, Running) => false
    (Error, Error) => false
    (PendingApproval, PendingApproval) => false
    (Terminated, Terminated) => false
    
    // active 状态转换
    (Active, Paused) => true
    (Active, Error) => true
    (Active, Terminated) => true
    (Active, Idle) => true
    (Active, Running) => true
    
    // paused 状态转换
    (Paused, Idle) => true
    (Paused, Terminated) => true
    
    // idle 状态转换
    (Idle, Running) => true
    (Idle, Paused) => true
    (Idle, Error) => true
    (Idle, Terminated) => true
    
    // running 状态转换
    (Running, Idle) => true
    (Running, Error) => true
    (Running, Paused) => true
    (Running, Terminated) => true
    
    // error 状态转换
    (Error, Idle) => true
    (Error, Paused) => true
    (Error, Terminated) => true
    
    // pending_approval 状态转换
    (PendingApproval, Idle) => true
    (PendingApproval, Terminated) => true
    
    // 其他转换非法
    _ => false
  }
}

pub fn apply_transition_agent(from: AgentStatus, to: AgentStatus) -> Result[AgentStatus, String] {
  if can_transition_agent(from, to) {
    Ok(to)
  } else {
    let from_str = agent_status_to_string(from)
    let to_str = agent_status_to_string(to)
    Err("Cannot transition Agent from " + from_str + " to " + to_str)
  }
}
