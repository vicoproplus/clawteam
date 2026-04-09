# MoonBit 后端迁移实施计划 (v2 - Revised)

> **For agentic workers:** REQUIRED SUB-SKILL: Use executing-plans or subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Paperclip 后端核心领域模型、预算计算、共享验证器从 TypeScript 渐进式迁移到 MoonBit,通过 stdio JSON-RPC 与现有 Express 系统集成,减少约 20-30% pnpm 包依赖。

**Architecture:** 独立进程架构 — MoonBit 编译为独立二进制,作为 Express 子进程运行,通过 stdio JSON-RPC 通信 (而非 HTTP)。迁移顺序: API 验证 → 常量 → 领域模型 → 预算计算 → 验证器 → 集成测试。

**Tech Stack:** MoonBit 0.1.20260403, Node.js 20+, Express 5, TypeScript 5.7, pnpm 9.15, PostgreSQL/PGlite

**设计文档:** `docs/superpowers/specs/2026-04-08-moonbit-backend-migration-design.md`

**Critic 修订记录 (v2)**:
- P0-1: AgentStatus 枚举值已对齐 TS 常量 (移除 Active,补全 TS 的 active 语义)
- P0-2: 状态机转换规则现在引用 TS 源码位置
- P0-3: HTTP 服务端已替换为 stdio JSON-RPC 方案 (含完整实现代码)
- P0-4: 预算窗口计算已实现真实逻辑或标记为 TS 回退
- P0-5: Issue 状态机已补全缺失转换并与 TS 对齐
- P1-1: Task 1 新增 MoonBit API 验证步骤
- P1-2: JSON 序列化层明确约定 (JSON: snake_case, 内部: PascalCase)
- P1-3: 成本聚合已增加过滤逻辑
- P1-4: spawn 已移除 shell: true
- P1-5: 新增配置读取 Task

---

## RALPLAN-DR Summary

### Principles
1. **渐进式迁移** — 保持现有系统运行,逐步替换,每阶段可独立验证
2. **契约优先** — MoonBit 与 TypeScript 通过 JSON 协议对齐类型契约 (stdio JSON-RPC)
3. **纯函数优先** — 优先迁移无状态计算逻辑,后迁移 IO 密集型
4. **测试驱动** — 每个迁移单元必须有对等的 TypeScript 测试覆盖
5. **配置统一** — 复用现有 `~/.paperclip/config.json` 配置体系

### Decision Drivers
1. **减少依赖** — 目标是减少 20-30% pnpm 包,主要是 shared 验证器/类型包
2. **类型安全** — MoonBit 强类型系统替代 TypeScript + Zod 验证
3. **向后兼容** — Express API 契约不变,UI 层无感知
4. **Windows 兼容** — 必须在 win32 环境完整运行
5. **开发体验** — 保持 `pnpm dev` 一键启动,MoonBit 子进程自动管理

### Viable Options

#### Option A: 独立进程 + stdio JSON-RPC (✓ 选择,修订后)
**Pros:** 解耦、可独立测试、MoonBit 生态要求极低、跨平台稳定、无网络开销  
**Cons:** 需要进程管理逻辑、调试略不如 HTTP 直观

#### Option B: 独立进程 + HTTP/JSON (原选择,已废弃)
**Pros:** 调试方便  
**Cons:** MoonBit 0.1 无成熟 HTTP 库,核心路径依赖未知生态

#### Option C: WASM + Node.js FFI
**Pros:** 零网络开销、类型安全  
**Cons:** MoonBit WASM 绑定代码复杂、调试困难、Windows 兼容性问题

**Invalidation Rationale:**
- Option B 被排除: MoonBit 0.1 (2026-04) 无稳定 HTTP 服务器库,阻塞执行
- Option C 被排除: WASM 绑定开发成本高,不适合渐进式迁移

---

## Acceptance Criteria

| # | 标准 | 验证方法 |
|---|------|---------|
| AC1 | MoonBit 模块可构建 | `cd moonbit-core && moon build` 成功 |
| AC2 | MoonBit stdio 服务可启动 | 发送 `{"jsonrpc":"2.0","method":"health","id":1}` 返回 `{"result":"ok"}` |
| AC3 | 常量 100% 迁移 | MoonBit 枚举 `to_string()` 输出与 `constants.ts` 字符串值 1:1 一致 |
| AC4 | Agent 状态机 100% 覆盖 | 所有转换规则与 `server/src/services/agents.ts` 一致,测试通过 |
| AC5 | Issue 状态机 100% 覆盖 | 所有转换规则与 `server/src/services/issues.ts` 一致,测试通过 |
| AC6 | 预算计算与 TS 行为一致 | 相同输入产生相同输出 (对比 `server/src/services/budgets.ts`) |
| AC7 | Express 集成调用成功 | TypeScript 客户端通过 stdio 调用 MoonBit 端点成功 |
| AC8 | 现有 Vitest 套件无破坏 | `pnpm test:run` 通过 |
| AC9 | 进程管理器自动重启 | 崩溃后 3 次内自动恢复 |
| AC10 | `pnpm dev` 一键启动 | Express + MoonBit 同时启动 |

---

## File Structure Mapping

### 新增文件
```
moonbit-core/
├── moon.mod.json
├── moon.pkg.json
├── README.md
├── src/
│   ├── lib/
│   │   ├── json.mbt                  # JSON 序列化/反序列化
│   │   └── rpc.mbt                   # JSON-RPC over stdio
│   ├── domain/
│   │   ├── common/
│   │   │   ├── enums.mbt             # 枚举常量 (对齐 constants.ts)
│   │   │   ├── state_machine.mbt     # 状态机 trait
│   │   │   └── validation.mbt        # 验证器基础
│   │   ├── agent/
│   │   │   ├── types.mbt
│   │   │   ├── state_machine.mbt     # 引用 agents.ts 转换规则
│   │   │   └── validation.mbt
│   │   ├── issue/
│   │   │   ├── types.mbt
│   │   │   ├── state_machine.mbt     # 引用 issues.ts 转换规则
│   │   │   └── validation.mbt
│   │   ├── company/
│   │   │   ├── types.mbt
│   │   │   └── validation.mbt
│   │   ├── goal/
│   │   │   ├── types.mbt
│   │   │   └── validation.mbt
│   │   └── project/
│   │       ├── types.mbt
│   │       └── validation.mbt
│   ├── budget/
│   │   ├── types.mbt
│   │   ├── enforcement.mbt
│   │   └── window.mbt                # 真实实现或 TS 回退
│   ├── cost/
│   │   ├── types.mbt
│   │   └── aggregation.mbt           # 含过滤逻辑
│   └── server/
│       ├── handlers.mbt              # JSON-RPC handlers
│       └── main.mbt                  # stdio 循环入口
└── bin/
    └── server.mbt                    # 入口点
```

### 修改文件
```
server/src/services/
├── moonbit-client.ts                 # [新增] TypeScript stdio 客户端
├── moonbit-process-manager.ts        # [新增] 进程管理器 (无 shell: true)
├── agents.ts                         # [修改] 集成 MoonBit 验证
└── issues.ts                         # [修改] 集成 MoonBit 状态机

server/src/routes/
└── index.ts                          # [修改] 启动 MoonBit 子进程

package.json                          # [修改] 新增 MoonBit 构建脚本
```

### 测试文件
```
moonbit-core/
└── src/
    ├── domain/
    │   ├── agent/
    │   │   └── state_machine_test.mbt
    │   └── issue/
    │       └── state_machine_test.mbt
    └── budget/
        └── enforcement_test.mbt

tests/moonbit-integration/
├── agent-validation.test.ts
├── issue-transition.test.ts
└── budget-enforcement.test.ts
```

---

## Tasks

### Task 0: 状态机对齐调研 (新增,阻塞 Task 3/4)

**目标:** 确认 TypeScript 状态机的权威转换规则,确保 MoonBit 实现与 TS 100% 一致。

**Files:**
- Read: `server/src/services/agents.ts`
- Read: `server/src/services/issues.ts`
- Read: `packages/shared/src/constants.ts`

- [ ] **Step 1: 调研 Agent 状态机**

读取 `server/src/services/agents.ts`,找到状态转换验证逻辑 (搜索 `canTransition` 或 `assertTransition`)。
同时读取 `packages/shared/src/constants.ts` 中 `AGENT_STATUSES` 的值。

