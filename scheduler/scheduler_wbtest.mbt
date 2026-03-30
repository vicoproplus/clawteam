///|
/// Scheduler 模块白盒测试
///
/// 白盒测试可以访问包内部实现
test "CliCallParams internal creation" {
  let params = CliCallParams::{
    cli_tool: "claude",
    agent_prompt: "You are an assistant",
    context: "Help the user",
    variables: {},
  }
  inspect(params.cli_tool, content="claude")
  inspect(params.agent_prompt, content="You are an assistant")
}

///|
test "CliCallResult internal creation" {
  let result = CliCallResult::{
    stdout: "output",
    stderr: "",
    status: 0,
    elapsed: 100,
  }
  inspect(result.stdout, content="output")
}

///|
test "DispatchContext internal creation" {
  let context = DispatchContext::{
    session_id: "session-1",
    user_id: Some("user-1"),
    channel: Some("web"),
    timestamp: 1700000000,
    metadata: {},
  }
  inspect(context.session_id, content="session-1")
}
