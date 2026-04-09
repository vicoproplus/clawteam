// moonbit-core/src/domain/common/budget_types.mbt

// Budget & Cost 领域类型 + 函数 (合并以避免跨文件引用问题)

// === Budget Types ===

pub(all) struct BudgetEnforcementInput {
  company_id: String
  agent_id: String
  budget_monthly_cents: Int
  spent_monthly_cents: Int
  current_cost_cents: Int
}

pub(all) struct BudgetEnforcementResult {
  allowed: Bool
  enforcement_level: String
  message: String
  budget_used_percent: Int
}

// === Cost TYPES ===

pub(all) struct CostEvent {
  company_id: String
  agent_id: String
  cost_cents: Int
  occurred_at: String
  event_type: String
}

pub(all) struct CostAggregationRequest {
  company_id: String
  agent_ids: Option[Array[String]]
  start_date: Option[String]
  end_date: Option[String]
}

pub(all) struct CostSummary {
  total_cost_cents: Int
  event_count: Int
  by_agent: Array[(String, Int)]
}

// === Budget Enforcement ===

pub fn check_budget_enforcement(input: BudgetEnforcementInput) -> BudgetEnforcementResult {
  let total_budget = input.budget_monthly_cents
  let total_spent = input.spent_monthly_cents + input.current_cost_cents
  
  let budget_used_percent = if total_budget > 0 {
    (total_spent * 100) / total_budget
  } else {
    0
  }
  
  if total_budget > 0 && budget_used_percent >= 100 {
    BudgetEnforcementResult::{
      allowed: false,
      enforcement_level: "hard_block",
      message: "Budget exhausted. Agent paused until budget is increased.",
      budget_used_percent: budget_used_percent
    }
  } else if total_budget > 0 && budget_used_percent >= 80 {
    BudgetEnforcementResult::{
      allowed: true,
      enforcement_level: "soft_warning",
      message: "Budget usage above 80%. Consider increasing budget.",
      budget_used_percent: budget_used_percent
    }
  } else {
    BudgetEnforcementResult::{
      allowed: true,
      enforcement_level: "none",
      message: "Budget within acceptable limits.",
      budget_used_percent: budget_used_percent
    }
  }
}

// === Cost Aggregation ===

pub fn aggregate_costs(
  events: Array[CostEvent],
  request: CostAggregationRequest
) -> CostSummary {
  let filtered = filter_events(events, request)
  let total_cost_cents = sum_costs(filtered)
  let by_agent = group_by_agent(filtered)
  
  CostSummary::{
    total_cost_cents: total_cost_cents,
    event_count: filtered.length(),
    by_agent: by_agent
  }
}

fn filter_events(events: Array[CostEvent], request: CostAggregationRequest) -> Array[CostEvent] {
  let result: Array[CostEvent] = []
  
  for i = 0; i < events.length(); i = i + 1 {
    let event = events[i]
    
    if event.company_id != request.company_id {
      continue
    }
    
    match request.agent_ids {
      Some(agent_ids) => {
        if !contains_agent(agent_ids, event.agent_id) {
          continue
        }
      }
      None => { let _ = () }
    }
    
    match (request.start_date, request.end_date) {
      (Some(start), Some(end)) => {
        if event.occurred_at < start || event.occurred_at >= end {
          continue
        }
      }
      _ => { let _ = () }
    }
    
    result.push(event)
  }
  
  result
}

fn contains_agent(agent_ids: Array[String], agent_id: String) -> Bool {
  for i = 0; i < agent_ids.length(); i = i + 1 {
    if agent_ids[i] == agent_id {
      return true
    }
  }
  false
}

fn sum_costs(events: Array[CostEvent]) -> Int {
  let mut total = 0
  for i = 0; i < events.length(); i = i + 1 {
    total = total + events[i].cost_cents
  }
  total
}

fn group_by_agent(events: Array[CostEvent]) -> Array[(String, Int)] {
  let result: Array[(String, Int)] = []
  let seen: Array[String] = []
  
  for i = 0; i < events.length(); i = i + 1 {
    let agent_id = events[i].agent_id
    
    if !contains_string(seen, agent_id) {
      seen.push(agent_id)
      let cost = sum_agent_costs(events, agent_id)
      result.push((agent_id, cost))
    }
  }
  
  result
}

fn contains_string(arr: Array[String], s: String) -> Bool {
  for i = 0; i < arr.length(); i = i + 1 {
    if arr[i] == s {
      return true
    }
  }
  false
}

fn sum_agent_costs(events: Array[CostEvent], agent_id: String) -> Int {
  let mut total = 0
  for i = 0; i < events.length(); i = i + 1 {
    if events[i].agent_id == agent_id {
      total = total + events[i].cost_cents
    }
  }
  total
}