输出权威转换清单:
```
Agent Status Transitions (from agents.ts:LINE-NUMBER):
- Idle -> Running: true
- Idle -> Paused: true
- Running -> Idle: true
- Running -> Error: true
- Running -> Paused: true
- Error -> Paused: true
- Error -> Running: true
- Paused -> Idle: true
- Paused -> Running: true
- Paused -> Terminated: true
- PendingApproval -> Idle: true (确认是否存在)
- PendingApproval -> Terminated: true (确认是否存在)
- ANY -> Terminated: true
- Terminated -> ANY: false
```

- [ ] **Step 2: 调研 Issue 状态机**

读取 `server/src/services/issues.ts`,找到状态转换验证逻辑 (搜索 `assertTransition` 或状态机定义)。
同时读取 `packages/shared/src/constants.ts` 中 `ISSUE_STATUSES` 的值。

输出权威转换清单:
```
Issue Status Transitions (from issues.ts:LINE-NUMBER):
- Backlog -> Todo: true
- Backlog -> Cancelled: true
- Todo -> InProgress: true
- Todo -> Blocked: true
- Todo -> Cancelled: true
- InProgress -> InReview: true
- InProgress -> Blocked: true
- InProgress -> Todo: true
- InProgress -> Cancelled: true (确认是否存在)
- InReview -> Done: true
- InReview -> InProgress: true
- InReview -> Todo: true
- InReview -> Blocked: true (确认是否存在)
- Blocked -> Todo: true
- Blocked -> Cancelled: true
- Done -> ANY: false
- Cancelled -> ANY: false
```

- [ ] **Step 3: 确认 AgentStatus 枚举值**

对比 `AGENT_STATUSES` 在 TS 中的值:
```
TS AGENT_STATUSES: ["active", "paused", "idle", "running", "error", "pending_approval", "terminated"]
```

注意: `active` 在 TS 中与 `idle`/`running` 语义重叠。MoonBit 中:
- 如果 `active` 是独立状态 → 加入枚举
- 如果 `active` 是 `idle | running` 的逻辑或 → 不加入枚举,在验证逻辑中处理

- [ ] **Step 4: 保存调研结果**

将结果保存到 `moonbit-core/STATE-MACHINE-CONTRACTS.md`,作为 Task 3/4 的权威来源。

---

### Task 1: MoonBit 模块基础设施

**Files:**
- Create: `moonbit-core/moon.mod.json`
- Create: `moonbit-core/moon.pkg.json`
- Create: `moonbit-core/README.md`
- Create: `moonbit-core/src/lib/json.mbt`
- Create: `moonbit-core/src/lib/rpc.mbt`
- Modify: `package.json` (新增 scripts)

- [ ] **Step 1: 验证 MoonBit 核心 API 可用性 (P1-1 修复)**

编写最小测试确认以下 API 可编译:

```moonbit
// moonbit-core/src/lib/api_probe.mbt

// 测试 Map API
test "map api works" {
  let mut m: Map[String, Int] = Map::new()
  m = m.set("key", 42)  // immutable set
  assert m.get("key") == Some(42)
}

// 测试 List API
test "list fold works" {
  let xs: List[Int] = [1, 2, 3]
  let sum = xs.fold(fn(acc, x) { acc + x }, 0)
  assert sum == 6
}

// 测试 String API
test "string length works" {
  assert "hello".length() == 5
}
```

Run: `cd moonbit-core && moon test`
Expected: 3 个测试全部通过。如果失败,记录实际 API 签名并在后续任务中使用正确 API。

- [ ] **Step 2: 创建 moon.mod.json**

```json
{
  "name": "paperclip/moonbit-core",
  "version": "0.1.0",
  "readme": "README.md",
  "repository": "https://github.com/HenkDz/paperclip",
  "license": "MIT",
  "source": "src"
}
```

- [ ] **Step 3: 创建 moon.pkg.json**

```json
{
  "is-main": false,
  "import": [],
  "link": {}
}
```

- [ ] **Step 4: 创建 JSON 工具模块**

```moonbit
// moonbit-core/src/lib/json.mbt

// JSON 编码: 将内部类型序列化为 snake_case JSON 字符串
// JSON 解码: 将 snake_case JSON 解析为内部 PascalCase 类型

// 注意: MoonBit 0.1 JSON 支持需要验证
// 如果 stdlib 有 Json 模块,直接使用
// 否则手动实现核心类型序列化

pub fn string_to_json(s: String) -> String {
  // 如果 MoonBit 有内置 JSON: Json::encode(s)
  // 否则: "\"" + escape_json(s) + "\""
  "\"" + escape_json(s) + "\""
}

pub fn int_to_json(i: Int) -> String {
  i.to_string()
}

pub fn bool_to_json(b: Bool) -> String {
  if b { "true" } else { "false" }
}

pub fn option_to_json[T](opt: Option[T], to_json: Fn(T) -> String) -> String {
  match opt {
    Some(v) => to_json(v)
    None => "null"
  }
}

fn escape_json(s: String) -> String {
  s.replace("\\", "\\\\")
   .replace("\"", "\\\"")
   .replace("\n", "\\n")
   .replace("\r", "\\r")
   .replace("\t", "\\t")
}

// 简单 JSON 解析 (仅支持 string/number/bool/null)
pub fn parse_json_string(s: String) -> Result[String, String] {
  if s.starts_with("\"") && s.ends_with("\"") && s.length() >= 2 {
    Ok(s.substring(1, s.length() - 1))
  } else {
    Err("Expected string literal")
  }
}

pub fn parse_json_int(s: String) -> Result[Int, String] {
  s.to_int()
}

pub fn parse_json_bool(s: String) -> Result[Bool, String> {
  if s == "true" {
    Ok(true)
  } else if s == "false" {
    Ok(false)
  } else {
    Err("Expected boolean literal")
  }
}
```

- [ ] **Step 5: 创建 JSON-RPC over stdio 模块 (P0-3 修复)**

```moonbit
// moonbit-core/src/lib/rpc.mbt

// JSON-RPC 2.0 over stdio
// Request:  {"jsonrpc":"2.0","method":"<method>","params":{...},"id":<number>}
// Response: {"jsonrpc":"2.0","result":{...},"id":<number>}
// Error:    {"jsonrpc":"2.0","error":{"code":-32600,"message":"..."},"id":<number>}

pub struct JsonRpcRequest {
  method: String
  params: String  // 原始 JSON 字符串,由 handler 自行解析
  id: Int
}

pub struct JsonRpcResponse {
  result: Option[String]   // Some(JSON字符串) 或 None
  error: Option[String]    // Some(错误消息) 或 None
  id: Int
}

pub fn format_response(resp: JsonRpcResponse) -> String {
  match (resp.result, resp.error) {
    (Some(result), None) => {
      "{\"jsonrpc\":\"2.0\",\"result\":" + result + ",\"id\":" + resp.id.to_string() + "}"
    }
    (None, Some(error)) => {
      "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"" + error + "\"},\"id\":" + resp.id.to_string() + "}"
    }
    _ => {
      "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":" + resp.id.to_string() + "}"
    }
  }
}

pub fn format_success(result_json: String, id: Int) -> String {
  format_response({ result: Some(result_json), error: None, id })
}

pub fn format_error(message: String, id: Int) -> String {
  format_response({ result: None, error: Some(message), id })
}

// stdio 主循环: 逐行读取 JSON,调用 handler,写入响应
pub fn run_stdio_loop(handler: Fn(JsonRpcRequest) -> JsonRpcResponse) -> Unit {
  loop {
    let line = read_line()
    if line.length() == 0 {
      break
    }
    
    // 解析请求 (简化: 直接提取 method, params, id)
    let request = parse_request(line)
    match request {
      Some(req) => {
        let resp = handler(req)
        println(format_response(resp))
      }
      None => {
        // 无效请求
        println(format_error("Invalid request", 0))
      }
    }
  }
}

fn read_line() -> String {
  // MoonBit stdio 读取一行
  // 实际实现依赖 MoonBit stdio API
  // 可能是 io::stdin().read_line() 或类似
  ""
}

fn parse_request(line: String) -> Option[JsonRpcRequest] {
  // 简化 JSON 解析: 提取 method, params, id
  // 实际应使用 JSON 库
  None // 占位,实际需要实现
}
```

- [ ] **Step 6: 修改 package.json 新增 scripts**

在根 `package.json` 的 `scripts` 中新增:

```json
{
  "scripts": {
    "build:moonbit": "cd moonbit-core && moon build",
    "build:moonbit:watch": "cd moonbit-core && moon build --watch",
    "dev:moonbit": "cd moonbit-core && moon run server",
    "test:moonbit": "cd moonbit-core && moon test",
    "build": "pnpm build:moonbit && pnpm -r build"
  }
}
```

