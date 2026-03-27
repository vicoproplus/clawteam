# ClawTeam 后端核心调度层 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 构建 ClawTeam 的核心调度层，包括项目初始化、配置系统、Agent 路由、CLI 工具注册、调度执行器、输出解析器和审计日志。

**架构：** 基于 MoonBit Native，从 MoonClaw 复用 gateway/job/channel/internal 模块，新增 scheduler/parser/logger 模块，通过 spawn 调用外部 CLI 工具。

**技术栈：** MoonBit (native), moonbitlang/async, moonbitlang/x, moonbitlang/regexp, moonbit-community/yaml

---

## 文件结构

### 新建文件

| 文件 | 职责 |
|------|------|
| `moon.mod.json` | 模块定义与依赖 |
| `moon.pkg` | 根包依赖声明 |
| `clawteam.mbt` | 主入口结构 |
| `config/mod.mbt` | 配置模块入口 |
| `config/loader.mbt` | 配置文件加载 |
| `config/types.mbt` | 配置数据结构 |
| `scheduler/mod.mbt` | 调度器模块入口 |
| `scheduler/agent_router.mbt` | Agent 角色路由 |
| `scheduler/dispatcher.mbt` | 调度执行器 |
| `scheduler/template.mbt` | 消息模板填充 |
| `scheduler/prompt.mbt` | Agent Prompt 模板 |
| `parser/mod.mbt` | 解析器模块入口 |
| `parser/regex_parser.mbt` | 正则解析器 |
| `logger/mod.mbt` | 日志模块入口 |
| `logger/audit_store.mbt` | 审计日志存储 |
| `logger/query.mbt` | 日志查询接口 |
| `errors.mbt` | 统一错误类型 |
| `types.mbt` | 核心类型定义 |

### 从 MoonClaw 复制（精简）

| 源路径 | 目标路径 | 修改 |
|--------|----------|------|
| `reference/moonclaw/internal/` | `internal/` | 完全复制 |
| `reference/moonclaw/gateway/` | `gateway/` | 完全复制 |
| `reference/moonclaw/job/` | `job/` | 完全复制 |
| `reference/moonclaw/channel/` | `channel/` | 完全复制 |
| `reference/moonclaw/channels/` | `channels/` | 完全复制 |
| `reference/moonclaw/event/` | `event/` | 完全复制 |
| `reference/moonclaw/clock/` | `clock/` | 完全复制 |
| `reference/moonclaw/cmd/` | `cmd/` | 复制并修改 |
| `reference/moonclaw/acp/` | `acp/` | 精简复制 |
| `reference/moonclaw/model/` | `model/` | 精简复制 |
| `reference/moonclaw/ai/` | `ai/` | 精简复制 |
| `reference/moonclaw/oauth/` | `oauth/` | 精简复制（仅保留 codex） |

---

## 任务分解

### 任务 1：项目初始化与基础结构

**文件：**
- 创建：`moon.mod.json`
- 创建：`moon.pkg`
- 创建：`clawteam.mbt`
- 创建：`errors.mbt`
- 创建：`types.mbt`

- [ ] **步骤 1：创建 moon.mod.json**

```json
{
  "name": "clawteam/clawteam",
  "version": "0.1.0",
  "deps": {
    "moonbitlang/async": "0.16.6",
    "moonbitlang/x": "0.4.40",
    "moonbitlang/regexp": "0.3.5",
    "moonbit-community/yaml": "0.0.3"
  },
  "readme": "README.md",
  "repository": "",
  "license": "Apache-2.0",
  "keywords": ["agent", "cli", "scheduler"],
  "description": "ClawTeam - Multi-Agent CLI Orchestrator",
  "preferred-target": "native"
}
```

- [ ] **步骤 2：创建根 moon.pkg**

```moonbit
import {
  "clawteam/clawteam/config",
  "clawteam/clawteam/scheduler",
  "clawteam/clawteam/parser",
  "clawteam/clawteam/logger",
  "clawteam/clawteam/gateway/server",
  "clawteam/clawteam/job",
  "clawteam/clawteam/channel",
  "clawteam/clawteam/acp",
  "clawteam/clawteam/event",
  "clawteam/clawteam/clock",
  "clawteam/clawteam/model",
  "clawteam/clawteam/ai",
  "clawteam/clawteam/internal/pino",
  "clawteam/clawteam/internal/uuid",
  "moonbitlang/core/json",
}
```

- [ ] **步骤 3：创建错误类型 errors.mbt**

```moonbit
/// ClawTeam 统一错误类型
pub enum ClawTeamError {
  // 配置错误
  ConfigNotFound(String)
  ConfigInvalid(String)
  AgentNotFound(String)
  SkillNotFound(String)
  CliToolNotFound(String)
  
  // 进程错误
  ProcessStartFailed(String)
  ProcessTimeout(String)
  ProcessCrashed(Int)
  
  // 调度错误
  NoAvailableAgent
  AgentBusy(String)
  DispatchFailed(String)
  
  // 渠道错误
  ChannelError(String)
  WebhookInvalid(String)
  
  // 审计错误
  AuditLogWriteFailed(String)
  
  // 通用错误
  InvalidRole
  TemplateRenderFailed(String)
} derive(ToJson, Show)

impl @json.FromJson for ClawTeamError with from_json(_ : Json) -> ClawTeamError {
  // 简化实现
  ClawTeamError::DispatchFailed("unknown")
}

pub fn ClawTeamError::to_message(self : ClawTeamError) -> String {
  match self {
    ConfigNotFound(path) => "配置文件未找到: \{path}"
    ConfigInvalid(msg) => "配置文件无效: \{msg}"
    AgentNotFound(id) => "Agent 未找到: \{id}"
    SkillNotFound(id) => "Skill 未找到: \{id}"
    CliToolNotFound(name) => "CLI 工具未找到: \{name}"
    ProcessStartFailed(cmd) => "进程启动失败: \{cmd}"
    ProcessTimeout(timeout) => "进程超时 (\{timeout}ms)"
    ProcessCrashed(code) => "进程崩溃，退出码: \{code}"
    NoAvailableAgent => "无可用 Agent"
    AgentBusy(id) => "Agent 忙碌: \{id}"
    DispatchFailed(msg) => "派发失败: \{msg}"
    ChannelError(msg) => "渠道错误: \{msg}"
    WebhookInvalid(msg) => "Webhook 无效: \{msg}"
    AuditLogWriteFailed(msg) => "审计日志写入失败: \{msg}"
    InvalidRole => "无效的角色类型"
    TemplateRenderFailed(msg) => "模板渲染失败: \{msg}"
  }
}

impl Show for ClawTeamError with output(self : ClawTeamError, logger : Logger) -> Unit {
  logger.write_string(self.to_message())
}
```

- [ ] **步骤 4：创建核心类型 types.mbt**

