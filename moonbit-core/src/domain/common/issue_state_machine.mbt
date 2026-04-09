// moonbit-core/src/domain/common/issue_state_machine.mbt

// Issue 状态机实现
// 权威来源: STATE-MACHINE-CONTRACTS.md
// 关键发现: TypeScript 不验证转换规则，只验证目标状态是否合法
// issues.ts:LINE-44-49: assertTransition 只检查 to 是否在 ALL_ISSUE_STATUSES 中

pub fn can_transition_issue(from: IssueStatus, to: IssueStatus) -> Bool {
  // TypeScript 允许任何 → 任何转换 (除了同状态 = no-op)
  // 但为了语义清晰，我们禁止 terminated/done 状态的转换
  match (from, to) {
    // 终端状态不可逆 (语义约束，TS 未强制)
    (Done, _) => false
    (Cancelled, _) => false
    
    // 同状态转换无意义
    (Backlog, Backlog) => false
    (Todo, Todo) => false
    (InProgress, InProgress) => false
    (InReview, InReview) => false
    (Done, Done) => false
    (Blocked, Blocked) => false
    (Cancelled, Cancelled) => false
    
    // 其他所有转换都允许 (匹配 TS 行为)
    _ => true
  }
}

pub fn apply_transition_issue(from: IssueStatus, to: IssueStatus) -> Result[IssueStatus, String] {
  if can_transition_issue(from, to) {
    Ok(to)
  } else {
    let from_str = issue_status_to_string(from)
    let to_str = issue_status_to_string(to)
    Err("Cannot transition Issue from " + from_str + " to " + to_str)
  }
}