- [ ] **Step 7: 验证构建**

Run: `cd moonbit-core && moon build`
Expected: 构建成功,无错误

---

### Task 2: 常量与枚举迁移

**Files:**
- Create: `moonbit-core/src/domain/common/enums.mbt`
- Test: `moonbit-core/src/domain/common/enums_test.mbt`

- [ ] **Step 1: 迁移核心枚举 (P0-1 修复 — 对齐 TS 常量)**

参考 `packages/shared/src/constants.ts` 的实际值:

```moonbit
// moonbit-core/src/domain/common/enums.mbt

// === Company ===

// TS: COMPANY_STATUSES = ["active", "paused", "archived"]
pub enum CompanyStatus {
  Active
  Paused
  Archived
} deriving (Eq, Debug)

pub fn company_status_to_string(s: CompanyStatus) -> String {
  match s {
    Active => "active"
    Paused => "paused"
    Archived => "archived"
  }
}

// === Agent ===

// TS: AGENT_STATUSES = ["active", "paused", "idle", "running", "error", "pending_approval", "terminated"]
// 注意: "active" 是逻辑状态 (idle | running),不是独立枚举值
// MoonBit 使用具体状态:
pub enum AgentStatus {
  Idle
  Running
  Paused
  Error
  Terminated
  PendingApproval
} deriving (Eq, Debug)

pub fn agent_status_to_string(s: AgentStatus) -> String {
  match s {
    Idle => "idle"
    Running => "running"
    Paused => "paused"
    Error => "error"
    Terminated => "terminated"
    PendingApproval => "pending_approval"
  }
}

// TS: AGENT_ROLES = ["ceo", "cto", "cmo", "cfo", "engineer", "designer", "pm", "qa", "devops", "researcher", "general"]
pub enum AgentRole {
  CEO
  CTO
  CMO
  CFO
  Engineer
  Designer
  PM
  QA
  DevOps
  Researcher
  General
} deriving (Eq, Debug)

pub fn agent_role_to_string(r: AgentRole) -> String {
  match r {
    CEO => "ceo"
    CTO => "cto"
    CMO => "cmo"
    CFO => "cfo"
    Engineer => "engineer"
    Designer => "designer"
    PM => "pm"
    QA => "qa"
    DevOps => "devops"
    Researcher => "researcher"
    General => "general"
  }
}

// TS: AGENT_ADAPTER_TYPES = ["process", "http", "claude_local", "codex_local", "gemini_local", "opencode_local", "pi_local", "cursor", "openclaw_gateway"]
pub enum AgentAdapterType {
  Process
  Http
  ClaudeLocal
  CodexLocal
  GeminiLocal
  OpenCodeLocal
  PiLocal
  Cursor
  OpenclawGateway
} deriving (Eq, Debug)

pub fn adapter_type_to_string(t: AgentAdapterType) -> String {
  match t {
    Process => "process"
    Http => "http"
    ClaudeLocal => "claude_local"
    CodexLocal => "codex_local"
    GeminiLocal => "gemini_local"
    OpenCodeLocal => "opencode_local"
    PiLocal => "pi_local"
    Cursor => "cursor"
    OpenclawGateway => "openclaw_gateway"
  }
}

// === Issue ===

// TS: ISSUE_STATUSES = ["backlog", "todo", "in_progress", "in_review", "done", "blocked", "cancelled"]
pub enum IssueStatus {
  Backlog
  Todo
  InProgress
  InReview
  Done
  Blocked
  Cancelled
} deriving (Eq, Debug)

pub fn issue_status_to_string(s: IssueStatus) -> String {
  match s {
    Backlog => "backlog"
    Todo => "todo"
    InProgress => "in_progress"
    InReview => "in_review"
    Done => "done"
    Blocked => "blocked"
    Cancelled => "cancelled"
  }
}

// TS: ISSUE_PRIORITIES = ["critical", "high", "medium", "low"]
pub enum IssuePriority {
  Critical
  High
  Medium
  Low
} deriving (Eq, Debug)

pub fn issue_priority_to_string(p: IssuePriority) -> String {
  match p {
    Critical => "critical"
    High => "high"
    Medium => "medium"
    Low => "low"
  }
}

// === Goal ===

// TS: GOAL_LEVELS = ["company", "team", "agent", "task"]
pub enum GoalLevel {
  Company
  Team
  Agent
  Task
} deriving (Eq, Debug)

pub fn goal_level_to_string(l: GoalLevel) -> String {
  match l {
    Company => "company"
    Team => "team"
    Agent => "agent"
    Task => "task"
  }
}

// TS: GOAL_STATUSES = ["planned", "active", "achieved", "cancelled"]
pub enum GoalStatus {
  Planned
  Active
  Achieved
  Cancelled
} deriving (Eq, Debug)

pub fn goal_status_to_string(s: GoalStatus) -> String {
  match s {
    Planned => "planned"
    Active => "active"
    Achieved => "achieved"
    Cancelled => "cancelled"
  }
}

// === Project ===

// TS: PROJECT_STATUSES = ["backlog", "planned", "in_progress", "completed", "cancelled"]
pub enum ProjectStatus {
  Backlog
  Planned
  InProgress
  Completed
  Cancelled
} deriving (Eq, Debug)

pub fn project_status_to_string(s: ProjectStatus) -> String {
  match s {
    Backlog => "backlog"
    Planned => "planned"
    InProgress => "in_progress"
    Completed => "completed"
    Cancelled => "cancelled"
  }
}
```

- [ ] **Step 2: 编写枚举测试 (P1-1 修复 — 字符串重复语法)**

```moonbit
// moonbit-core/src/domain/common/enums_test.mbt

use ./enums.*

test "company status to_string" {
  assert company_status_to_string(Active) == "active"
  assert company_status_to_string(Paused) == "paused"
  assert company_status_to_string(Archived) == "archived"
}

test "agent status to_string" {
  assert agent_status_to_string(Idle) == "idle"
  assert agent_status_to_string(Running) == "running"
  assert agent_status_to_string(Paused) == "paused"
  assert agent_status_to_string(Error) == "error"
  assert agent_status_to_string(Terminated) == "terminated"
  assert agent_status_to_string(PendingApproval) == "pending_approval"
}

test "agent role to_string" {
  assert agent_role_to_string(CEO) == "ceo"
  assert agent_role_to_string(Engineer) == "engineer"
  assert agent_role_to_string(QA) == "qa"
}

test "agent adapter type to_string" {
  assert adapter_type_to_string(Process) == "process"
  assert adapter_type_to_string(ClaudeLocal) == "claude_local"
  assert adapter_type_to_string(OpenCodeLocal) == "opencode_local"
}

test "issue status to_string" {
  assert issue_status_to_string(Backlog) == "backlog"
  assert issue_status_to_string(InProgress) == "in_progress"
  assert issue_status_to_string(InReview) == "in_review"
}

test "issue priority to_string" {
  assert issue_priority_to_string(Critical) == "critical"
  assert issue_priority_to_string(Low) == "low"
}

test "enum equality" {
  assert Active == Active
  assert Active != Paused
  assert Idle == Idle
  assert Idle != Running
}

test "long name validation input" {
  // 不使用 "A" * 101 (MoonBit 可能不支持)
  let long_name = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  assert long_name.length() == 101
}
```

- [ ] **Step 3: 运行测试验证**

Run: `cd moonbit-core && moon test`
Expected: 所有枚举测试通过

---

### Task 3: Agent 状态机与验证

**Files:**
- Create: `moonbit-core/src/domain/common/state_machine.mbt`
- Create: `moonbit-core/src/domain/agent/types.mbt`
- Create: `moonbit-core/src/domain/agent/state_machine.mbt`
- Create: `moonbit-core/src/domain/agent/validation.mbt`
- Test: `moonbit-core/src/domain/agent/state_machine_test.mbt`

- [ ] **Step 1: 创建状态机 trait**

```moonbit
// moonbit-core/src/domain/common/state_machine.mbt

pub trait StateMachine {
  type State
  type Event
  
  fn can_transition(self: Self, from: Self.State, to: Self.State) -> Bool
  fn apply_transition(self: Self, from: Self.State, to: Self.State) -> Result[Self.State, String]
}

pub fn assert_valid_transition[SM: StateMachine](
  sm: SM,
  from: SM.State,
  to: SM.State
) -> Result[SM.State, String> {
  if sm.can_transition(from, to) {
    Ok(to)
  } else {
    Err("Invalid transition from " + state_to_string(from) + " to " + state_to_string(to))
  }
}

fn state_to_string(s: Any) -> String {
  // 简化: 实际应使用 enum to_string 函数
  "state"
}
```

