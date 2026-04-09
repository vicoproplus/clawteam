// moonbit-core/src/domain/common/agent_state_machine_test.mbt

// Agent 状态机测试
// 权威来源: STATE-MACHINE-CONTRACTS.md

test "agent active to paused" {
  inspect(can_transition_agent(Active, Paused), content="true")
}

test "agent active to terminated" {
  inspect(can_transition_agent(Active, Terminated), content="true")
}

test "agent paused to idle" {
  inspect(can_transition_agent(Paused, Idle), content="true")
}

test "agent paused to terminated" {
  inspect(can_transition_agent(Paused, Terminated), content="true")
}

test "agent idle to running" {
  inspect(can_transition_agent(Idle, Running), content="true")
}

test "agent idle to paused" {
  inspect(can_transition_agent(Idle, Paused), content="true")
}

test "agent idle to terminated" {
  inspect(can_transition_agent(Idle, Terminated), content="true")
}

test "agent running to idle" {
  inspect(can_transition_agent(Running, Idle), content="true")
}

test "agent running to error" {
  inspect(can_transition_agent(Running, Error), content="true")
}

test "agent running to paused" {
  inspect(can_transition_agent(Running, Paused), content="true")
}

test "agent running to terminated" {
  inspect(can_transition_agent(Running, Terminated), content="true")
}

test "agent error to idle" {
  inspect(can_transition_agent(Error, Idle), content="true")
}

test "agent error to paused" {
  inspect(can_transition_agent(Error, Paused), content="true")
}

test "agent error to terminated" {
  inspect(can_transition_agent(Error, Terminated), content="true")
}

test "agent pending_approval to idle" {
  inspect(can_transition_agent(PendingApproval, Idle), content="true")
}

test "agent pending_approval to terminated" {
  inspect(can_transition_agent(PendingApproval, Terminated), content="true")
}

// 无效转换测试

test "agent terminated to anything is false" {
  inspect(can_transition_agent(Terminated, Active), content="false")
  inspect(can_transition_agent(Terminated, Paused), content="false")
  inspect(can_transition_agent(Terminated, Idle), content="false")
  inspect(can_transition_agent(Terminated, Running), content="false")
  inspect(can_transition_agent(Terminated, Error), content="false")
  inspect(can_transition_agent(Terminated, PendingApproval), content="false")
}

test "agent same status to same status is false" {
  inspect(can_transition_agent(Idle, Idle), content="false")
  inspect(can_transition_agent(Running, Running), content="false")
  inspect(can_transition_agent(Paused, Paused), content="false")
}

test "agent pending_approval to paused is false" {
  inspect(can_transition_agent(PendingApproval, Paused), content="false")
}

test "agent apply_transition returns error for invalid transition" {
  let result = apply_transition_agent(Terminated, Active)
  match result {
    Ok(_) => inspect("should not succeed", content="failed")
    Err(msg) => inspect(msg.has_prefix("Cannot transition"), content="true")
  }
}