```moonbit
/// 支持的 CLI 工具类型
pub enum CliToolType {
  Claude
  Codex
  Gemini
  OpenCode
  Qwen
  OpenClaw
  Shell
} derive(ToJson, FromJson, Show, Eq)

/// Agent 角色定义
pub enum AgentRole {
  Assistant    // 助手：理解需求、决策、分配任务（必须，默认）
  Worker       // 员工：执行任务、反馈给助手
  Supervisor   // 监工：核查任务完成情况、反馈给助手
} derive(ToJson, FromJson, Show, Eq)

/// 终端会话状态
pub enum TerminalSessionStatus {
  Pending   // 创建中
  Online    // 就绪，等待输入
  Working   // 执行中
  Offline   // 已退出
  Broken    // 启动失败
} derive(ToJson, FromJson, Show, Eq)

/// 派发策略
pub enum DispatchStrategy {
  RoundRobin      // 轮询
  LeastLoaded     // 最少负载
  CapabilityMatch // 能力匹配
  Manual          // 手动指定
} derive(ToJson, FromJson, Show, Eq)

/// Skill 变量类型
pub enum SkillVarType {
  Text
  Number
  File
  Directory
  Select(Array[String])
} derive(ToJson, FromJson, Show)

/// 消息状态
pub enum MessageStatus {
  Pending
  Dispatched
  Processing
  Completed
  Failed
} derive(ToJson, FromJson, Show, Eq)

/// 审计事件类型
pub enum AuditEventType {
  // 调度审计
  JobTriggered
  JobCompleted
  // 进程审计
  CliStarted
  CliCompleted
  CliFailed
  CliKilled
  // 输出审计
  OutputCaptured
  // 干预审计
  UserPaused
  UserResumed
  UserCancelled
  // 异常审计
  ProcessTimeout
  OutputError
} derive(ToJson, FromJson, Show)
```

- [ ] **步骤 5：创建主入口 clawteam.mbt**

```moonbit
/// ClawTeam 主入口
pub struct ClawTeam {
  logger : @pino.Logger
  config : @config.ClawTeamConfig
  scheduler : @scheduler.Scheduler
}

pub fn ClawTeam::new(
  config : @config.ClawTeamConfig,
  logger : @pino.Logger,
) -> ClawTeam {
  ClawTeam::{
    logger,
    config,
    scheduler: @scheduler.Scheduler::new(config, logger),
  }
}
```

- [ ] **步骤 6：运行 moon check 验证**

运行：`moon check`
预期：通过（可能有未定义模块的警告）

- [ ] **步骤 7：Commit**

```bash
git add moon.mod.json moon.pkg clawteam.mbt errors.mbt types.mbt
git commit -m "feat: initialize project structure with core types"
```

---

### 任务 2：配置系统

**文件：**
- 创建：`config/moon.pkg`
- 创建：`config/mod.mbt`
- 创建：`config/types.mbt`
- 创建：`config/loader.mbt`
- 创建：`config/types_test.mbt`

- [ ] **步骤 1：创建 config/moon.pkg**

```moonbit
import {
  "moonbitlang/core/json",
  "moonbitlang/core/test",
  "moonbitlang/x/path",
  "clawteam/clawteam/errors",
  "clawteam/clawteam/types",
  "clawteam/clawteam/internal/fsx",
  "clawteam/clawteam/internal/pino",
  "moonbit-community/yaml",
}
```

- [ ] **步骤 2：创建配置类型 config/types.mbt**

```moonbit
/// CLI 工具配置
pub struct CliToolConfig {
  pub tool_type : CliToolType
  pub command : String
  pub args : Array[String]
  pub env : Map[String, String]
  pub cwd : String?
  pub ready_patterns : Array[String]
  pub working_patterns : Array[String]
  pub output_patterns : OutputPatterns?
  pub timeout? : Int
} derive(ToJson, FromJson, Show)

/// 输出解析模式
pub struct OutputPatterns {
  pub success_pattern : String?
  pub error_pattern : String?
  pub progress_pattern : String?
} derive(ToJson, FromJson, Show)

/// 角色特定配置
pub enum RoleConfig {
  Assistant(AssistantConfig)
  Worker(WorkerConfig)
  Supervisor(SupervisorConfig)
} derive(ToJson, FromJson, Show)

/// 助手配置
pub struct AssistantConfig {
  pub available_workers : Array[String]
  pub available_supervisors : Array[String]
  pub dispatch_strategy : DispatchStrategy
} derive(ToJson, FromJson, Show)

/// 员工配置
pub struct WorkerConfig {
  pub assistant_id : String?
  pub capabilities : Array[String]
} derive(ToJson, FromJson, Show)

/// 监工配置
pub struct SupervisorConfig {
  pub assistant_id : String?
  pub review_criteria : Array[String]
} derive(ToJson, FromJson, Show)

/// Agent 配置
pub struct AgentConfig {
  pub id : String
  pub name : String
  pub role : AgentRole
  pub cli_tool : String
  pub args : Array[String]
  pub env : Map[String, String]
  pub cwd : String?
  pub auto_start : Bool
  pub role_config : RoleConfig?
  pub skills : Array[String]
  pub enabled : Bool
} derive(ToJson, FromJson, Show)

/// Skill 变量定义
pub struct SkillVariable {
  pub name : String
  pub label : String
  pub var_type : SkillVarType
  pub required : Bool
  pub default : String?
} derive(ToJson, FromJson, Show)

/// Skill 执行模板
pub struct SkillTemplate {
  pub message_template : String
  pub variables : Array[SkillVariable]
  pub timeout_ms : Int?
} derive(ToJson, FromJson, Show)

/// Skill 定义
pub struct Skill {
  pub id : String
  pub name : String
  pub name_key : String
  pub description : String
  pub icon : String
  pub color : String
  pub bg : String
  pub ring : String
  pub version : String
  pub tags : Array[String]
  pub applicable_roles : Array[AgentRole]
  pub template : SkillTemplate
} derive(ToJson, FromJson, Show)

/// 渠道配置
pub struct ChannelConfig {
  pub enabled : Bool
  pub app_id : String?
  pub app_secret : String?
} derive(ToJson, FromJson, Show)

/// 审计配置
pub struct AuditConfig {
  pub enabled : Bool
  pub retention_days : Int
  pub log_path : String
} derive(ToJson, FromJson, Show)

/// Gateway 配置
pub struct GatewayConfig {
  pub port : Int
  pub host : String
} derive(ToJson, FromJson, Show)

/// 工作空间配置
pub struct WorkspaceConfig {
  pub path : String
  pub name : String
} derive(ToJson, FromJson, Show)

/// ClawTeam 主配置
pub struct ClawTeamConfig {
  pub version : String
  pub gateway : GatewayConfig
  pub workspace : WorkspaceConfig?
  pub cli_tools : Map[String, CliToolConfig]
  pub agents : Array[AgentConfig]
  pub skills : Array[Skill]
  pub channels : Map[String, ChannelConfig]
  pub audit : AuditConfig
} derive(ToJson, FromJson, Show)
```

- [ ] **步骤 3：编写配置解析测试 config/types_test.mbt**

```moonbit
test "parse CliToolType from json" {
  let json = @json.from_string!("\"Claude\"")
  let result = CliToolType::from_json(json)
  inspect!(result, content="Claude")
}

test "parse AgentRole from json" {
  let json = @json.from_string!("\"Assistant\"")
  let result = AgentRole::from_json(json)
  inspect!(result, content="Assistant")
}

test "parse AgentConfig from json" {
  let json_str = "{
    \"id\": \"assistant-1\",
    \"name\": \"主助手\",
    \"role\": \"Assistant\",
    \"cliTool\": \"opencode\",
    \"args\": [],
    \"env\": {},
    \"autoStart\": true,
    \"enabled\": true,
    \"skills\": []
  }"
  let json = @json.from_string!(json_str)
  let result = AgentConfig::from_json(json)
  inspect!(result.id, content="assistant-1")
  inspect!(result.name, content="主助手")
  inspect!(result.role, content="Assistant")
}
```