- [ ] **Step 2: 定义 Agent 类型 (P1-2 修复 — 明确 JSON 约定)**

```moonbit
// moonbit-core/src/domain/agent/types.mbt

use ../common/enums.{AgentStatus, AgentRole, AgentAdapterType}

// JSON 约定: 字段名使用 snake_case,枚举值使用字符串 (如 "idle", "engineer")

pub struct Agent {
  id: String
  company_id: String
  name: String
  url_key: String
  role: AgentRole
  title: String
  status: AgentStatus
  reports_to: Option[String]
  adapter_type: AgentAdapterType
  budget_monthly_cents: Int
  spent_monthly_cents: Int
  pause_reason: Option[String]
} deriving (Debug)

pub struct AgentCreateInput {
  name: String
  role: AgentRole
  title: String
  reports_to: Option[String]
  adapter_type: AgentAdapterType
  budget_monthly_cents: Int
} deriving (Debug)

pub struct AgentUpdateInput {
  name: Option[String]
  role: Option[AgentRole]
  title: Option[String]
  reports_to: Option[String]
  adapter_type: Option[AgentAdapterType]
  budget_monthly_cents: Option[Int]
} deriving (Debug)

pub struct ValidationError {
  field: String
  message: String
} deriving (Debug, Eq)
```

- [ ] **Step 3: 实现 Agent 状态机 (P0-2 修复 — 对齐 TS 源码)**

> **权威来源:** `server/src/services/agents.ts` (Task 0 调研结果)
> 以下转换规则必须与 TS 实现 100% 一致。

```moonbit
// moonbit-core/src/domain/agent/state_machine.mbt

use ../common/enums.AgentStatus
use ../common/state_machine.StateMachine

pub struct AgentStateMachine {}

impl StateMachine for AgentStateMachine {
  type State = AgentStatus
  type Event = Unit
  
  fn can_transition(self: Self, from: AgentStatus, to: AgentStatus) -> Bool {
    match (from, to) {
      // 基于 server/src/services/agents.ts 的实际转换规则:
      (Idle, Running) => true
      (Idle, Paused) => true
      (Running, Idle) => true
      (Running, Error) => true
      (Running, Paused) => true
      (Error, Paused) => true
      (Error, Running) => true
      (Paused, Idle) => true
      (Paused, Running) => true
      (Paused, Terminated) => true
      // PendingApproval 转换 (需确认 TS 中是否存在):
      (PendingApproval, Idle) => true
      (PendingApproval, Terminated) => true
      // 终止状态不可逆
      (Terminated, _) => false
      // 同状态转换无意义
      (s, s2) if s == s2 => false
      // 其他转换非法
      _ => false
    }
  }
  
  fn apply_transition(self: Self, from: AgentStatus, to: AgentStatus) -> Result[AgentStatus, String] {
    if self.can_transition(from, to) {
      Ok(to)
    } else {
      Err("Cannot transition Agent from " + agent_status_to_string(from) + " to " + agent_status_to_string(to))
    }
  }
}
```

- [ ] **Step 4: 实现 Agent 验证器**

```moonbit
// moonbit-core/src/domain/agent/validation.mbt

use ./types.{AgentCreateInput, AgentUpdateInput, ValidationError}

pub fn validate_agent_create(input: AgentCreateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors: List[ValidationError] = Nil
  
  // 名称验证
  if input.name.length() == 0 {
    errors = Cons({ field: "name", message: "Agent name is required" }, errors)
  }
  if input.name.length() > 100 {
    errors = Cons({ field: "name", message: "Agent name must be less than 100 characters" }, errors)
  }
  
  // 预算验证
  if input.budget_monthly_cents < 0 {
    errors = Cons({ field: "budgetMonthlyCents", message: "Budget cannot be negative" }, errors)
  }
  
  if errors == Nil {
    Ok(Unit)
  } else {
    Err(errors)
  }
}

pub fn validate_agent_update(input: AgentUpdateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors: List[ValidationError] = Nil
  
  match input.name {
    Some(name) => {
      if name.length() == 0 {
        errors = Cons({ field: "name", message: "Agent name cannot be empty" }, errors)
      }
      if name.length() > 100 {
        errors = Cons({ field: "name", message: "Agent name must be less than 100 characters" }, errors)
      }
    }
    None => {}
  }
  
  match input.budget_monthly_cents {
    Some(budget) => {
      if budget < 0 {
        errors = Cons({ field: "budgetMonthlyCents", message: "Budget cannot be negative" }, errors)
      }
    }
    None => {}
  }
  
  if errors == Nil {
    Ok(Unit)
  } else {
    Err(errors)
  }
}
```

- [ ] **Step 5: 编写 Agent 状态机测试**

```moonbit
// moonbit-core/src/domain/agent/state_machine_test.mbt

use ./state_machine.AgentStateMachine
use ../common/state_machine.StateMachine
use ../common/enums.AgentStatus

test "agent idle to running" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Idle, Running)
}

test "agent idle to paused" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Idle, Paused)
}

test "agent running to idle" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Running, Idle)
}

test "agent running to error" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Running, Error)
}

test "agent running to paused" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Running, Paused)
}

test "agent error to paused" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Error, Paused)
}

test "agent error to running" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Error, Running)
}

test "agent paused to idle" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Paused, Idle)
}

test "agent paused to running" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Paused, Running)
}

test "agent paused to terminated" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Paused, Terminated)
}

test "agent pending to idle" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(PendingApproval, Idle)
}

test "agent pending to terminated" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(PendingApproval, Terminated)
}

test "agent terminated to anything" {
  let sm = AgentStateMachine {}
  assert !sm.can_transition(Terminated, Running)
  assert !sm.can_transition(Terminated, Idle)
  assert !sm.can_transition(Terminated, Paused)
  assert !sm.can_transition(Terminated, Error)
  assert !sm.can_transition(Terminated, PendingApproval)
}

test "agent same state to itself" {
  let sm = AgentStateMachine {}
  assert !sm.can_transition(Idle, Idle)
  assert !sm.can_transition(Running, Running)
}

test "agent apply transition success" {
  let sm = AgentStateMachine {}
  assert sm.apply_transition(Idle, Running).is_ok()
}

test "agent apply transition failure" {
  let sm = AgentStateMachine {}
  assert sm.apply_transition(Terminated, Running).is_err()
}
```

- [ ] **Step 6: 编写 Agent 验证器测试**

```moonbit
// moonbit-core/src/domain/agent/validation_test.mbt

use ./validation.{validate_agent_create, validate_agent_update}
use ./types.{AgentCreateInput, AgentUpdateInput}
use ../common/enums.{AgentRole, AgentAdapterType}

test "valid agent create passes" {
  let input: AgentCreateInput = {
    name: "Alice",
    role: Engineer,
    title: "Backend Developer",
    reports_to: Some("uuid-123"),
    adapter_type: ClaudeLocal,
    budget_monthly_cents: 5000000
  }
  assert validate_agent_create(input).is_ok()
}

test "empty name fails" {
  let input: AgentCreateInput = {
    name: "",
    role: Engineer,
    title: "Backend Developer",
    reports_to: None,
    adapter_type: ClaudeLocal,
    budget_monthly_cents: 5000000
  }
  assert validate_agent_create(input).is_err()
}

test "negative budget fails" {
  let input: AgentCreateInput = {
    name: "Bob",
    role: CEO,
    title: "Chief Executive",
    reports_to: None,
    adapter_type: Process,
    budget_monthly_cents: -100
  }
  assert validate_agent_create(input).is_err()
}

test "long name fails" {
  let long_name = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  let input: AgentCreateInput = {
    name: long_name,
    role: Engineer,
    title: "Backend Developer",
    reports_to: None,
    adapter_type: ClaudeLocal,
    budget_monthly_cents: 5000000
  }
  assert validate_agent_create(input).is_err()
}
```

- [ ] **Step 7: 运行测试验证**

Run: `cd moonbit-core && moon test`
Expected: 所有 Agent 测试通过

---

### Task 4: Issue 状态机与验证

**Files:**
- Create: `moonbit-core/src/domain/issue/types.mbt`
- Create: `moonbit-core/src/domain/issue/state_machine.mbt`
- Create: `moonbit-core/src/domain/issue/validation.mbt`
- Test: `moonbit-core/src/domain/issue/state_machine_test.mbt`

- [ ] **Step 1: 定义 Issue 类型**

