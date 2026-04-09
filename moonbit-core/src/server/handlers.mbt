// moonbit-core/src/server/handlers.mbt

// JSON-RPC Handlers (简化版，不依赖 domain 包)

// 处理 JSON-RPC 请求
pub fn handle_request(method: String, params_json: String, id: Int) -> String {
  match method {
    "health" => handle_health(id)
    "agent.validate" => handle_agent_validate(params_json, id)
    "agent.transition" => handle_agent_transition(params_json, id)
    "issue.transition" => handle_issue_transition(params_json, id)
    "budget.enforce" => handle_budget_enforce(params_json, id)
    "cost.aggregate" => handle_cost_aggregate(params_json, id)
    _ => format_error(-32601, "Method not found: " + method, id)
  }
}

fn handle_health(id: Int) -> String {
  format_success("{\"status\":\"ok\"}", id)
}

fn handle_agent_validate(params_json: String, id: Int) -> String {
  format_success("{\"valid\":true}", id)
}

fn handle_agent_transition(params_json: String, id: Int) -> String {
  format_success("{\"allowed\":true}", id)
}

fn handle_issue_transition(params_json: String, id: Int) -> String {
  format_success("{\"allowed\":true}", id)
}

fn handle_budget_enforce(params_json: String, id: Int) -> String {
  format_success("{\"allowed\":true,\"level\":\"none\"}", id)
}

fn handle_cost_aggregate(params_json: String, id: Int) -> String {
  format_success("{\"total\":0,\"count\":0}", id)
}

// JSON-RPC 响应格式化

fn format_success(result_json: String, id: Int) -> String {
  "{\"jsonrpc\":\"2.0\",\"result\":" + result_json + ",\"id\":" + id.to_string() + "}"
}

fn format_error(code: Int, message: String, id: Int) -> String {
  "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":" + code.to_string() + ",\"message\":\"" + message + "\"},\"id\":" + id.to_string() + "}"
}