- [ ] **步骤 4：运行测试验证失败**

运行：`moon test config/`
预期：FAIL - from_json 方法未实现

- [ ] **步骤 5：实现 FromJson/ToJson 派生**

确保 types.mbt 中的 derive 包含 FromJson 和 ToJson。

- [ ] **步骤 6：运行测试验证通过**

运行：`moon test config/`
预期：PASS

- [ ] **步骤 7：创建配置加载器 config/loader.mbt**

```moonbit
/// 配置加载器
pub struct ConfigLoader {
  logger : @pino.Logger
}

pub fn ConfigLoader::new(logger : @pino.Logger) -> ConfigLoader {
  ConfigLoader::{ logger }
}

/// 配置文件路径优先级
pub fn config_paths(home : String, cwd : String) -> Array[String] {
  [
    @path.Path(home).join(@path.Path("clawteam.json")).to_string(),
    @path.Path(home).join(@path.Path(".clawteam")).join(@path.Path("clawteam.json")).to_string(),
    @path.Path(cwd).join(@path.Path("clawteam.json")).to_string(),
    @path.Path(cwd).join(@path.Path(".clawteam")).join(@path.Path("clawteam.json")).to_string(),
  ]
}

/// 加载配置
pub async fn ConfigLoader::load(
  self : ConfigLoader,
  home : String,
  cwd : String,
) -> Result[ClawTeamConfig, ClawTeamError] {
  let paths = config_paths(home, cwd)
  
  for path in paths {
    match @fsx.read_file(path) {
      Ok(content) => {
        self.logger.info("Loading config from \{path}")
        return self.parse_config(content)
      }
      Err(_) => continue
    }
  }
  
  // 返回默认配置
  self.logger.info("No config found, using default")
  Ok(self.default_config(home))
}

/// 解析配置
fn ConfigLoader::parse_config(
  self : ConfigLoader,
  content : String,
) -> Result[ClawTeamConfig, ClawTeamError] {
  match @json.from_string(content) {
    Err(msg) => Err(ClawTeamError::ConfigInvalid(msg)),
    Ok(json) => {
      match ClawTeamConfig::from_json(json) {
        Some(config) => Ok(config),
        None => Err(ClawTeamError::ConfigInvalid("Failed to parse config")),
      }
    }
  }
}

/// 默认配置
fn ConfigLoader::default_config(self : ConfigLoader, home : String) -> ClawTeamConfig {
  ClawTeamConfig::{
    version: "1.0.0",
    gateway: GatewayConfig::{ port: 3000, host: "127.0.0.1" },
    workspace: None,
    cli_tools: {
      "claude": CliToolConfig::{
        tool_type: CliToolType::Claude,
        command: "claude",
        args: [],
        env: {},
        cwd: None,
        ready_patterns: ["╭─"],
        working_patterns: ["Thinking...", "Working..."],
        output_patterns: None,
        timeout: None,
      },
      "opencode": CliToolConfig::{
        tool_type: CliToolType::OpenCode,
        command: "opencode",
        args: [],
        env: {},
        cwd: None,
        ready_patterns: [">"],
        working_patterns: ["Processing..."],
        output_patterns: None,
        timeout: None,
      },
    },
    agents: [
      AgentConfig::{
        id: "assistant-1",
        name: "主助手",
        role: AgentRole::Assistant,
        cli_tool: "opencode",
        args: [],
        env: {},
        cwd: None,
        auto_start: true,
        role_config: Some(RoleConfig::Assistant(AssistantConfig::{
          available_workers: [],
          available_supervisors: [],
          dispatch_strategy: DispatchStrategy::CapabilityMatch,
        })),
        skills: [],
        enabled: true,
      },
    ],
    skills: [],
    channels: {
      "web": ChannelConfig::{ enabled: true, app_id: None, app_secret: None },
    },
    audit: AuditConfig::{
      enabled: true,
      retention_days: 30,
      log_path: @path.Path(home).join(@path.Path("logs")).join(@path.Path("audit.jsonl")).to_string(),
    },
  }
}
```

- [ ] **步骤 8：创建模块入口 config/mod.mbt**

```moonbit
pub use config/types.*
pub use config/loader.*
```

- [ ] **步骤 9：运行测试**

运行：`moon check && moon test config/`
预期：PASS

- [ ] **步骤 10：Commit**

```bash
git add config/
git commit -m "feat: add configuration system with loader and types"
```

---

### 任务 3：复制 internal 工具库

**文件：**
- 复制：`reference/moonclaw/internal/` → `internal/`

- [ ] **步骤 1：复制 internal 目录**

```bash
cp -r reference/moonclaw/internal/ internal/
```

- [ ] **步骤 2：更新 moon.pkg 中的包名**

修改 `internal/*/moon.pkg` 中的包名，将 `moonbitlang/moonclaw` 改为 `clawteam/clawteam`。

使用批量替换命令（PowerShell）：
```powershell
Get-ChildItem -Path "internal" -Filter "moon.pkg" -Recurse | ForEach-Object {
    (Get-Content $_.FullName) -replace 'moonbitlang/moonclaw', 'clawteam/clawteam' | Set-Content $_.FullName
}
```

- [ ] **步骤 3：运行 moon check**

运行：`moon check internal/`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add internal/
git commit -m "feat: copy internal utilities from moonclaw"
```

---

### 任务 4：复制并适配 ACP 模块（精简）

**文件：**
- 复制并修改：`reference/moonclaw/acp/` → `acp/`

- [ ] **步骤 1：复制 acp 目录**

```bash
cp -r reference/moonclaw/acp/ acp/
```

- [ ] **步骤 2：更新 moon.pkg 包名**

```powershell
Get-ChildItem -Path "acp" -Filter "moon.pkg" -Recurse | ForEach-Object {
    (Get-Content $_.FullName) -replace 'moonbitlang/moonclaw', 'clawteam/clawteam' | Set-Content $_.FullName
}
```

- [ ] **步骤 3：精简 acp/runtime.mbt**

保留核心结构，删除 Codex 特定的 OAuth 逻辑，保留通用的 CLI 进程调用。

关键修改：
1. 保留 `AcpTarget`, `AcpSession`, `AcpRun` 结构
2. 删除 Codex OAuth 相关代码
3. 简化配置加载逻辑
4. 保留 spawn 调用逻辑

- [ ] **步骤 4：运行 moon check**

运行：`moon check acp/`
预期：PASS（可能有警告）

- [ ] **步骤 5：Commit**

```bash
git add acp/
git commit -m "feat: copy and simplify acp module from moonclaw"
```

---

### 任务 5：复制 Gateway 模块

**文件：**
- 复制：`reference/moonclaw/gateway/` → `gateway/`

- [ ] **步骤 1：复制 gateway 目录**

```bash
cp -r reference/moonclaw/gateway/ gateway/
```

- [ ] **步骤 2：更新所有 moon.pkg 包名**

```powershell
Get-ChildItem -Path "gateway" -Filter "moon.pkg" -Recurse | ForEach-Object {
    (Get-Content $_.FullName) -replace 'moonbitlang/moonclaw', 'clawteam/clawteam' | Set-Content $_.FullName
}
```

- [ ] **步骤 3：更新 import 语句**

检查所有 `.mbt` 文件中的 import 语句，将 `moonbitlang/moonclaw` 改为 `clawteam/clawteam`。

- [ ] **步骤 4：运行 moon check**

运行：`moon check gateway/`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add gateway/
git commit -m "feat: copy gateway module from moonclaw"
```