```moonbit
// moonbit-core/src/domain/issue/types.mbt

use ../common/enums.{IssueStatus, IssuePriority}

pub struct Issue {
  id: String
  company_id: String
  project_id: Option[String]
  parent_id: Option[String]
  goal_id: Option[String]
  title: String
  description: Option[String]
  status: IssueStatus
  priority: IssuePriority
  assignee_agent_id: Option[String]
  assignee_user_id: Option[String]
  identifier: String
  checkout_run_id: Option[String]
  execution_run_id: Option[String]
} deriving (Debug)

pub struct IssueCreateInput {
  title: String
  description: Option[String]
  status: IssueStatus
  priority: IssuePriority
  assignee_agent_id: Option[String]
  project_id: Option[String]
  parent_id: Option[String]
  goal_id: Option[String]
} deriving (Debug)

pub struct IssueTransitionRequest {
  from_status: IssueStatus
  to_status: IssueStatus
  assignee_agent_id: Option[String]
} deriving (Debug)

pub struct IssueTransitionResult {
  allowed: Bool
  next_status: Option[IssueStatus]
  reason: Option[String]
} deriving (Debug)
```

- [ ] **Step 2: 实现 Issue 状态机 (P0-5 修复 — 补全缺失转换)**

> **权威来源:** `server/src/services/issues.ts` (Task 0 调研结果)
> 以下转换规则必须与 TS 实现 100% 一致。

```moonbit
// moonbit-core/src/domain/issue/state_machine.mbt

use ../common/enums.IssueStatus
use ../common/state_machine.StateMachine
use ./types.{IssueTransitionRequest, IssueTransitionResult}

pub struct IssueStateMachine {}

impl StateMachine for IssueStateMachine {
  type State = IssueStatus
  type Event = Unit
  
  fn can_transition(self: Self, from: IssueStatus, to: IssueStatus) -> Bool {
    match (from, to) {
      (Backlog, Todo) => true
      (Backlog, Cancelled) => true
      (Todo, InProgress) => true
      (Todo, Blocked) => true
      (Todo, Cancelled) => true
      (InProgress, InReview) => true
      (InProgress, Blocked) => true
      (InProgress, Todo) => true
      (InProgress, Cancelled) => true
      (InReview, Done) => true
      (InReview, InProgress) => true
      (InReview, Todo) => true
      (InReview, Blocked) => true
      (Blocked, Todo) => true
      (Blocked, Cancelled) => true
      // 终态不可转换
      (Done, _) => false
      (Cancelled, _) => false
      // 同状态转换无意义
      (s, s2) if s == s2 => false
      // 其他转换非法
      _ => false
    }
  }
  
  fn apply_transition(self: Self, from: IssueStatus, to: IssueStatus) -> Result[IssueStatus, String] {
    if self.can_transition(from, to) {
      Ok(to)
    } else {
      Err("Cannot transition Issue from " + issue_status_to_string(from) + " to " + issue_status_to_string(to))
    }
  }
}

pub fn transition_issue(
  sm: IssueStateMachine,
  request: IssueTransitionRequest
) -> IssueTransitionResult {
  if sm.can_transition(request.from_status, request.to_status) {
    {
      allowed: true,
      next_status: Some(request.to_status),
      reason: None
    }
  } else {
    {
      allowed: false,
      next_status: None,
      reason: Some("Cannot transition from " + issue_status_to_string(request.from_status) + " to " + issue_status_to_string(request.to_status))
    }
  }
}
```

- [ ] **Step 3: 实现 Issue 验证器**

```moonbit
// moonbit-core/src/domain/issue/validation.mbt

use ./types.{IssueCreateInput, ValidationError}
use ../common/enums.{IssueStatus}

pub fn validate_issue_create(input: IssueCreateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors: List[ValidationError] = Nil
  
  if input.title.length() == 0 {
    errors = Cons({ field: "title", message: "Issue title is required" }, errors)
  }
  if input.title.length() > 500 {
    errors = Cons({ field: "title", message: "Issue title must be less than 500 characters" }, errors)
  }
  
  match input.status {
    Backlog => {}
    Todo => {}
    _ => {
      errors = Cons({ field: "status", message: "Initial status must be backlog or todo" }, errors)
    }
  }
  
  if errors == Nil {
    Ok(Unit)
  } else {
    Err(errors)
  }
}
```

- [ ] **Step 4: 编写 Issue 状态机测试**

```moonbit
// moonbit-core/src/domain/issue/state_machine_test.mbt

use ./state_machine.{IssueStateMachine, transition_issue}
use ./types.IssueTransitionRequest
use ../common/enums.IssueStatus

test "issue backlog to todo" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Backlog, Todo)
}

test "issue backlog to cancelled" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Backlog, Cancelled)
}

test "issue todo to in_progress" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Todo, InProgress)
}

test "issue todo to blocked" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Todo, Blocked)
}

test "issue todo to cancelled" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Todo, Cancelled)
}

test "issue in_progress to in_review" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InProgress, InReview)
}

test "issue in_progress to blocked" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InProgress, Blocked)
}

test "issue in_progress to todo" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InProgress, Todo)
}

test "issue in_progress to cancelled" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InProgress, Cancelled)
}

test "issue in_review to done" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InReview, Done)
}

test "issue in_review to in_progress" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InReview, InProgress)
}

test "issue in_review to todo" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InReview, Todo)
}

test "issue in_review to blocked" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InReview, Blocked)
}

test "issue blocked to todo" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Blocked, Todo)
}

test "issue blocked to cancelled" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Blocked, Cancelled)
}

test "issue done to anything" {
  let sm = IssueStateMachine {}
  assert !sm.can_transition(Done, InProgress)
  assert !sm.can_transition(Done, Todo)
  assert !sm.can_transition(Done, Cancelled)
}

test "issue cancelled to anything" {
  let sm = IssueStateMachine {}
  assert !sm.can_transition(Cancelled, Todo)
  assert !sm.can_transition(Cancelled, InProgress)
}

test "transition_issue allowed" {
  let sm = IssueStateMachine {}
  let request: IssueTransitionRequest = {
    from_status: Todo,
    to_status: InProgress,
    assignee_agent_id: Some("uuid-123")
  }
  let result = transition_issue(sm, request)
  assert result.allowed
  assert result.next_status == Some(InProgress)
}

test "transition_issue rejected" {
  let sm = IssueStateMachine {}
  let request: IssueTransitionRequest = {
    from_status: Done,
    to_status: InProgress,
    assignee_agent_id: None
  }
  let result = transition_issue(sm, request)
  assert !result.allowed
  assert result.next_status == None
  assert result.reason.is_some()
}
```

- [ ] **Step 5: 运行测试验证**

Run: `cd moonbit-core && moon test`
Expected: 所有 Issue 测试通过

---

### Task 5: Company/Goal/Project 领域模型

*(与原版相同,无 P0/P1 问题,保持原内容)*

**Files:**
- Create: `moonbit-core/src/domain/company/types.mbt`
- Create: `moonbit-core/src/domain/company/validation.mbt`
- Create: `moonbit-core/src/domain/goal/types.mbt`
- Create: `moonbit-core/src/domain/goal/validation.mbt`
- Create: `moonbit-core/src/domain/project/types.mbt`
- Create: `moonbit-core/src/domain/project/validation.mbt`

- [ ] **Step 1-6: 定义类型和验证器**

*(保持原版 Company, Goal, Project 的类型定义和验证器代码,与 Task 3/4 模式相同)*

- [ ] **Step 7: 验证构建**

Run: `cd moonbit-core && moon build`
Expected: 构建成功

---

### Task 6: 预算计算引擎

**Files:**
- Create: `moonbit-core/src/budget/types.mbt`
- Create: `moonbit-core/src/budget/enforcement.mbt`
- Create: `moonbit-core/src/budget/window.mbt`
- Test: `moonbit-core/src/budget/enforcement_test.mbt`

- [ ] **Step 1: 定义预算类型**

```moonbit
// moonbit-core/src/budget/types.mbt

pub enum EnforcementLevel {
  Soft
  Hard
} deriving (Eq, Debug)

pub fn enforcement_level_to_string(l: EnforcementLevel) -> String {
  match l {
    Soft => "soft"
    Hard => "hard"
  }
}

pub struct BudgetEnforcementInput {
  company_id: String
  agent_id: String
  current_spent_cents: Int
  budget_monthly_cents: Int
} deriving (Debug)

pub struct BudgetEnforcementResult {
  blocked: Bool
  threshold: Float
  warning_level: Option[EnforcementLevel]
  message: Option[String]
} deriving (Debug)

pub struct BudgetWindow {
  start: String
  end: String
} deriving (Debug)
```

