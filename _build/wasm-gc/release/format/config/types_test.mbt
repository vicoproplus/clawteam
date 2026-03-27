///|
/// 配置类型测试
test "CliToolType equality" {
  let claude = @types.CliToolType::Claude
  let codex = @types.CliToolType::Codex
  inspect(claude == claude, content="true")
  inspect(claude == codex, content="false")
}

///|
test "AgentRole equality" {
  let assistant = @types.AgentRole::Assistant
  let worker = @types.AgentRole::Worker
  inspect(assistant == assistant, content="true")
  inspect(assistant == worker, content="false")
}

///|
test "DispatchStrategy equality" {
  let round_robin = @types.DispatchStrategy::RoundRobin
  let least_loaded = @types.DispatchStrategy::LeastLoaded
  inspect(round_robin == round_robin, content="true")
  inspect(round_robin == least_loaded, content="false")
}

///|
test "CliToolConfig creation" {
  let config = CliToolConfig::{
    tool_type: @types.CliToolType::Claude,
    command: "claude",
    args: [],
    env: {},
    cwd: None,
    ready_patterns: ["╭─"],
    working_patterns: [],
    output_patterns: None,
    timeout: None,
  }
  inspect(config.tool_type == @types.CliToolType::Claude, content="true")
  inspect(config.command, content="claude")
}

///|
test "AgentConfig creation" {
  let config = AgentConfig::{
    id: "agent-001",
    name: "Primary Assistant",
    role: @types.AgentRole::Assistant,
    cli_tool: "claude",
    args: [],
    env: {},
    cwd: None,
    auto_start: true,
    role_config: None,
    skills: [],
    enabled: true,
  }
  inspect(config.id, content="agent-001")
  inspect(config.name, content="Primary Assistant")
  inspect(config.role == @types.AgentRole::Assistant, content="true")
}

///|
test "GatewayConfig creation" {
  let config = GatewayConfig::{ port: 3000, host: "127.0.0.1" }
  inspect(config.port, content="3000")
  inspect(config.host, content="127.0.0.1")
}

///|
test "AuditConfig creation" {
  let config = AuditConfig::{
    enabled: true,
    retention_days: 30,
    log_path: "/var/log/clawteam/audit.jsonl",
  }
  inspect(config.enabled, content="true")
  inspect(config.retention_days, content="30")
}

///|
test "default config is valid" {
  let config = ConfigLoader::default_config("/home/user")
  let result = validate_config(config)
  match result {
    Ok(_) => inspect("valid", content="valid")
    Err(e) => inspect(e.to_message(), content="should not fail")
  }
}

///|
test "config paths priority" {
  let paths = config_paths("/home/user", "/workspace")
  inspect(paths.length(), content="4")
  inspect(paths[0], content="/home/user/.clawteam/clawteam.json")
  inspect(paths[3], content="/workspace/clawteam.json")
}

///|
test "default cli tools" {
  let tools = default_cli_tools()
  inspect(tools.contains("claude"), content="true")
  inspect(tools.contains("gemini"), content="true")
  inspect(tools.contains("codex"), content="true")
  inspect(tools.contains("shell"), content="true")
}

///|
test "default gateway config" {
  let config = default_gateway_config()
  inspect(config.port, content="3000")
  inspect(config.host, content="127.0.0.1")
}

///|
test "validate config with invalid port" {
  // 创建一个端口无效的配置
  let config = ClawTeamConfig::{
    version: "1.0.0",
    gateway: GatewayConfig::{ port: 0, host: "127.0.0.1" },
    workspace: None,
    cli_tools: {},
    agents: [],
    skills: [],
    channels: {},
    audit: AuditConfig::{
      enabled: true,
      retention_days: 30,
      log_path: "/tmp/audit.jsonl",
    },
  }
  let result = validate_config(config)
  match result {
    Ok(_) => inspect("should fail", content="fail")
    Err(e) => inspect(e.to_message().contains("port"), content="true")
  }
}