---

### 任务 6：复制 Job 模块

**文件：**
- 复制：`reference/moonclaw/job/` → `job/`

- [ ] **步骤 1：复制 job 目录**

```bash
cp -r reference/moonclaw/job/ job/
```

- [ ] **步骤 2：更新所有 moon.pkg 包名**

```powershell
Get-ChildItem -Path "job" -Filter "moon.pkg" -Recurse | ForEach-Object {
    (Get-Content $_.FullName) -replace 'moonbitlang/moonclaw', 'clawteam/clawteam' | Set-Content $_.FullName
}
```

- [ ] **步骤 3：更新 import 语句**

- [ ] **步骤 4：运行 moon check**

运行：`moon check job/`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add job/
git commit -m "feat: copy job module from moonclaw"
```

---

### 任务 7：复制 Channel 模块

**文件：**
- 复制：`reference/moonclaw/channel/` → `channel/`
- 复制：`reference/moonclaw/channels/` → `channels/`

- [ ] **步骤 1：复制目录**

```bash
cp -r reference/moonclaw/channel/ channel/
cp -r reference/moonclaw/channels/ channels/
```

- [ ] **步骤 2：更新所有 moon.pkg 包名**

```powershell
Get-ChildItem -Path "channel", "channels" -Filter "moon.pkg" -Recurse | ForEach-Object {
    (Get-Content $_.FullName) -replace 'moonbitlang/moonclaw', 'clawteam/clawteam' | Set-Content $_.FullName
}
```

- [ ] **步骤 3：运行 moon check**

运行：`moon check channel/ channels/`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add channel/ channels/
git commit -m "feat: copy channel modules from moonclaw"
```

---

### 任务 8：复制其他依赖模块

**文件：**
- 复制：`reference/moonclaw/event/` → `event/`
- 复制：`reference/moonclaw/clock/` → `clock/`
- 复制：`reference/moonclaw/model/` → `model/`
- 复制：`reference/moonclaw/ai/` → `ai/`
- 复制：`reference/moonclaw/oauth/` → `oauth/`
- 复制：`reference/moonclaw/cmd/` → `cmd/`

- [ ] **步骤 1：批量复制**

```bash
cp -r reference/moonclaw/event/ event/
cp -r reference/moonclaw/clock/ clock/
cp -r reference/moonclaw/model/ model/
cp -r reference/moonclaw/ai/ ai/
cp -r reference/moonclaw/oauth/ oauth/
cp -r reference/moonclaw/cmd/ cmd/
```

- [ ] **步骤 2：批量更新包名**

```powershell
$dirs = @("event", "clock", "model", "ai", "oauth", "cmd")
foreach ($dir in $dirs) {
    Get-ChildItem -Path $dir -Filter "moon.pkg" -Recurse | ForEach-Object {
        (Get-Content $_.FullName) -replace 'moonbitlang/moonclaw', 'clawteam/clawteam' | Set-Content $_.FullName
    }
}
```

- [ ] **步骤 3：运行完整检查**

运行：`moon check`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add event/ clock/ model/ ai/ oauth/ cmd/
git commit -m "feat: copy remaining dependency modules from moonclaw"
```

---

### 任务 9：Scheduler 调度器模块

**文件：**
- 创建：`scheduler/moon.pkg`
- 创建：`scheduler/mod.mbt`
- 创建：`scheduler/types.mbt`
- 创建：`scheduler/prompt.mbt`
- 创建：`scheduler/template.mbt`
- 创建：`scheduler/agent_router.mbt`
- 创建：`scheduler/dispatcher.mbt`
- 创建：`scheduler/dispatcher_test.mbt`

- [ ] **步骤 1：创建 scheduler/moon.pkg**

```moonbit
import {
  "moonbitlang/core/json",
  "moonbitlang/core/test",
  "moonbitlang/x/path",
  "clawteam/clawteam/config",
  "clawteam/clawteam/errors",
  "clawteam/clawteam/types",
  "clawteam/clawteam/acp",
  "clawteam/clawteam/internal/pino",
  "clawteam/clawteam/internal/uuid",
  "clawteam/clawteam/internal/spawn",
  "clawteam/clawteam/clock",
}
```

- [ ] **步骤 2：创建调度器类型 scheduler/types.mbt**

```moonbit
/// CLI 调用参数
pub struct CliCallParams {
  pub cli_tool : String
  pub agent_prompt : String
  pub context : String
  pub variables : Map[String, String]
} derive(ToJson, Show)

/// CLI 调用结果
pub struct CliCallResult {
  pub stdout : String
  pub stderr : String
  pub status : Int
  pub elapsed : Int
} derive(ToJson, Show)

/// Agent 决策
pub struct AgentDecision {
  pub steps : Array[DispatchStep]
  pub selected_agents : Array[String]
} derive(ToJson, Show)

/// 派发步骤
pub enum DispatchStep {
  Dispatch(String, String)  // (agent_id, task)
  DirectReply
  WaitForUser
} derive(ToJson, Show)

/// 派发上下文
pub struct DispatchContext {
  pub session_id : String
  pub user_id : String?
  pub channel : String?
  pub timestamp : Int
  pub metadata : Map[String, String]
} derive(Show)

/// 会话状态
pub struct SessionState {
  pub agent_id : String
  pub status : TerminalSessionStatus
  pub last_output_at : Int
  pub working_since : Int?
  pub dispatch_queue : Array[QueuedDispatch]
} derive(Show)

/// 排队中的派发
pub struct QueuedDispatch {
  pub id : String
  pub context : DispatchContext
  pub message : String
  pub created_at : Int
} derive(Show)
```

- [ ] **步骤 3：创建 Agent Prompt 模板 scheduler/prompt.mbt**

```moonbit
/// Agent 角色提示词模板
pub struct AgentPromptTemplate {
  pub role : AgentRole
  pub system_prompt : String
}

/// 助手提示词模板（协调器，不思考）
let ASSISTANT_PROMPT_TEMPLATE : String = """
你是一个助手角色（协调器），负责：
1. 接收用户需求
2. 分配任务给合适的员工
3. 收集员工反馈并返回给用户确认
4. 识别可并行任务并分发
5. 安排监工审核
6. 处理审核结果（通过→下一步，失败→让员工修正）

当前可用员工: {{available_workers}}
当前可用监工: {{available_supervisors}}

当前可用技能: {{skills}}

重要：你只负责协调，不负责思考和分析。所有思考工作由员工完成。
"""

/// 员工提示词模板（执行者，负责思考）
let WORKER_PROMPT_TEMPLATE : String = """
你是一个员工角色（执行者），负责：
1. 执行助手分配的任务
2. 进行思考、分析、规划、编码等工作
3. 向助手反馈执行结果

你的能力: {{capabilities}}

助手分配的任务: {{assigned_task}}

请认真思考并执行任务，返回详细结果。
"""

