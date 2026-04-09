// moonbit-core/src/domain/common/budget_enforcement_test.mbt

// Budget 执行测试

test "budget enforcement - not configured" {
  let input: BudgetEnforcementInput = {
    company_id: "company-1",
    agent_id: "agent-1",
    budget_monthly_cents: 0,
    spent_monthly_cents: 0,
    current_cost_cents: 100
  }
  let result = check_budget_enforcement(input)
  inspect(result.allowed, content="true")
  inspect(result.enforcement_level, content="none")
}

test "budget enforcement - within budget (50%)" {
  let input: BudgetEnforcementInput = {
    company_id: "company-1",
    agent_id: "agent-1",
    budget_monthly_cents: 10000,
    spent_monthly_cents: 4000,
    current_cost_cents: 1000
  }
  let result = check_budget_enforcement(input)
  inspect(result.allowed, content="true")
  inspect(result.enforcement_level, content="none")
  inspect(result.budget_used_percent, content="50")
}

test "budget enforcement - soft warning (85%)" {
  let input: BudgetEnforcementInput = {
    company_id: "company-1",
    agent_id: "agent-1",
    budget_monthly_cents: 10000,
    spent_monthly_cents: 7500,
    current_cost_cents: 1000
  }
  let result = check_budget_enforcement(input)
  inspect(result.allowed, content="true")
  inspect(result.enforcement_level, content="soft_warning")
  inspect(result.budget_used_percent, content="85")
}

test "budget enforcement - hard block (100%)" {
  let input: BudgetEnforcementInput = {
    company_id: "company-1",
    agent_id: "agent-1",
    budget_monthly_cents: 10000,
    spent_monthly_cents: 9000,
    current_cost_cents: 1000
  }
  let result = check_budget_enforcement(input)
  inspect(result.allowed, content="false")
  inspect(result.enforcement_level, content="hard_block")
  inspect(result.budget_used_percent, content="100")
}

test "budget enforcement - over budget (110%)" {
  let input: BudgetEnforcementInput = {
    company_id: "company-1",
    agent_id: "agent-1",
    budget_monthly_cents: 10000,
    spent_monthly_cents: 10000,
    current_cost_cents: 1000
  }
  let result = check_budget_enforcement(input)
  inspect(result.allowed, content="false")
  inspect(result.enforcement_level, content="hard_block")
  inspect(result.budget_used_percent, content="110")
}
