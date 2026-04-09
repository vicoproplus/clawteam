// moonbit-core/src/domain/common/cost_aggregation_test.mbt

// Cost 聚合测试

test "aggregate costs - filter by company" {
  let events: Array[CostEvent] = [
    CostEvent::{ company_id: "c1", agent_id: "a1", cost_cents: 100, occurred_at: "2026-04-01T00:00:00Z", event_type: "api_call" },
    CostEvent::{ company_id: "c2", agent_id: "a1", cost_cents: 200, occurred_at: "2026-04-01T00:00:00Z", event_type: "api_call" },
  ]
  let request: CostAggregationRequest = {
    company_id: "c1",
    agent_ids: None,
    start_date: None,
    end_date: None
  }
  let result = aggregate_costs(events, request)
  inspect(result.total_cost_cents, content="100")
  inspect(result.event_count, content="1")
}

test "aggregate costs - filter by agent_ids" {
  let events: Array[CostEvent] = [
    CostEvent::{ company_id: "c1", agent_id: "a1", cost_cents: 100, occurred_at: "2026-04-01T00:00:00Z", event_type: "api_call" },
    CostEvent::{ company_id: "c1", agent_id: "a2", cost_cents: 200, occurred_at: "2026-04-01T00:00:00Z", event_type: "api_call" },
  ]
  let request: CostAggregationRequest = {
    company_id: "c1",
    agent_ids: Some(["a1"]),
    start_date: None,
    end_date: None
  }
  let result = aggregate_costs(events, request)
  inspect(result.total_cost_cents, content="100")
  inspect(result.event_count, content="1")
}

test "aggregate costs - sum multiple events" {
  let events: Array[CostEvent] = [
    CostEvent::{ company_id: "c1", agent_id: "a1", cost_cents: 100, occurred_at: "2026-04-01T00:00:00Z", event_type: "api_call" },
    CostEvent::{ company_id: "c1", agent_id: "a1", cost_cents: 150, occurred_at: "2026-04-01T01:00:00Z", event_type: "api_call" },
    CostEvent::{ company_id: "c1", agent_id: "a1", cost_cents: 50, occurred_at: "2026-04-01T02:00:00Z", event_type: "api_call" },
  ]
  let request: CostAggregationRequest = {
    company_id: "c1",
    agent_ids: None,
    start_date: None,
    end_date: None
  }
  let result = aggregate_costs(events, request)
  inspect(result.total_cost_cents, content="300")
  inspect(result.event_count, content="3")
}

test "aggregate costs - group by agent" {
  let events: Array[CostEvent] = [
    CostEvent::{ company_id: "c1", agent_id: "a1", cost_cents: 100, occurred_at: "2026-04-01T00:00:00Z", event_type: "api_call" },
    CostEvent::{ company_id: "c1", agent_id: "a2", cost_cents: 200, occurred_at: "2026-04-01T00:00:00Z", event_type: "api_call" },
    CostEvent::{ company_id: "c1", agent_id: "a1", cost_cents: 50, occurred_at: "2026-04-01T01:00:00Z", event_type: "api_call" },
  ]
  let request: CostAggregationRequest = {
    company_id: "c1",
    agent_ids: None,
    start_date: None,
    end_date: None
  }
  let result = aggregate_costs(events, request)
  inspect(result.total_cost_cents, content="350")
  inspect(result.by_agent.length(), content="2")
}