/// 监工提示词模板
let SUPERVISOR_PROMPT_TEMPLATE : String = """
你是一个监工角色（审核者），负责：
1. 审核员工的工作成果
2. 检查是否符合标准
3. 返回审核结果（通过/失败+具体问题）

审核标准: {{review_criteria}}

请审核指定的工作成果。
"""

/// 渲染提示词模板
pub fn render_prompt(
  template : String,
  variables : Map[String, String],
) -> String {
  var result = template
  for key, value in variables {
    result = result.replace("{{" + key + "}}", value)
  }
  result
}

/// 获取角色对应的提示词模板
pub fn get_prompt_template(role : AgentRole) -> String {
  match role {
    AgentRole::Assistant => ASSISTANT_PROMPT_TEMPLATE
    AgentRole::Worker => WORKER_PROMPT_TEMPLATE
    AgentRole::Supervisor => SUPERVISOR_PROMPT_TEMPLATE
  }
}
```

- [ ] **步骤 4：编写提示词渲染测试**

```moonbit
test "render_prompt replaces variables" {
  let template = "Hello {{name}}, welcome to {{place}}!"
  let variables = { "name": "World", "place": "ClawTeam" }
  let result = render_prompt(template, variables)
  inspect!(result, content="Hello World, welcome to ClawTeam!")
}

test "get_prompt_template returns correct template" {
  let assistant_template = get_prompt_template(AgentRole::Assistant)
  assert!(assistant_template.contains("协调器"))
  
  let worker_template = get_prompt_template(AgentRole::Worker)
  assert!(worker_template.contains("执行者"))
}
```

- [ ] **步骤 5：运行测试**

运行：`moon test scheduler/`
预期：PASS

- [ ] **步骤 6：创建消息模板填充 scheduler/template.mbt**

```moonbit
/// 消息模板填充器
pub struct MessageTemplate {
  template : String
  variables : Map[String, String]
}

pub fn MessageTemplate::new(template : String) -> MessageTemplate {
  MessageTemplate::{ template, variables: {} }
}

pub fn MessageTemplate::with_variable(
  self : MessageTemplate,
  key : String,
  value : String,
) -> MessageTemplate {
  let mut new_vars = self.variables
  new_vars.set(key, value)
  MessageTemplate::{ template: self.template, variables: new_vars }
}

pub fn MessageTemplate::with_variables(
  self : MessageTemplate,
  vars : Map[String, String],
) -> MessageTemplate {
  let mut new_vars = self.variables
  for key, value in vars {
    new_vars.set(key, value)
  }
  MessageTemplate::{ template: self.template, variables: new_vars }
}

pub fn MessageTemplate::render(self : MessageTemplate) -> String {
  render_prompt(self.template, self.variables)
}

/// 构建 CLI 命令
pub fn build_cli_command(
  tool_config : @config.CliToolConfig,
  params : CliCallParams,
) -> Array[String] {
  let prompt = render_prompt(params.agent_prompt, params.variables)
  let full_prompt = "\{prompt}\n\n\{params.context}"
  
  let mut args = tool_config.args.to_array()
  args.push("-p")
  args.push(full_prompt)
  args
}
```

- [ ] **步骤 7：创建 Agent 路由器 scheduler/agent_router.mbt**

```moonbit
/// Agent 路由器
pub struct AgentRouter {
  config : @config.ClawTeamConfig
  session_states : Map[String, SessionState]
  logger : @pino.Logger
}

pub fn AgentRouter::new(
  config : @config.ClawTeamConfig,
  logger : @pino.Logger,
) -> AgentRouter {
  AgentRouter::{
    config,
    session_states: {},
    logger,
  }
}

/// 获取 Agent 配置
pub fn AgentRouter::get_agent(
  self : AgentRouter,
  agent_id : String,
) -> Result[@config.AgentConfig, ClawTeamError] {
  for agent in self.config.agents {
    if agent.id == agent_id {
      return Ok(agent)
    }
  }
  Err(ClawTeamError::AgentNotFound(agent_id))
}

/// 获取默认助手
pub fn AgentRouter::get_default_assistant(
  self : AgentRouter,
) -> Result[@config.AgentConfig, ClawTeamError] {
  for agent in self.config.agents {
    if agent.role == AgentRole::Assistant && agent.enabled {
      return Ok(agent)
    }
  }
  Err(ClawTeamError::NoAvailableAgent)
}

/// 按角色和能力选择 Agent
pub fn AgentRouter::select_worker(
  self : AgentRouter,
  capabilities : Array[String],
) -> Result[@config.AgentConfig, ClawTeamError] {
  for agent in self.config.agents {
    if agent.role == AgentRole::Worker && agent.enabled {
      match agent.role_config {
        Some(@config.RoleConfig::Worker(wc)) => {
          let has_capability = capabilities.iter().any(fn(cap) {
            wc.capabilities.iter().any(fn(c) { c == cap })
          })
          if has_capability {
            return Ok(agent)
          }
        }
        _ => continue
      }
    }
  }
  Err(ClawTeamError::NoAvailableAgent)
}

/// 获取可用员工列表
pub fn AgentRouter::get_available_workers(
  self : AgentRouter,
  assistant_id : String,
) -> Array[String] {
  let mut workers = []
  for agent in self.config.agents {
    if agent.role == AgentRole::Worker && agent.enabled {
      match agent.role_config {
        Some(@config.RoleConfig::Worker(wc)) => {
          if wc.assistant_id == assistant_id || wc.assistant_id.is_none() {
            workers.push(agent.id)
          }
        }
        _ => {}
      }
    }
  }
  workers
}

/// 更新会话状态
pub fn AgentRouter::update_session_status(
  self : &mut AgentRouter,
  agent_id : String,
  status : TerminalSessionStatus,
) -> Unit {
  match self.session_states.get(agent_id) {
    Some(mut state) => {
      if state.status != status {
        state.status = status
        if status == TerminalSessionStatus::Working {
          state.working_since = Some(@clock.now_ms())
        }
        self.session_states.set(agent_id, state)
      }
    }
    None => {
      self.session_states.set(agent_id, SessionState::{
        agent_id,
        status,
        last_output_at: @clock.now_ms(),
        working_since: if status == TerminalSessionStatus::Working { Some(@clock.now_ms()) } else { None },
        dispatch_queue: [],
      })
    }
  }
}

/// 检查 Agent 是否可用
pub fn AgentRouter::is_agent_available(
  self : AgentRouter,
  agent_id : String,
) -> Bool {
  match self.session_states.get(agent_id) {
    Some(state) => {
      state.status == TerminalSessionStatus::Online || 
      state.status == TerminalSessionStatus::Pending
    }
    None => true
  }
}
```

- [ ] **步骤 8：创建调度执行器 scheduler/dispatcher.mbt**

```moonbit
/// 调度执行器
pub struct Dispatcher {
  config : @config.ClawTeamConfig
  router : AgentRouter
  logger : @pino.Logger
  uuid : @uuid.Generator
}

pub fn Dispatcher::new(
  config : @config.ClawTeamConfig,
  logger : @pino.Logger,
) -> Dispatcher {
  Dispatcher::{
    config,
    router: AgentRouter::new(config, logger),
    logger,
    uuid: @uuid.Generator::new(),
  }
}

