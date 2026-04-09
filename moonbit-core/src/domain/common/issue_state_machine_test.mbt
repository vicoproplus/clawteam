// moonbit-core/src/domain/common/issue_state_machine_test.mbt

// Issue 状态机测试
// 权威来源: STATE-MACHINE-CONTRACTS.md
// 关键: TS 允许任何 → 任何转换 (除了同状态)

test "issue backlog to todo" {
  inspect(can_transition_issue(Backlog, Todo), content="true")
}

test "issue backlog to cancelled" {
  inspect(can_transition_issue(Backlog, Cancelled), content="true")
}

test "issue todo to in_progress" {
  inspect(can_transition_issue(Todo, InProgress), content="true")
}

test "issue todo to blocked" {
  inspect(can_transition_issue(Todo, Blocked), content="true")
}

test "issue todo to cancelled" {
  inspect(can_transition_issue(Todo, Cancelled), content="true")
}

test "issue in_progress to in_review" {
  inspect(can_transition_issue(InProgress, InReview), content="true")
}

test "issue in_progress to blocked" {
  inspect(can_transition_issue(InProgress, Blocked), content="true")
}

test "issue in_progress to todo" {
  inspect(can_transition_issue(InProgress, Todo), content="true")
}

test "issue in_progress to cancelled" {
  inspect(can_transition_issue(InProgress, Cancelled), content="true")
}

test "issue in_review to done" {
  inspect(can_transition_issue(InReview, Done), content="true")
}

test "issue in_review to in_progress" {
  inspect(can_transition_issue(InReview, InProgress), content="true")
}

test "issue in_review to todo" {
  inspect(can_transition_issue(InReview, Todo), content="true")
}

test "issue in_review to blocked" {
  inspect(can_transition_issue(InReview, Blocked), content="true")
}

test "issue blocked to todo" {
  inspect(can_transition_issue(Blocked, Todo), content="true")
}

test "issue blocked to cancelled" {
  inspect(can_transition_issue(Blocked, Cancelled), content="true")
}

test "issue done to anything is false" {
  inspect(can_transition_issue(Done, Backlog), content="false")
  inspect(can_transition_issue(Done, Todo), content="false")
  inspect(can_transition_issue(Done, InProgress), content="false")
  inspect(can_transition_issue(Done, InReview), content="false")
  inspect(can_transition_issue(Done, Blocked), content="false")
  inspect(can_transition_issue(Done, Cancelled), content="false")
}

test "issue cancelled to anything is false" {
  inspect(can_transition_issue(Cancelled, Backlog), content="false")
  inspect(can_transition_issue(Cancelled, Todo), content="false")
  inspect(can_transition_issue(Cancelled, InProgress), content="false")
  inspect(can_transition_issue(Cancelled, InReview), content="false")
  inspect(can_transition_issue(Cancelled, Done), content="false")
  inspect(can_transition_issue(Cancelled, Blocked), content="false")
}

test "issue same status to same status is false" {
  inspect(can_transition_issue(Backlog, Backlog), content="false")
  inspect(can_transition_issue(Todo, Todo), content="false")
  inspect(can_transition_issue(InProgress, InProgress), content="false")
  inspect(can_transition_issue(InReview, InReview), content="false")
  inspect(can_transition_issue(Blocked, Blocked), content="false")
}

test "issue apply_transition returns error for invalid transition" {
  let result = apply_transition_issue(Done, Todo)
  match result {
    Ok(_) => inspect("should not succeed", content="failed")
    Err(msg) => inspect(msg.has_prefix("Cannot transition"), content="true")
  }
}

test "issue apply_transition returns success for valid transition" {
  let result = apply_transition_issue(Backlog, Todo)
  match result {
    Ok(status) => inspect(issue_status_to_string(status), content="todo")
    Err(_) => inspect("should succeed", content="failed")
  }
}
