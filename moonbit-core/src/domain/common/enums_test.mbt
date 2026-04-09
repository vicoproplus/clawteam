// moonbit-core/src/domain/common/enums_test.mbt

use ./enums.*

test "company status to_string" {
  inspect(company_status_to_string(Active), content="\"active\"")
  inspect(company_status_to_string(Paused), content="\"paused\"")
  inspect(company_status_to_string(Archived), content="\"archived\"")
}

test "agent status to_string" {
  inspect(agent_status_to_string(Active), content="\"active\"")
  inspect(agent_status_to_string(Paused), content="\"paused\"")
  inspect(agent_status_to_string(Idle), content="\"idle\"")
  inspect(agent_status_to_string(Running), content="\"running\"")
  inspect(agent_status_to_string(Error), content="\"error\"")
  inspect(agent_status_to_string(PendingApproval), content="\"pending_approval\"")
  inspect(agent_status_to_string(Terminated), content="\"terminated\"")
}

test "agent role to_string" {
  inspect(agent_role_to_string(CEO), content="\"ceo\"")
  inspect(agent_role_to_string(Engineer), content="\"engineer\"")
  inspect(agent_role_to_string(QA), content="\"qa\"")
  inspect(agent_role_to_string(General), content="\"general\"")
}

test "agent adapter type to_string" {
  inspect(adapter_type_to_string(Process), content="\"process\"")
  inspect(adapter_type_to_string(ClaudeLocal), content="\"claude_local\"")
  inspect(adapter_type_to_string(OpenCodeLocal), content="\"opencode_local\"")
}

test "issue status to_string" {
  inspect(issue_status_to_string(Backlog), content="\"backlog\"")
  inspect(issue_status_to_string(InProgress), content="\"in_progress\"")
  inspect(issue_status_to_string(InReview), content="\"in_review\"")
}

test "issue priority to_string" {
  inspect(issue_priority_to_string(Critical), content="\"critical\"")
  inspect(issue_priority_to_string(Low), content="\"low\"")
}

test "goal level to_string" {
  inspect(goal_level_to_string(Company), content="\"company\"")
  inspect(goal_level_to_string(Task), content="\"task\"")
}

test "goal status to_string" {
  inspect(goal_status_to_string(Planned), content="\"planned\"")
  inspect(goal_status_to_string(Active), content="\"active\"")
}

test "project status to_string" {
  inspect(project_status_to_string(Backlog), content="\"backlog\"")
  inspect(project_status_to_string(InProgress), content="\"in_progress\"")
}

test "enum equality" {
  assert_eq(Active, Active)
  assert_ne(Active, Paused)
  assert_eq(Idle, Idle)
  assert_ne(Idle, Running)
}