/// 派发消息到助手
pub async fn Dispatcher::dispatch_to_assistant(
  self : &Dispatcher,
  message : String,
  context : DispatchContext,
) -> Result[CliCallResult, ClawTeamError] {
  // 获取默认助手
  let assistant = self.router.get_default_assistant()?
  
  // 获取可用员工和技能
  let workers = self.router.get_available_workers(assistant.id)
  let skill_names = self.config.skills.map(fn(s) { s.name }).join(", ")
  
  // 构建提示词变量
  let variables = {
    "available_workers": workers.join(", "),
    "available_supervisors": "", // TODO: 实现
    "skills": skill_names,
  }
  
  let agent_prompt = render_prompt(get_prompt_template(AgentRole::Assistant), variables)
  
  // 获取工具配置
  let tool_config = self.config.cli_tools.get(assistant.cli_tool)?
  
  let params = CliCallParams::{
    cli_tool: assistant.cli_tool,
    agent_prompt,
    context: message,
    variables: {},
  }
  
  // 执行 CLI 调用
  self.execute_cli_call(tool_config, params, assistant)
}

/// 派发任务到员工
pub async fn Dispatcher::dispatch_to_worker(
  self : &Dispatcher,
  worker_id : String,
  task : String,
  context : DispatchContext,
) -> Result[CliCallResult, ClawTeamError] {
  let worker = self.router.get_agent(worker_id)?
  
  // 获取能力配置
  let capabilities = match worker.role_config {
    Some(@config.RoleConfig::Worker(wc)) => wc.capabilities.join(", "),
    _ => "",
  }
  
  let variables = { "capabilities": capabilities, "assigned_task": task }
  let agent_prompt = render_prompt(get_prompt_template(AgentRole::Worker), variables)
  
  let tool_config = self.config.cli_tools.get(worker.cli_tool)?
  
  let params = CliCallParams::{
    cli_tool: worker.cli_tool,
    agent_prompt,
    context: task,
    variables: {},
  }
  
  self.execute_cli_call(tool_config, params, worker)
}

/// 派发审核任务到监工
pub async fn Dispatcher::dispatch_to_supervisor(
  self : &Dispatcher,
  supervisor_id : String,
  review_target : String,
  context : DispatchContext,
) -> Result[CliCallResult, ClawTeamError] {
  let supervisor = self.router.get_agent(supervisor_id)?
  
  let criteria = match supervisor.role_config {
    Some(@config.RoleConfig::Supervisor(sc)) => sc.review_criteria.join(", "),
    _ => "",
  }
  
  let variables = { "review_criteria": criteria }
  let agent_prompt = render_prompt(get_prompt_template(AgentRole::Supervisor), variables)
  
  let tool_config = self.config.cli_tools.get(supervisor.cli_tool)?
  
  let params = CliCallParams::{
    cli_tool: supervisor.cli_tool,
    agent_prompt,
    context: review_target,
    variables: {},
  }
  
  self.execute_cli_call(tool_config, params, supervisor)
}

/// 执行 CLI 调用
async fn Dispatcher::execute_cli_call(
  self : &Dispatcher,
  tool_config : @config.CliToolConfig,
  params : CliCallParams,
  agent : @config.AgentConfig,
) -> Result[CliCallResult, ClawTeamError] {
  let args = build_cli_command(tool_config, params)
  
  let cwd = match agent.cwd {
    Some(c) => c,
    None => match self.config.workspace {
      Some(ws) => ws.path,
      None => ".",
    },
  }
  
  let merged_env = {
    let mut env = tool_config.env
    for k, v in agent.env {
      env.set(k, v)
    }
    env
  }
  
  self.logger.info("Dispatching to \{agent.cli_tool}: \{tool_config.command}")
  
  let start_time = @clock.now_ms()
  
  match @spawn.spawn(
    tool_config.command,
    args,
    cwd=Some(cwd),
    env=Some(merged_env),
  ) {
    Ok(result) => {
      let elapsed = @clock.now_ms() - start_time
      Ok(CliCallResult::{
        stdout: result.stdout,
        stderr: result.stderr,
        status: result.status,
        elapsed,
      })
    }
    Err(e) => {
      self.logger.error("CLI call failed: \{e}")
      Err(ClawTeamError::ProcessStartFailed(tool_config.command))
    }
  }
}
```

- [ ] **步骤 9：创建模块入口 scheduler/mod.mbt**

```moonbit
pub use scheduler/types.*
pub use scheduler/prompt.*
pub use scheduler/template.*
pub use scheduler/agent_router.*
pub use scheduler/dispatcher.*

/// 调度器（组合入口）
pub struct Scheduler {
  config : @config.ClawTeamConfig
  dispatcher : Dispatcher
  logger : @pino.Logger
}

pub fn Scheduler::new(
  config : @config.ClawTeamConfig,
  logger : @pino.Logger,
) -> Scheduler {
  Scheduler::{
    config,
    dispatcher: Dispatcher::new(config, logger),
    logger,
  }
}

pub async fn Scheduler::dispatch(
  self : &Scheduler,
  message : String,
  context : DispatchContext,
) -> Result[CliCallResult, ClawTeamError] {
  self.dispatcher.dispatch_to_assistant(message, context)
}
```

- [ ] **步骤 10：运行完整检查和测试**

运行：`moon check && moon test scheduler/`
预期：PASS

- [ ] **步骤 11：Commit**

```bash
git add scheduler/
git commit -m "feat: add scheduler module with agent router and dispatcher"
```

---

### 任务 10：Parser 输出解析模块

**文件：**
- 创建：`parser/moon.pkg`
- 创建：`parser/mod.mbt`
- 创建：`parser/regex_parser.mbt`
- 创建：`parser/regex_parser_test.mbt`

- [ ] **步骤 1：创建 parser/moon.pkg**

```moonbit
import {
  "moonbitlang/core/json",
  "moonbitlang/core/test",
  "moonbitlang/regexp",
  "clawteam/clawteam/config",
  "clawteam/clawteam/types",
}
```

- [ ] **步骤 2：创建正则解析器 parser/regex_parser.mbt**

```moonbit
/// 解析结果
pub struct ParseResult {
  pub matched : Bool
  pub pattern_type : PatternType
  pub captures : Map[String, String]
} derive(ToJson, Show)

/// 模式类型
pub enum PatternType {
  Success
  Error
  Progress
  Ready
  Working
  Unknown
} derive(ToJson, FromJson, Show, Eq)

/// 正则解析器
pub struct RegexParser {
  patterns : @config.OutputPatterns?
  ready_patterns : Array[String]
  working_patterns : Array[String]
}

pub fn RegexParser::new(
  patterns : @config.OutputPatterns?,
  ready_patterns : Array[String],
  working_patterns : Array[String],
) -> RegexParser {
  RegexParser::{ patterns, ready_patterns, working_patterns }
}

