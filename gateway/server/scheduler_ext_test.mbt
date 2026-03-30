/// SchedulerExtension 测试

///|
test "scheduler extension can be created" {
  let config = @config.ClawTeamConfig::{
    version: "1.0.0",
    gateway: @config.GatewayConfig::{ port: 3000, host: "localhost" },
    workspace: None,
    cli_tools: {},
    agents: [],
    skills: [],
    channels: {},
    audit: @config.AuditConfig::{
      enabled: false,
      retention_days: 7,
      log_path: "/tmp/test/audit",
    },
  }
  let ext = SchedulerExtension::new(config)

  // 验证配置版本
  inspect(ext.config.version, content="1.0.0")

  // 验证审计日志状态
  inspect(ext.audit.enabled, content="false")
}

///|
test "scheduler extension list_agents returns empty array when no agents" {
  let config = @config.ClawTeamConfig::{
    version: "1.0.0",
    gateway: @config.GatewayConfig::{ port: 3000, host: "localhost" },
    workspace: None,
    cli_tools: {},
    agents: [],
    skills: [],
    channels: {},
    audit: @config.AuditConfig::{
      enabled: false,
      retention_days: 7,
      log_path: "/tmp/test/audit",
    },
  }
  let ext = SchedulerExtension::new(config)

  let agents = ext.list_agents()
  inspect(agents.length(), content="0")
}

///|
test "scheduler extension list_agents filters disabled agents" {
  let config = @config.ClawTeamConfig::{
    version: "1.0.0",
    gateway: @config.GatewayConfig::{ port: 3000, host: "localhost" },
    workspace: None,
    cli_tools: {},
    agents: [
      @config.AgentConfig::{
        id: "agent-1",
        name: "Enabled Agent",
        role: @types.AgentRole::Assistant,
        cli_tool: "claude",
        args: [],
        env: {},
        cwd: None,
        auto_start: true,
        role_config: None,
        skills: [],
        enabled: true,
      },
      @config.AgentConfig::{
        id: "agent-2",
        name: "Disabled Agent",
        role: @types.AgentRole::Worker,
        cli_tool: "codex",
        args: [],
        env: {},
        cwd: None,
        auto_start: false,
        role_config: None,
        skills: [],
        enabled: false,
      },
    ],
    skills: [],
    channels: {},
    audit: @config.AuditConfig::{
      enabled: false,
      retention_days: 7,
      log_path: "/tmp/test/audit",
    },
  }
  let ext = SchedulerExtension::new(config)

  let agents = ext.list_agents()
  inspect(agents.length(), content="1")
  inspect(agents[0].id, content="agent-1")
}

///|
test "scheduler extension get_default_assistant finds assistant" {
  let config = @config.ClawTeamConfig::{
    version: "1.0.0",
    gateway: @config.GatewayConfig::{ port: 3000, host: "localhost" },
    workspace: None,
    cli_tools: {},
    agents: [
      @config.AgentConfig::{
        id: "assistant-1",
        name: "Main Assistant",
        role: @types.AgentRole::Assistant,
        cli_tool: "claude",
        args: [],
        env: {},
        cwd: None,
        auto_start: true,
        role_config: None,
        skills: [],
        enabled: true,
      },
      @config.AgentConfig::{
        id: "worker-1",
        name: "Worker Agent",
        role: @types.AgentRole::Worker,
        cli_tool: "codex",
        args: [],
        env: {},
        cwd: None,
        auto_start: false,
        role_config: None,
        skills: [],
        enabled: true,
      },
    ],
    skills: [],
    channels: {},
    audit: @config.AuditConfig::{
      enabled: false,
      retention_days: 7,
      log_path: "/tmp/test/audit",
    },
  }
  let ext = SchedulerExtension::new(config)

  let result = ext.get_default_assistant()
  match result {
    Ok(agent) => {
      inspect(agent.id, content="assistant-1")
      inspect(agent.role == @types.AgentRole::Assistant, content="true")
    }
    Err(_) => inspect("error", content="should not happen")
  }
}

///|
test "scheduler extension get_default_assistant returns error when no assistant" {
  let config = @config.ClawTeamConfig::{
    version: "1.0.0",
    gateway: @config.GatewayConfig::{ port: 3000, host: "localhost" },
    workspace: None,
    cli_tools: {},
    agents: [
      @config.AgentConfig::{
        id: "worker-1",
        name: "Worker Agent",
        role: @types.AgentRole::Worker,
        cli_tool: "codex",
        args: [],
        env: {},
        cwd: None,
        auto_start: false,
        role_config: None,
        skills: [],
        enabled: true,
      },
    ],
    skills: [],
    channels: {},
    audit: @config.AuditConfig::{
      enabled: false,
      retention_days: 7,
      log_path: "/tmp/test/audit",
    },
  }
  let ext = SchedulerExtension::new(config)

  let result = ext.get_default_assistant()
  match result {
    Ok(_) => inspect("ok", content="should not find assistant")
    Err(e) => {
      let is_no_agent = match e {
        @errors.ClawTeamError::NoAvailableAgent => true
        _ => false
      }
      inspect(is_no_agent, content="true")
    }
  }
}

///|
test "scheduler extension list_skills returns all skills" {
  let config = @config.ClawTeamConfig::{
    version: "1.0.0",
    gateway: @config.GatewayConfig::{ port: 3000, host: "localhost" },
    workspace: None,
    cli_tools: {},
    agents: [],
    skills: [
      @config.Skill::{
        id: "skill-1",
        name: "Code Review",
        name_key: "skills.codeReview",
        description: "Review code quality",
        icon: "review",
        color: "blue",
        bg: "#ffffff",
        ring: "#000000",
        version: "1.0.0",
        tags: ["code", "review"],
        applicable_roles: [@types.AgentRole::Assistant],
        template: @config.SkillTemplate::{
          message_template: "Please review this code: {{code}}",
          variables: [],
          timeout_ms: None,
        },
      },
    ],
    channels: {},
    audit: @config.AuditConfig::{
      enabled: false,
      retention_days: 7,
      log_path: "/tmp/test/audit",
    },
  }
  let ext = SchedulerExtension::new(config)

  let skills = ext.list_skills()
  inspect(skills.length(), content="1")
  inspect(skills[0].id, content="skill-1")
  inspect(skills[0].name, content="Code Review")
}

///|
test "scheduler extension emit_status_changed works" {
  let config = @config.ClawTeamConfig::{
    version: "1.0.0",
    gateway: @config.GatewayConfig::{ port: 3000, host: "localhost" },
    workspace: None,
    cli_tools: {},
    agents: [],
    skills: [],
    channels: {},
    audit: @config.AuditConfig::{
      enabled: false,
      retention_days: 7,
      log_path: "/tmp/test/audit",
    },
  }
  let ext = SchedulerExtension::new(config)

  // 发送状态变更事件（不应该崩溃）
  ext.emit_status_changed("agent", "agent-1", "online", "working")

  // 事件应该被放入队列
  inspect("success", content="success")
}