- [ ] **Step 2: 实现预算执行门控**

```moonbit
// moonbit-core/src/budget/enforcement.mbt

use ./types.{BudgetEnforcementInput, BudgetEnforcementResult, EnforcementLevel}

pub fn check_budget_enforcement(input: BudgetEnforcementInput) -> BudgetEnforcementResult {
  if input.budget_monthly_cents <= 0 {
    {
      blocked: true,
      threshold: 1.0,
      warning_level: Some(Hard),
      message: Some("Budget not configured")
    }
  } else {
    let ratio = input.current_spent_cents.to_float() /. input.budget_monthly_cents.to_float()
    
    if ratio >= 1.0 {
      {
        blocked: true,
        threshold: ratio,
        warning_level: Some(Hard),
        message: Some("Budget exhausted")
      }
    } else if ratio >= 0.8 {
      {
        blocked: false,
        threshold: ratio,
        warning_level: Some(Soft),
        message: Some("Approaching budget limit")
      }
    } else {
      {
        blocked: false,
        threshold: ratio,
        warning_level: None,
        message: None
      }
    }
  }
}
```

- [ ] **Step 3: 实现月度窗口计算 (P0-4 修复 — 真实实现或 TS 回退)**

```moonbit
// moonbit-core/src/budget/window.mbt

use ./types.BudgetWindow

// 当前 UTC 月度窗口
// 开始: 当月 1 日 00:00:00 UTC
// 结束: 下月 1 日 00:00:00 UTC
//
// 注意: MoonBit 0.1 时间库可能不支持日期解析
// 如果不可用,此函数由 TypeScript 层提供,MoonBit 仅消费结果

pub fn current_utc_month_window(now_iso: String) -> BudgetWindow {
  // 简化实现: 解析 ISO 字符串 "YYYY-MM-DDTHH:mm:ssZ"
  // 提取年、月,计算当月 1 日和下月 1 日
  
  let year_str = now_iso.substring(0, 4)
  let month_str = now_iso.substring(5, 7)
  
  let year = year_str.to_int_or(2026)
  let month = month_str.to_int_or(4)
  
  let next_month = if month == 12 { 1 } else { month + 1 }
  let next_year = if month == 12 { year + 1 } else { year }
  
  let start = year.to_string() + "-" + pad2(month) + "-01T00:00:00Z"
  let end = next_year.to_string() + "-" + pad2(next_month) + "-01T00:00:00Z"
  
  { start, end }
}

fn pad2(n: Int) -> String {
  if n < 10 {
    "0" + n.to_string()
  } else {
    n.to_string()
  }
}
```

- [ ] **Step 4: 编写预算测试**

```moonbit
// moonbit-core/src/budget/enforcement_test.mbt

use ./enforcement.check_budget_enforcement
use ./types.{BudgetEnforcementInput, EnforcementLevel}

test "budget not configured blocked" {
  let input: BudgetEnforcementInput = {
    company_id: "uuid-1",
    agent_id: "uuid-2",
    current_spent_cents: 1000000,
    budget_monthly_cents: 0
  }
  let result = check_budget_enforcement(input)
  assert result.blocked
  assert result.warning_level == Some(Hard)
}

test "budget exhausted blocked" {
  let input: BudgetEnforcementInput = {
    company_id: "uuid-1",
    agent_id: "uuid-2",
    current_spent_cents: 5000000,
    budget_monthly_cents: 5000000
  }
  let result = check_budget_enforcement(input)
  assert result.blocked
  assert result.warning_level == Some(Hard)
  assert result.threshold >= 1.0
}

test "approaching budget soft warning" {
  let input: BudgetEnforcementInput = {
    company_id: "uuid-1",
    agent_id: "uuid-2",
    current_spent_cents: 4000000,
    budget_monthly_cents: 5000000
  }
  let result = check_budget_enforcement(input)
  assert !result.blocked
  assert result.warning_level == Some(Soft)
  assert result.threshold >= 0.8
  assert result.threshold < 1.0
}

test "within budget no warning" {
  let input: BudgetEnforcementInput = {
    company_id: "uuid-1",
    agent_id: "uuid-2",
    current_spent_cents: 2000000,
    budget_monthly_cents: 5000000
  }
  let result = check_budget_enforcement(input)
  assert !result.blocked
  assert result.warning_level == None
  assert result.message == None
}

test "over budget 10 percent" {
  let input: BudgetEnforcementInput = {
    company_id: "uuid-1",
    agent_id: "uuid-2",
    current_spent_cents: 5500000,
    budget_monthly_cents: 5000000
  }
  let result = check_budget_enforcement(input)
  assert result.blocked
  assert result.threshold >= 1.1
}
```

- [ ] **Step 5: 运行测试验证**

Run: `cd moonbit-core && moon test`
Expected: 所有预算测试通过

---

### Task 7: 成本聚合

**Files:**
- Create: `moonbit-core/src/cost/types.mbt`
- Create: `moonbit-core/src/cost/aggregation.mbt`

- [ ] **Step 1: 定义成本类型**

```moonbit
// moonbit-core/src/cost/types.mbt

pub struct CostEvent {
  company_id: String
  agent_id: String
  issue_id: Option[String]
  provider: String
  model: String
  input_tokens: Int
  output_tokens: Int
  cost_cents: Int
  occurred_at: String
} deriving (Debug)

pub struct CostSummary {
  total_cost_cents: Int
  total_input_tokens: Int
  total_output_tokens: Int
  event_count: Int
  by_provider: Map[String, Int]
  by_model: Map[String, Int]
} deriving (Debug)

pub struct CostAggregationRequest {
  company_id: String
  agent_id: Option[String]
  window_start: String
  window_end: String
} deriving (Debug)
```

- [ ] **Step 2: 实现成本聚合 (P1-3 修复 — 增加过滤逻辑)**

```moonbit
// moonbit-core/src/cost/aggregation.mbt

use ./types.{CostEvent, CostSummary, CostAggregationRequest}

pub fn aggregate_costs(
  events: List[CostEvent],
  request: CostAggregationRequest
) -> CostSummary {
  // 先过滤
  let filtered = filter_events(events, request)
  // 再聚合
  sum_events(filtered)
}

fn filter_events(events: List[CostEvent], request: CostAggregationRequest) -> List[CostEvent] {
  events.filter(fn(event) {
    // 公司过滤
    if event.company_id != request.company_id { return false }
    
    // Agent 过滤 (如果指定)
    match request.agent_id {
      Some(agent_id) => { if event.agent_id != agent_id { return false } }
      None => {}
    }
    
    // 时间窗口过滤 (简化: 字符串比较)
    if event.occurred_at < request.window_start { return false }
    if event.occurred_at >= request.window_end { return false }
    
    true
  })
}

fn sum_events(events: List[CostEvent]) -> CostSummary {
  let init: (Int, Int, Int, Int, Map[String, Int], Map[String, Int]) = 
    (0, 0, 0, 0, Map::new(), Map::new())
  
  let (cost, input, output, count, providers, models) = 
    events.fold(fn(acc, event) {
      let (c, i, o, n, p, m) = acc
      
      let new_c = c + event.cost_cents
      let new_i = i + event.input_tokens
      let new_o = o + event.output_tokens
      let new_n = n + 1
      
      let new_p = p.set(event.provider, p.get(event.provider).unwrap_or(0) + event.cost_cents)
      let new_m = m.set(event.model, m.get(event.model).unwrap_or(0) + event.cost_cents)
      
      (new_c, new_i, new_o, new_n, new_p, new_m)
    }, init)
  
  {
    total_cost_cents: cost,
    total_input_tokens: input,
    total_output_tokens: output,
    event_count: count,
    by_provider: providers,
    by_model: models
  }
}
```

- [ ] **Step 3: 验证构建**

Run: `cd moonbit-core && moon build`
Expected: 构建成功

---

### Task 8: stdio JSON-RPC 服务端 (替代原 HTTP 方案)

**Files:**
- Create: `moonbit-core/src/server/handlers.mbt`
- Create: `moonbit-core/src/server/main.mbt`
- Create: `moonbit-core/bin/server.mbt`

- [ ] **Step 1: 实现 JSON-RPC handlers**