/// 解析输出行
pub fn RegexParser::parse_line(
  self : RegexParser,
  line : String,
) -> ParseResult {
  // 检查就绪模式
  for pattern in self.ready_patterns {
    if line.contains(pattern) {
      return ParseResult::{
        matched: true,
        pattern_type: PatternType::Ready,
        captures: {},
      }
    }
  }
  
  // 检查工作中模式
  for pattern in self.working_patterns {
    if line.contains(pattern) {
      return ParseResult::{
        matched: true,
        pattern_type: PatternType::Working,
        captures: {},
      }
    }
  }
  
  // 检查输出模式
  match self.patterns {
    Some(p) => {
      // 成功模式
      match p.success_pattern {
        Some(sp) if line.contains(sp) => {
          return ParseResult::{
            matched: true,
            pattern_type: PatternType::Success,
            captures: {},
          }
        }
        _ => {}
      }
      
      // 错误模式
      match p.error_pattern {
        Some(ep) if line.contains(ep) => {
          return ParseResult::{
            matched: true,
            pattern_type: PatternType::Error,
            captures: {},
          }
        }
        _ => {}
      }
      
      // 进度模式
      match p.progress_pattern {
        Some(pp) if line.contains(pp) => {
          return ParseResult::{
            matched: true,
            pattern_type: PatternType::Progress,
            captures: {},
          }
        }
        _ => {}
      }
    }
    None => {}
  }
  
  ParseResult::{
    matched: false,
    pattern_type: PatternType::Unknown,
    captures: {},
  }
}

/// 解析多行输出
pub fn RegexParser::parse_output(
  self : RegexParser,
  output : String,
) -> Array[ParseResult] {
  output.split('\n').iter()
    .map(fn(line) { self.parse_line(line) })
    .filter(fn(r) { r.matched })
    .to_array()
}

/// 检测最终状态
pub fn RegexParser::detect_final_status(
  self : RegexParser,
  output : String,
) -> PatternType {
  let results = self.parse_output(output)
  
  // 从后往前检查
  for result in results.reverse() {
    match result.pattern_type {
      PatternType::Success => return PatternType::Success
      PatternType::Error => return PatternType::Error
      _ => continue
    }
  }
  
  PatternType::Unknown
}
```

- [ ] **步骤 3：编写解析器测试**

```moonbit
test "parse_line detects ready pattern" {
  let parser = RegexParser::new(None, ["╭─", ">"], ["Thinking..."])
  let result = parser.parse_line("╭─ Claude Code")
  assert!(result.matched)
  inspect!(result.pattern_type, content="Ready")
}

test "parse_line detects working pattern" {
  let parser = RegexParser::new(None, [">"], ["Thinking...", "Working..."])
  let result = parser.parse_line("Thinking... about your request")
  assert!(result.matched)
  inspect!(result.pattern_type, content="Working")
}

test "parse_line with output patterns" {
  let patterns = @config.OutputPatterns::{
    success_pattern: Some("✓ Complete"),
    error_pattern: Some("✗ Error"),
    progress_pattern: Some("..."),
  }
  let parser = RegexParser::new(Some(patterns), [], [])
  
  let success_result = parser.parse_line("✓ Complete")
  inspect!(success_result.pattern_type, content="Success")
  
  let error_result = parser.parse_line("✗ Error: something went wrong")
  inspect!(error_result.pattern_type, content="Error")
}

test "detect_final_status returns correct status" {
  let patterns = @config.OutputPatterns::{
    success_pattern: Some("✓"),
    error_pattern: Some("✗"),
    progress_pattern: None,
  }
  let parser = RegexParser::new(Some(patterns), [], [])
  
  let output = "Processing...\nWorking...\n✓ Complete"
  let status = parser.detect_final_status(output)
  inspect!(status, content="Success")
}
```

- [ ] **步骤 4：运行测试**

运行：`moon test parser/`
预期：PASS

- [ ] **步骤 5：创建模块入口 parser/mod.mbt**

```moonbit
pub use parser/regex_parser.*
```

- [ ] **步骤 6：Commit**

```bash
git add parser/
git commit -m "feat: add parser module with regex output parser"
```

---

### 任务 11：Logger 审计日志模块

**文件：**
- 创建：`logger/moon.pkg`
- 创建：`logger/mod.mbt`
- 创建：`logger/types.mbt`
- 创建：`logger/audit_store.mbt`
- 创建：`logger/query.mbt`
- 创建：`logger/audit_test.mbt`

- [ ] **步骤 1：创建 logger/moon.pkg**

```moonbit
import {
  "moonbitlang/core/json",
  "moonbitlang/core/test",
  "moonbitlang/x/path",
  "clawteam/clawteam/config",
  "clawteam/clawteam/types",
  "clawteam/clawteam/internal/fsx",
  "clawteam/clawteam/internal/pino",
  "clawteam/clawteam/internal/uuid",
  "clawteam/clawteam/clock",
}
```

- [ ] **步骤 2：创建日志类型 logger/types.mbt**

```moonbit
/// 审计日志条目
pub struct AuditLogEntry {
  pub id : String
  pub timestamp : Int
  pub event_type : AuditEventType
  pub session_id : String?
  pub target_id : String?
  pub job_id : String?
  pub run_id : String?
  pub user_id : String?
  pub channel : String?
  pub details : Json
} derive(ToJson, FromJson, Show)

/// 日志查询条件
pub struct AuditQuery {
  pub start_time : Int?
  pub end_time : Int?
  pub event_types : Array[AuditEventType]?
  pub session_id : String?
  pub target_id : String?
  pub user_id : String?
  pub limit : Int
} derive(Show)

impl Default for AuditQuery with default() -> AuditQuery {
  AuditQuery::{
    start_time: None,
    end_time: None,
    event_types: None,
    session_id: None,
    target_id: None,
    user_id: None,
    limit: 100,
  }
}
```

- [ ] **步骤 3：创建审计存储 logger/audit_store.mbt**

```moonbit
/// 审计日志存储
pub struct AuditStore {
  log_path : String
  logger : @pino.Logger
  uuid : @uuid.Generator
  enabled : Bool
}

pub fn AuditStore::new(
  config : @config.AuditConfig,
  logger : @pino.Logger,
) -> AuditStore {
  // 确保日志目录存在
  let parent = @path.Path(config.log_path).parent().to_string()
  @fsx.make_directory(parent, recursive=true) catch { _ => () }
  
  AuditStore::{
    log_path: config.log_path,
    logger,
    uuid: @uuid.Generator::new(),
    enabled: config.enabled,
  }
}

/// 写入审计日志
pub async fn AuditStore::write(
  self : &AuditStore,
  event_type : AuditEventType,
  details : Json,
  session_id? : String,
  target_id? : String,
  job_id? : String,
  run_id? : String,
  user_id? : String,
  channel? : String,
) -> Result[String, ClawTeamError] {
  if !self.enabled {
    return Ok("")
  }
  
  let id = self.uuid.generate()
  let entry = AuditLogEntry::{
    id,
    timestamp: @clock.now_ms(),
    event_type,
    session_id,
    target_id,
    job_id,
    run_id,
    user_id,
    channel,
    details,
  }
  
  let line = entry.to_json().stringify() + "\n"
  
  match @fsx.append_file(self.log_path, line) {
    Ok(()) => {
      self.logger.debug("Audit log written: \{id}")
      Ok(id)
    }
    Err(e) => {
      self.logger.error("Failed to write audit log: \{e}")
      Err(ClawTeamError::AuditLogWriteFailed(e))
    }
  }
}

/// 记录 CLI 启动
pub async fn AuditStore::log_cli_started(
  self : &AuditStore,
  session_id : String,
  target_id : String,
  command : String,
  args : Array[String],
) -> Result[String, ClawTeamError] {
  let details = @json.from_string!("{\"command\": \"\{command}\", \"args\": \{args.to_json().stringify()}}")
  self.write(
    AuditEventType::CliStarted,
    details,
    session_id=Some(session_id),
    target_id=Some(target_id),
  )
}

