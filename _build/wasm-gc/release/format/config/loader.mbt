/// ClawTeam 配置加载器
/// 
/// 本模块提供配置文件加载功能，包括：
/// - 配置文件路径解析
/// - 默认配置生成
/// - 配置合并策略

///|
/// 配置加载器
pub struct ConfigLoader {}

///|
/// 创建新的配置加载器
pub fn ConfigLoader::new() -> ConfigLoader {
  ConfigLoader::{  }
}

///|
/// 配置文件路径优先级
/// 
/// 按优先级从高到低返回可能的配置文件路径：
/// 1. ~/.clawteam/clawteam.json
/// 2. ~/.clawteam.json
/// 3. <cwd>/.clawteam/clawteam.json
/// 4. <cwd>/clawteam.json
pub fn config_paths(home : String, cwd : String) -> Array[String] {
  [
    home + "/.clawteam/clawteam.json",
    home + "/clawteam.json",
    cwd + "/.clawteam/clawteam.json",
    cwd + "/clawteam.json",
  ]
}

///|
/// 默认 Gateway 配置
pub fn default_gateway_config() -> GatewayConfig {
  GatewayConfig::{ port: 3000, host: "127.0.0.1" }
}

///|
/// 默认审计配置
pub fn default_audit_config(home : String) -> AuditConfig {
  AuditConfig::{
    enabled: true,
    retention_days: 30,
    log_path: home + "/.clawteam/logs/audit.jsonl",
  }
}

///|
/// 默认 Claude CLI 工具配置
pub fn default_claude_config() -> CliToolConfig {
  CliToolConfig::{
    tool_type: @types.CliToolType::Claude,
    command: "claude",
    args: [],
    env: {},
    cwd: None,
    ready_patterns: ["╭─"],
    working_patterns: ["Thinking...", "Working..."],
    output_patterns: None,
    timeout: None,
  }
}

///|
/// 默认 Gemini CLI 工具配置
pub fn default_gemini_config() -> CliToolConfig {
  CliToolConfig::{
    tool_type: @types.CliToolType::Gemini,
    command: "gemini",
    args: [],
    env: {},
    cwd: None,
    ready_patterns: [">"],
    working_patterns: ["Thinking..."],
    output_patterns: None,
    timeout: None,
  }
}

///|
/// 默认 Codex CLI 工具配置
pub fn default_codex_config() -> CliToolConfig {
  CliToolConfig::{
    tool_type: @types.CliToolType::Codex,
    command: "codex",
    args: [],
    env: {},
    cwd: None,
    ready_patterns: [">"],
    working_patterns: [],
    output_patterns: None,
    timeout: None,
  }
}

///|
/// 默认 Shell 工具配置
pub fn default_shell_config() -> CliToolConfig {
  CliToolConfig::{
    tool_type: @types.CliToolType::Shell,
    command: "bash",
    args: ["-l"],
    env: {},
    cwd: None,
    ready_patterns: ["$"],
    working_patterns: [],
    output_patterns: None,
    timeout: None,
  }
}

///|
/// 默认 CLI 工具集合
pub fn default_cli_tools() -> Map[String, CliToolConfig] {
  {
    "claude": default_claude_config(),
    "gemini": default_gemini_config(),
    "codex": default_codex_config(),
    "shell": default_shell_config(),
  }
}

///|
/// 默认配置
/// 
/// 生成一个包含合理默认值的配置对象
pub fn ConfigLoader::default_config(home : String) -> ClawTeamConfig {
  ClawTeamConfig::{
    version: "1.0.0",
    gateway: default_gateway_config(),
    workspace: None,
    cli_tools: default_cli_tools(),
    agents: [],
    skills: [],
    channels: {},
    audit: default_audit_config(home),
  }
}

///|
/// 验证配置有效性
pub fn validate_config(
  config : ClawTeamConfig,
) -> Result[ClawTeamConfig, @errors.ClawTeamError] {
  // 验证 Gateway 端口
  if config.gateway.port < 1 || config.gateway.port > 65535 {
    return Err(
      @errors.ClawTeamError::ConfigInvalid(
        "Gateway port must be between 1 and 65535",
      ),
    )
  }

  // 验证 Gateway host
  if config.gateway.host.length() == 0 {
    return Err(
      @errors.ClawTeamError::ConfigInvalid("Gateway host cannot be empty"),
    )
  }

  // 验证 Agent ID 唯一性
  let agent_ids : Array[String] = []
  for agent in config.agents {
    if agent_ids.contains(agent.id) {
      return Err(
        @errors.ClawTeamError::ConfigInvalid("Duplicate agent ID: \{agent.id}"),
      )
    }
    agent_ids.push(agent.id)
  }

  // 验证 Skill ID 唯一性
  let skill_ids : Array[String] = []
  for skill in config.skills {
    if skill_ids.contains(skill.id) {
      return Err(
        @errors.ClawTeamError::ConfigInvalid("Duplicate skill ID: \{skill.id}"),
      )
    }
    skill_ids.push(skill.id)
  }

  Ok(config)
}