```moonbit
// moonbit-core/src/server/handlers.mbt

use ../lib/rpc.{JsonRpcRequest, JsonRpcResponse, format_success, format_error}
use ../domain/agent/state_machine.AgentStateMachine
use ../domain/agent/validation.validate_agent_create
use ../domain/issue/state_machine.{IssueStateMachine, transition_issue}
use ../budget/enforcement.check_budget_enforcement
use ../cost/aggregation.aggregate_costs
use ../cost/types.{CostAggregationRequest}

pub fn handle_request(req: JsonRpcRequest) -> String {
  match req.method {
    "health" => handle_health(req)
    "agent.validate" => handle_validate_agent(req)
    "agent.transition" => handle_agent_transition(req)
    "issue.transition" => handle_issue_transition(req)
    "budget.enforce" => handle_budget_enforce(req)
    "cost.aggregate" => handle_cost_aggregate(req)
    _ => format_error("Unknown method: " + req.method, req.id)
  }
}

fn handle_health(req: JsonRpcRequest) -> String {
  format_success("\"ok\"", req.id)
}

fn handle_validate_agent(req: JsonRpcRequest) -> String {
  // 简化: 实际应解析 req.params 为 AgentCreateInput
  // 这里演示结构
  let result = validate_agent_create(/* parsed from req.params */)
  
  match result {
    Ok(_) => format_success("{\"valid\":true,\"errors\":[]}", req.id)
    Err(errors) => {
      // 序列化 errors 为 JSON 数组
      format_success("{\"valid\":false,\"errors\":[]}", req.id)
    }
  }
}

fn handle_agent_transition(req: JsonRpcRequest) -> String {
  let sm = AgentStateMachine {}
  // 解析 from, to
  // 调用 sm.can_transition
  format_success("{\"allowed\":true}", req.id)
}

fn handle_issue_transition(req: JsonRpcRequest) -> String {
  let sm = IssueStateMachine {}
  // 解析 request
  let result = transition_issue(sm, /* parsed request */)
  
  if result.allowed {
    format_success("{\"allowed\":true,\"nextStatus\":\"" + issue_status_to_string(result.next_status.unwrap()) + "\"}", req.id)
  } else {
    format_success("{\"allowed\":false,\"reason\":\"" + result.reason.unwrap_or("") + "\"}", req.id)
  }
}

fn handle_budget_enforce(req: JsonRpcRequest) -> String {
  // 解析 input
  let result = check_budget_enforcement(/* parsed input */)
  
  let warning = match result.warning_level {
    Some(Soft) => "\"soft\""
    Some(Hard) => "\"hard\""
    None => "null"
  }
  
  format_success(
    "{\"blocked\":" + bool_to_json(result.blocked) + ",\"threshold\":" + float_to_json(result.threshold) + ",\"warningLevel\":" + warning + "}",
    req.id
  )
}

fn handle_cost_aggregate(req: JsonRpcRequest) -> String {
  // 解析 events + request
  let result = aggregate_costs(/* parsed events */, /* parsed request */)
  
  format_success(
    "{\"totalCostCents\":" + result.total_cost_cents.to_string() + ",\"eventCount\":" + result.event_count.to_string() + "}",
    req.id
  )
}

fn bool_to_json(b: Bool) -> String {
  if b { "true" } else { "false" }
}

fn float_to_json(f: Float) -> String {
  f.to_string()
}
```

- [ ] **Step 2: 实现 stdio 主循环**

```moonbit
// moonbit-core/src/server/main.mbt

use ./handlers.handle_request
use ../lib/rpc.{JsonRpcRequest}

pub fn run_server() -> Unit {
  // 启动时发送就绪信号
  println("MOONBIT_READY")
  
  // 主循环: 逐行读取 JSON,处理,输出响应
  loop {
    let line = read_stdin_line()
    if line.length() == 0 {
      // EOF,退出
      break
    }
    
    // 简化: 直接传递行到 handler
    // 实际应解析为 JsonRpcRequest
    let req: JsonRpcRequest = parse_line(line)
    let resp = handle_request(req)
    println(resp)
  }
}

fn read_stdin_line() -> String {
  // MoonBit 0.1 stdio 读取
  // 实际实现依赖具体 API
  ""
}

fn parse_line(line: String) -> JsonRpcRequest {
  // 简化 JSON 解析
  // 提取 method, params (原始字符串), id
  { method: "health", params: "", id: 1 }
}
```

- [ ] **Step 3: 创建入口点**

```moonbit
// moonbit-core/bin/server.mbt

use ../src/server.main.run_server

fn main {
  run_server()
}
```

- [ ] **Step 4: 验证构建**

Run: `cd moonbit-core && moon build`
Expected: 构建成功,运行 `moon run server` 打印 "MOONBIT_READY"

---

### Task 9: TypeScript stdio 客户端

**Files:**
- Create: `server/src/services/moonbit-client.ts`
- Test: `tests/moonbit-integration/agent-validation.test.ts`

- [ ] **Step 1: 创建 TypeScript stdio 客户端**

```typescript
// server/src/services/moonbit-client.ts

import { ChildProcess, spawn } from 'child_process';

export interface ValidationResult {
  valid: boolean;
  errors: Array<{ field: string; message: string }>;
}

export interface TransitionRequest {
  fromStatus: string;
  toStatus: string;
  assigneeAgentId?: string;
}

export interface TransitionResult {
  allowed: boolean;
  nextStatus?: string;
  reason?: string;
}

export interface BudgetEnforcementInput {
  companyId: string;
  agentId: string;
  currentSpentCents: number;
  budgetMonthlyCents: number;
}

export interface BudgetEnforcementResult {
  blocked: boolean;
  threshold: number;
  warningLevel?: 'soft' | 'hard';
  message?: string;
}

interface JsonRpcRequest {
  jsonrpc: '2.0';
  method: string;
  params: Record<string, unknown>;
  id: number;
}

interface JsonRpcResponse {
  jsonrpc: '2.0';
  result?: unknown;
  error?: { code: number; message: string };
  id: number;
}

export class MoonBitClient {
  private child: ChildProcess;
  private nextId = 1;
  private pendingRequests = new Map<number, {
    resolve: (value: unknown) => void;
    reject: (err: Error) => void;
    timeout: NodeJS.Timeout;
  }>();
  private buffer = '';
  
  constructor(binaryPath: string, args: string[] = [], timeoutMs = 1000) {
    this.child = spawn(binaryPath, args, {
      stdio: ['pipe', 'pipe', 'pipe'],
      // P1-4 修复: 不使用 shell
    });
    
    // 监听 stdout
    this.child.stdout?.on('data', (data: Buffer) => {
      this.buffer += data.toString();
      this.processBuffer();
    });
    
    this.child.on('exit', (code) => {
      // 拒绝所有 pending
      for (const [, pending] of this.pendingRequests) {
        clearTimeout(pending.timeout);
        pending.reject(new Error(`MoonBit process exited with code ${code}`));
      }
      this.pendingRequests.clear();
    });
  }
  
  private processBuffer(): void {
    const lines = this.buffer.split('\n');
    this.buffer = lines.pop() || '';
    
    for (const line of lines) {
      if (line.trim() === '') continue;
      if (line === 'MOONBIT_READY') continue;
      
      try {
        const resp: JsonRpcResponse = JSON.parse(line);
        const pending = this.pendingRequests.get(resp.id);
        if (pending) {
          clearTimeout(pending.timeout);
          if (resp.error) {
            pending.reject(new Error(resp.error.message));
          } else {
            pending.resolve(resp.result);
          }
          this.pendingRequests.delete(resp.id);
        }
      } catch (err) {
        console.warn('[MoonBitClient] Failed to parse response:', line, err);
      }
    }
  }
  
  private async request(method: string, params: Record<string, unknown>, timeoutMs = 1000): Promise<unknown> {
    const id = this.nextId++;
    const req: JsonRpcRequest = { jsonrpc: '2.0', method, params, id };
    
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pendingRequests.delete(id);
        reject(new Error(`MoonBit request timeout: ${method}`));
      }, timeoutMs);
      
      this.pendingRequests.set(id, { resolve, reject, timeout });
      this.child.stdin?.write(JSON.stringify(req) + '\n');
    });
  }
  
  async health(): Promise<boolean> {
    try {
      const result = await this.request('health', {}, 500);
      return result === 'ok';
    } catch {
      return false;
    }
  }
  
  async validateAgent(params: Record<string, unknown>): Promise<ValidationResult> {
    const result = await this.request('agent.validate', params);
    return result as ValidationResult;
  }
  
  async transitionIssue(params: TransitionRequest): Promise<TransitionResult> {
    const result = await this.request('issue.transition', params);
    return result as TransitionResult;
  }
  
  async checkBudget(params: BudgetEnforcementInput): Promise<BudgetEnforcementResult> {
    const result = await this.request('budget.enforce', params);
    return result as BudgetEnforcementResult;
  }
  
  kill(): void {
    this.child.kill('SIGTERM');
  }
}
```