/// 记录 CLI 完成
pub async fn AuditStore::log_cli_completed(
  self : &AuditStore,
  session_id : String,
  target_id : String,
  status : Int,
  elapsed : Int,
) -> Result[String, ClawTeamError] {
  let details = @json.from_string!("{\"status\": \{status}, \"elapsed_ms\": \{elapsed}}")
  self.write(
    AuditEventType::CliCompleted,
    details,
    session_id=Some(session_id),
    target_id=Some(target_id),
  )
}

/// 记录 CLI 失败
pub async fn AuditStore::log_cli_failed(
  self : &AuditStore,
  session_id : String,
  target_id : String,
  error : String,
) -> Result[String, ClawTeamError] {
  let details = @json.from_string!("{\"error\": \"\{error}\"}")
  self.write(
    AuditEventType::CliFailed,
    details,
    session_id=Some(session_id),
    target_id=Some(target_id),
  )
}

/// 记录输出捕获
pub async fn AuditStore::log_output_captured(
  self : &AuditStore,
  session_id : String,
  output_type : String,
  content : String,
) -> Result[String, ClawTeamError] {
  // 截断过长的输出
  let truncated = if content.length() > 10000 {
    content[:10000] + "...[truncated]"
  } else {
    content
  }
  
  let details = @json.from_string!("{\"output_type\": \"\{output_type}\", \"content\": \"\{truncated.escape()}\"}")
  self.write(
    AuditEventType::OutputCaptured,
    details,
    session_id=Some(session_id),
  )
}
```

- [ ] **步骤 4：创建日志查询 logger/query.mbt**

```moonbit
/// 日志查询器
pub struct AuditQuery {
  store : AuditStore
}

impl AuditQuery {
  /// 查询日志
  pub async fn query(
    self : &AuditQuery,
    query : AuditQuery,
  ) -> Result[Array[AuditLogEntry], ClawTeamError] {
    let content = @fsx.read_file(self.store.log_path)?
    let lines = content.split('\n')
    
    let mut entries = []
    for line in lines {
      if line.is_blank() { continue }
      
      match @json.from_string(line) {
        Ok(json) => {
          match AuditLogEntry::from_json(json) {
            Some(entry) => {
              if self.matches_query(entry, query) {
                entries.push(entry)
              }
            }
            None => continue
          }
        }
        Err(_) => continue
      }
    }
    
    // 按时间倒序排列
    entries.sort(fn(a, b) { b.timestamp <=> a.timestamp })
    
    // 限制数量
    if entries.length() > query.limit {
      entries = entries[:query.limit]
    }
    
    Ok(entries)
  }
  
  /// 检查条目是否匹配查询条件
  fn matches_query(
    self : AuditQuery,
    entry : AuditLogEntry,
    query : AuditQuery,
  ) -> Bool {
    // 时间范围
    match query.start_time {
      Some(t) if entry.timestamp < t => return false
      _ => {}
    }
    match query.end_time {
      Some(t) if entry.timestamp > t => return false
      _ => {}
    }
    
    // 事件类型
    match query.event_types {
      Some(types) if !types.iter().any(fn(t) { t == entry.event_type }) => return false
      _ => {}
    }
    
    // Session ID
    match query.session_id {
      Some(id) if entry.session_id != Some(id) => return false
      _ => {}
    }
    
    // Target ID
    match query.target_id {
      Some(id) if entry.target_id != Some(id) => return false
      _ => {}
    }
    
    // User ID
    match query.user_id {
      Some(id) if entry.user_id != Some(id) => return false
      _ => {}
    }
    
    true
  }
}
```

- [ ] **步骤 5：编写审计日志测试**

```moonbit
test "AuditLogEntry to_json and from_json" {
  let entry = AuditLogEntry::{
    id: "test-123",
    timestamp: 1700000000000,
    event_type: AuditEventType::CliStarted,
    session_id: Some("session-1"),
    target_id: Some("target-1"),
    job_id: None,
    run_id: None,
    user_id: Some("user-1"),
    channel: Some("web"),
    details: @json.from_string!("{}"),
  }
  
  let json = entry.to_json()
  let parsed = AuditLogEntry::from_json(json)
  
  match parsed {
    Some(p) => {
      assert!(p.id == "test-123")
      assert!(p.event_type == AuditEventType::CliStarted)
    }
    None => assert!(false, "Failed to parse AuditLogEntry")
  }
}
```

- [ ] **步骤 6：创建模块入口 logger/mod.mbt**

```moonbit
pub use logger/types.*
pub use logger/audit_store.*
pub use logger/query.*
```

- [ ] **步骤 7：运行测试**

运行：`moon test logger/`
预期：PASS

- [ ] **步骤 8：Commit**

```bash
git add logger/
git commit -m "feat: add audit logger module"
```

---

### 任务 12：集成测试与完整构建

**文件：**
- 创建：`clawteam_test.mbt`
- 修改：`moon.pkg`

- [ ] **步骤 1：创建集成测试 clawteam_test.mbt**

```moonbit
test "config loader creates default config" {
  let logger = @pino.Logger::new("test")
  let loader = @config.ConfigLoader::new(logger)
  
  // 测试默认配置
  let default = loader.default_config("/tmp/test")
  assert!(default.version == "1.0.0")
  assert!(default.gateway.port == 3000)
  assert!(default.agents.length() > 0)
}

test "agent router finds default assistant" {
  let logger = @pino.Logger::new("test")
  let config = @config.ConfigLoader::new(logger).default_config("/tmp/test")
  let router = @scheduler.AgentRouter::new(config, logger)
  
  let result = router.get_default_assistant()
  match result {
    Ok(agent) => {
      assert!(agent.role == AgentRole::Assistant)
      assert!(agent.enabled)
    }
    Err(_) => assert!(false, "Should find default assistant")
  }
}

test "prompt rendering works" {
  let template = get_prompt_template(AgentRole::Assistant)
  let variables = {
    "available_workers": "worker-1, worker-2",
    "available_supervisors": "supervisor-1",
    "skills": "generate-api",
  }
  
  let result = render_prompt(template, variables)
  assert!(result.contains("worker-1, worker-2"))
  assert!(result.contains("supervisor-1"))
  assert!(result.contains("generate-api"))
}
```

- [ ] **步骤 2：运行完整测试**

运行：`moon check && moon test`
预期：PASS

- [ ] **步骤 3：运行 moon info 更新接口**

运行：`moon info`

- [ ] **步骤 4：运行 moon fmt 格式化**

运行：`moon fmt`

- [ ] **步骤 5：检查 .mbti diff**

运行：`git diff *.mbti`
预期：无意外变更

- [ ] **步骤 6：Final Commit**

```bash
git add .
git commit -m "feat: complete clawteam backend core with integration tests"
```

---

## 验证清单

- [ ] `moon check` 通过
- [ ] `moon test` 通过
- [ ] `moon info` 无错误
- [ ] `moon fmt` 格式化完成
- [ ] 所有新文件已提交
- [ ] 从 MoonClaw 复制的文件包名已更新

---

## 下一步计划

完成此计划后，继续执行：

1. **计划 B: 前端 UI 扩展** - `clawteam-chat` 多 Agent 聊天界面
2. **计划 C: 渠道集成与端到端测试** - Feishu/Weixin 集成、E2E 测试