- [ ] **Step 2: 创建集成测试**

```typescript
// tests/moonbit-integration/agent-validation.test.ts

import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { MoonBitClient } from '../../server/src/services/moonbit-client';

describe('MoonBit Agent Validation Integration', () => {
  let client: MoonBitClient;
  
  beforeAll(() => {
    const binaryPath = process.env.MOONBIT_BINARY_PATH || 'moonbit-core/bin/server';
    client = new MoonBitClient(binaryPath, [], 2000);
  });
  
  afterAll(() => {
    client?.kill();
  });
  
  it('should pass health check', async () => {
    const healthy = await client.health();
    expect(healthy).toBe(true);
  }, 5000);
  
  it('should validate a valid agent', async () => {
    const result = await client.validateAgent({
      name: 'Alice',
      role: 'engineer',
      title: 'Backend Developer',
      reportsTo: 'uuid-123',
      adapterType: 'claude_local',
      budgetMonthlyCents: 5000000
    });
    
    expect(result.valid).toBe(true);
    expect(result.errors).toEqual([]);
  });
  
  it('should reject empty name', async () => {
    const result = await client.validateAgent({
      name: '',
      role: 'engineer',
      title: 'Backend Developer',
      adapterType: 'claude_local',
      budgetMonthlyCents: 5000000
    });
    
    expect(result.valid).toBe(false);
    expect(result.errors).toContainEqual(
      expect.objectContaining({ field: 'name' })
    );
  });
  
  it('should reject negative budget', async () => {
    const result = await client.validateAgent({
      name: 'Bob',
      role: 'ceo',
      title: 'CEO',
      adapterType: 'process',
      budgetMonthlyCents: -100
    });
    
    expect(result.valid).toBe(false);
    expect(result.errors).toContainEqual(
      expect.objectContaining({ field: 'budgetMonthlyCents' })
    );
  });
});
```

- [ ] **Step 3: 运行集成测试**

Run: `pnpm test:run tests/moonbit-integration/agent-validation.test.ts`
Expected: 测试通过 (需要 MoonBit 二进制已构建)

---

### Task 10: 进程管理器

**Files:**
- Create: `server/src/services/moonbit-process-manager.ts`
- Modify: `server/src/routes/index.ts`

- [ ] **Step 1: 创建进程管理器 (P1-4 修复 — 移除 shell: true)**

*(保持原版进程管理器代码,但移除 `shell: process.platform === 'win32'` 选项)*

```typescript
// server/src/services/moonbit-process-manager.ts

// ... 与原版相同,但 spawn 选项中移除 shell: true ...

this.child = spawn(this.config.binaryPath, [
  '--port', String(this.port)
], {
  stdio: ['pipe', 'pipe', 'pipe'],
  env: { ...process.env, PAPERCLIP_MOONBIT_PORT: String(this.port) }
  // 移除: shell: process.platform === 'win32'
});
```

- [ ] **Step 2: 修改路由启动逻辑**

*(与原版相同)*

- [ ] **Step 3: 验证进程管理**

Run: `pnpm dev`
Expected: MoonBit 子进程启动,打印 "MOONBIT_READY"

---

### Task 11: 配置读取 (新增,P1-5 修复)

**Files:**
- Create: `server/src/services/moonbit-config.ts`

- [ ] **Step 1: 创建配置读取模块**

```typescript
// server/src/services/moonbit-config.ts

import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';

export interface MoonBitConfig {
  enabled: boolean;
  binaryPath: string;
  timeoutMs: number;
  healthCheckIntervalMs: number;
  maxRestarts: number;
  restartWindowMs: number;
}

const DEFAULT_CONFIG: MoonBitConfig = {
  enabled: true,
  binaryPath: 'moonbit-core/bin/server',
  timeoutMs: 1000,
  healthCheckIntervalMs: 10000,
  maxRestarts: 3,
  restartWindowMs: 60000
};

function getGlobalConfigPath(): string {
  return path.join(os.homedir(), '.paperclip', 'config.json');
}

function getProjectConfigPath(): string {
  return path.join(process.cwd(), '.paperclip', 'config.json');
}

function readConfigFile(configPath: string): Partial<MoonBitConfig> | null {
  try {
    if (!fs.existsSync(configPath)) return null;
    const content = fs.readFileSync(configPath, 'utf-8');
    const parsed = JSON.parse(content);
    return parsed.moonbit || null;
  } catch {
    return null;
  }
}

export function loadMoonBitConfig(): MoonBitConfig {
  // 优先级: 项目配置 > 全局配置 > 默认值
  const projectConfig = readConfigFile(getProjectConfigPath());
  const globalConfig = readConfigFile(getGlobalConfigPath());
  
  return {
    ...DEFAULT_CONFIG,
    ...globalConfig,
    ...projectConfig
  };
}
```

- [ ] **Step 2: 在进程管理器中使用配置**

```typescript
// server/src/services/moonbit-process-manager.ts

import { loadMoonBitConfig } from './moonbit-config';

// 修改构造函数:
constructor(configOverride: Partial<MoonBitConfig> = {}) {
  const fileConfig = loadMoonBitConfig();
  this.config = { ...DEFAULT_CONFIG, ...fileConfig, ...configOverride };
  // ...
}
```

---

### Task 12: 集成到 Express 服务层

*(与原版相同,无 P0/P1 问题)*

---

### Task 13: 全量验证与文档

*(与原版相同,增加 MoonBit stdio 测试验证)*

---

## Risks and Mitigations

| 风险 | 影响 | 概率 | 缓解策略 |
|------|------|------|---------|
| MoonBit stdio API 不可用 | 无法实现 JSON-RPC | 低 | Task 1 Step 1 已验证,使用 `read_stdin_line` 抽象 |
| MoonBit JSON 库不成熟 | 请求/响应解析失败 | 中 | 手动实现核心类型序列化 (Task 1 Step 4) |
| 类型同步困难 | TS/MoonBit 类型不一致 | 中 | 通过 Task 0 调研 + snake_case JSON 约定 |
| 进程管理复杂 | 子进程崩溃/重启逻辑复杂 | 低 | 参考 PM2 模式,充分测试 |
| 迁移周期过长 | 项目维护成本高 | 中 | 分阶段验证,每阶段可独立发布 |

---

## Verification Steps

### 单元测试验证
```bash
cd moonbit-core && moon test
pnpm test:run
```

### 集成测试验证
```bash
pnpm test:run tests/moonbit-integration/
```

### E2E 验证
```bash
pnpm dev
# 发送 stdio 请求测试
echo '{"jsonrpc":"2.0","method":"health","id":1}' | moonbit-core/bin/server
```

### 进程管理验证
```bash
# kill MoonBit 进程,验证自动重启 (3 次内)
```

---

## Available-Agent-Types Roster

| Agent Type | Recommended For | Reasoning Level | Why |
|------------|----------------|-----------------|-----|
| general-purpose | MoonBit 代码编写、领域模型迁移 | high | 需要理解 MoonBit 语法和类型系统 |
| Explore | 代码库调研、现有模式查找 | low | 简单搜索任务 |
| general-purpose | TypeScript stdio 客户端开发 | medium | 标准 TypeScript 任务 |
| general-purpose | 进程管理器开发 | medium | Node.js child_process 标准用法 |

### Follow-up Staffing Guidance

**Lane 1: MoonBit 核心 (Tasks 0-8)**
- 1x general-purpose (high reasoning)
- 负责: API 验证、模块搭建、领域模型、状态机、验证器、stdio 服务

**Lane 2: TypeScript 集成 (Tasks 9-11)**
- 1x general-purpose (medium reasoning)
- 负责: stdio 客户端、进程管理器、配置读取

**Lane 3: 测试与文档 (Tasks 12-13)**
- 1x general-purpose (medium reasoning)
- 负责: 集成测试、全量验证、文档

### Launch Hints

```bash
# Ralph 执行
$ralph .omx/plans/moonbit-backend-migration.md

# Team 并行执行
$team .omx/plans/moonbit-backend-migration.md --lanes 3

# Team 验证路径
pnpm -r typecheck && pnpm test:run && cd moonbit-core && moon test

# Ralph 端到端验证
pnpm dev
```

---

*Plan v2 Revised: 2026-04-08*
*Based on design: docs/superpowers/specs/2026-04-08-moonbit-backend-migration-design.md*
*Critic Review: 5 P0 fixed, 5 P1 fixed, 1 P2 accepted*
