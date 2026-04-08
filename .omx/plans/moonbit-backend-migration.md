# MoonBit 后端迁移实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use executing-plans or subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Paperclip 后端核心领域模型、预算计算、共享验证器从 TypeScript 渐进式迁移到 MoonBit,通过 HTTP/JSON REST 与现有 Express 系统集成,减少约 20-30% pnpm 包依赖。

**Architecture:** 独立进程架构 — MoonBit 编译为独立二进制,作为 Express 子进程运行,通过 HTTP/JSON REST 通信。迁移顺序:常量 → 领域模型 → 预算计算 → 验证器 → 集成测试。

**Tech Stack:** MoonBit 0.1.20260403, Node.js 20+, Express 5, TypeScript 5.7, pnpm 9.15, PostgreSQL/PGlite

**设计文档:** `docs/superpowers/specs/2026-04-08-moonbit-backend-migration-design.md`

---

## RALPLAN-DR Summary

### Principles
1. **渐进式迁移** — 保持现有系统运行,逐步替换,每阶段可独立验证
2. **契约优先** — MoonBit 与 TypeScript 通过 JSON Schema 对齐类型契约
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

#### Option A: 独立进程 + HTTP/JSON (✓ 选择)
**Pros:** 解耦、可独立测试、MoonBit 生态要求低、调试方便、跨平台  
**Cons:** 网络开销 (~5ms)、需要进程管理逻辑

#### Option B: WASM + Node.js FFI
**Pros:** 零网络开销、类型安全、同进程部署  
**Cons:** MoonBit WASM 绑定代码复杂、调试困难、Windows 兼容性问题

#### Option C: 原生库 + N-API
**Pros:** 直接调用、无序列化  
**Cons:** N-API 绑定复杂、平台绑定、Windows DLL 问题

**Invalidation Rationale:**  
- Option B 被排除: MoonBit WASM 生态仍在发展中,绑定代码开发成本高,不适合渐进式迁移
- Option C 被排除: N-API 在 Windows 环境稳定性未知,且绑定代码维护成本高

---

## Acceptance Criteria

| # | 标准 | 验证方法 |
|---|------|---------|
| AC1 | MoonBit 模块可构建 | `cd moonbit-core && moon build` 成功 |
| AC2 | MoonBit HTTP 服务可启动 | `/health` 返回 200 |
| AC3 | 常量 100% 迁移 | MoonBit 枚举值与 `constants.ts` 一致 |
| AC4 | Agent 状态机 100% 覆盖 | 所有转换规则测试通过 |
| AC5 | Issue 状态机 100% 覆盖 | 所有转换规则测试通过 |
| AC6 | 预算计算与 TS 行为一致 | 相同输入产生相同输出 |
| AC7 | Express 集成调用成功 | TypeScript 客户端调用 MoonBit 端点成功 |
| AC8 | 现有 Vitest 套件无破坏 | `pnpm test:run` 通过 |
| AC9 | 进程管理器自动重启 | 崩溃后 3 次内自动恢复 |
| AC10 | `pnpm dev` 一键启动 | Express + MoonBit 同时启动 |

---

## File Structure Mapping

### 新增文件
```
moonbit-core/
├── moon.mod.json                     # MoonBit 模块定义
├── moon.pkg.json                     # 包配置
├── README.md                         # 模块说明
├── src/
│   ├── lib/                          # 工具库
│   │   ├── json.mbt                  # JSON 序列化/反序列化
│   │   └── http.mbt                  # HTTP 工具
│   ├── domain/                       # 领域模型
│   │   ├── common/                   # 共享领域
│   │   │   ├── enums.mbt             # 枚举常量 (迁移自 constants.ts)
│   │   │   ├── state_machine.mbt     # 状态机 trait
│   │   │   └── validation.mbt        # 验证器基础
│   │   ├── agent/                    # Agent 领域
│   │   │   ├── types.mbt             # Agent 类型定义
│   │   │   ├── state_machine.mbt     # Agent 状态机
│   │   │   └── validation.mbt        # Agent 验证器
│   │   ├── issue/                    # Issue 领域
│   │   │   ├── types.mbt             # Issue 类型定义
│   │   │   ├── state_machine.mbt     # Issue 状态机
│   │   │   └── validation.mbt        # Issue 验证器
│   │   ├── company/                  # Company 领域
│   │   │   ├── types.mbt
│   │   │   └── validation.mbt
│   │   ├── goal/                     # Goal 领域
│   │   │   ├── types.mbt
│   │   │   └── validation.mbt
│   │   └── project/                  # Project 领域
│   │       ├── types.mbt
│   │       └── validation.mbt
│   ├── budget/                       # 预算计算
│   │   ├── types.mbt                 # 预算类型
│   │   ├── enforcement.mbt           # 预算执行门控
│   │   └── window.mbt                # 月度窗口计算
│   ├── cost/                         # 成本聚合
│   │   ├── types.mbt
│   │   └── aggregation.mbt
│   └── http/                         # HTTP 服务端
│       ├── router.mbt                # 路由定义
│       ├── handlers.mbt              # 请求处理器
│       └── server.mbt                # HTTP 服务器
└── bin/
    └── server.mbt                    # 入口点
```

### 修改文件
```
server/src/services/
├── moonbit-client.ts                 # [新增] TypeScript HTTP 客户端
├── moonbit-process-manager.ts        # [新增] 进程管理器
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

### Task 1: MoonBit 模块基础设施

**Files:**
- Create: `moonbit-core/moon.mod.json`
- Create: `moonbit-core/moon.pkg.json`
- Create: `moonbit-core/README.md`
- Create: `moonbit-core/src/lib/json.mbt`
- Create: `moonbit-core/src/lib/http.mbt`
- Modify: `package.json` (新增 scripts)

- [ ] **Step 1: 创建 moon.mod.json**

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

- [ ] **Step 2: 创建 moon.pkg.json**

```json
{
  "is-main": false,
  "import": [],
  "link": {}
}
```

- [ ] **Step 3: 创建 README.md**

```markdown
# MoonBit Core - Paperclip Domain Model

MoonBit implementation of Paperclip's core domain models, state machines, and business logic.

## Build

```bash
moon build
```

## Test

```bash
moon test
```

## Run Server

```bash
moon run server
```
```

- [ ] **Step 4: 创建 JSON 工具模块**

```moonbit
// moonbit-core/src/lib/json.mbt
pub fn encode[T](value: T) -> String {
  // 使用 MoonBit 内置 JSON 编码
  // 注意: MoonBit 0.1 可能需手动实现或使用 stdlib
  value.to_string()
}

pub fn decode[T](json: String) -> Result[T, String] {
  // JSON 解码
  // 实际实现依赖 MoonBit JSON 库
  Ok(unsafe_from_string(json))
}
```

- [ ] **Step 5: 创建 HTTP 工具模块**

```moonbit
// moonbit-core/src/lib/http.mbt
pub struct HttpResponse {
  status: Int
  headers: Map[String, String]
  body: String
}

pub fn json_response(status: Int, body: String) -> HttpResponse {
  let headers = {
    let mut m = Map::new()
    m["Content-Type"] = "application/json"
    m["Content-Length"] = body.length().to_string()
    m
  }
  { status, headers, body }
}

pub fn parse_json_body(body: String) -> Result[Map[String, Value], String] {
  // 解析 JSON 请求体
  Json::parse(body)
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

- [ ] **Step 1: 迁移核心枚举**

参考 `packages/shared/src/constants.ts`,迁移以下枚举:

```moonbit
// moonbit-core/src/domain/common/enums.mbt

// Company Status
pub enum CompanyStatus {
  Active
  Paused
  Archived
} deriving (Eq, Debug, String)

// Agent Status
pub enum AgentStatus {
  Idle
  Running
  Paused
  Error
  Terminated
  PendingApproval
} deriving (Eq, Debug, String)

// Agent Role
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
} deriving (Eq, Debug, String)

// Agent Adapter Type
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
} deriving (Eq, Debug, String)

// Issue Status
pub enum IssueStatus {
  Backlog
  Todo
  InProgress
  InReview
  Done
  Blocked
  Cancelled
} deriving (Eq, Debug, String)

// Issue Priority
pub enum IssuePriority {
  Critical
  High
  Medium
  Low
} deriving (Eq, Debug, String)

// Goal Level
pub enum GoalLevel {
  Company
  Team
  Agent
  Task
} deriving (Eq, Debug, String)

// Goal Status
pub enum GoalStatus {
  Planned
  Active
  Achieved
  Cancelled
} deriving (Eq, Debug, String)

// Project Status
pub enum ProjectStatus {
  Backlog
  Planned
  InProgress
  Completed
  Cancelled
} deriving (Eq, Debug, String)

// 枚举与字符串的转换
pub trait ToString {
  fn to_string(self: Self) -> String
}

impl ToString for CompanyStatus {
  fn to_string(self: Self) -> String {
    match self {
      Active => "active"
      Paused => "paused"
      Archived => "archived"
    }
  }
}

impl ToString for AgentStatus {
  fn to_string(self: Self) -> String {
    match self {
      Idle => "idle"
      Running => "running"
      Paused => "paused"
      Error => "error"
      Terminated => "terminated"
      PendingApproval => "pending_approval"
    }
  }
}

// ...其他枚举的 ToString 实现
```

- [ ] **Step 2: 编写枚举测试**

```moonbit
// moonbit-core/src/domain/common/enums_test.mbt

test "company status active to_string" {
  assert CompanyStatus::Active.to_string() == "active"
}

test "company status paused to_string" {
  assert CompanyStatus::Paused.to_string() == "paused"
}

test "agent status idle to_string" {
  assert AgentStatus::Idle.to_string() == "idle"
}

test "agent status running to_string" {
  assert AgentStatus::Running.to_string() == "running"
}

test "issue status backlog to_string" {
  assert IssueStatus::Backlog.to_string() == "backlog"
}

test "issue priority critical to_string" {
  assert IssuePriority::Critical.to_string() == "critical"
}

test "enum equality company status" {
  assert CompanyStatus::Active == CompanyStatus::Active
  assert CompanyStatus::Active != CompanyStatus::Paused
}

test "enum equality agent status" {
  assert AgentStatus::Idle == AgentStatus::Idle
  assert AgentStatus::Idle != AgentStatus::Running
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

// 通用状态转换验证
pub fn assert_valid_transition[SM: StateMachine](
  sm: SM,
  from: SM.State,
  to: SM.State
) -> Result[SM.State, String> {
  if sm.can_transition(from, to) {
    Ok(to)
  } else {
    Err("Invalid transition from \(from) to \(to)")
  }
}
```

- [ ] **Step 2: 定义 Agent 类型**

```moonbit
// moonbit-core/src/domain/agent/types.mbt

use ../common/enums.{AgentStatus, AgentRole, AgentAdapterType}

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

- [ ] **Step 3: 实现 Agent 状态机**

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
      (PendingApproval, Idle) => true
      (PendingApproval, Terminated) => true
      (_, Terminated) => true
      // 终止状态不可逆
      (Terminated, _) => false
      // 其他转换非法
      _ => false
    }
  }
  
  fn apply_transition(self: Self, from: AgentStatus, to: AgentStatus) -> Result[AgentStatus, String] {
    if self.can_transition(from, to) {
      Ok(to)
    } else {
      Err("Cannot transition Agent from \(from) to \(to)")
    }
  }
}
```

- [ ] **Step 4: 实现 Agent 验证器**

```moonbit
// moonbit-core/src/domain/agent/validation.mbt

use ./types.{AgentCreateInput, AgentUpdateInput, ValidationError}
use ../common/enums.{AgentRole, AgentAdapterType}

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
  
  // 名称验证 (如果提供)
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
  
  // 预算验证 (如果提供)
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

test "agent can transition from idle to running" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Idle, Running)
}

test "agent can transition from idle to paused" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Idle, Paused)
}

test "agent can transition from running to idle" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Running, Idle)
}

test "agent can transition from running to error" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Running, Error)
}

test "agent can transition from running to paused" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Running, Paused)
}

test "agent can transition from error to paused" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Error, Paused)
}

test "agent can transition from error to running" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Error, Running)
}

test "agent can transition from paused to idle" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Paused, Idle)
}

test "agent can transition from paused to running" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Paused, Running)
}

test "agent can transition from paused to terminated" {
  let sm = AgentStateMachine {}
  assert sm.can_transition(Paused, Terminated)
}

test "agent cannot transition from terminated to anything" {
  let sm = AgentStateMachine {}
  assert !sm.can_transition(Terminated, Running)
  assert !sm.can_transition(Terminated, Idle)
  assert !sm.can_transition(Terminated, Paused)
  assert !sm.can_transition(Terminated, Error)
}

test "agent cannot transition from idle to done" {
  // done 不是 Agent 状态,是 Issue 状态
  let sm = AgentStateMachine {}
  assert !sm.can_transition(Idle, Idle) // 同状态转换无意义
}

test "agent apply transition success" {
  let sm = AgentStateMachine {}
  let result = sm.apply_transition(Idle, Running)
  assert result.is_ok()
}

test "agent apply transition failure" {
  let sm = AgentStateMachine {}
  let result = sm.apply_transition(Terminated, Running)
  assert result.is_err()
}
```

- [ ] **Step 6: 编写 Agent 验证器测试**

```moonbit
// moonbit-core/src/domain/agent/validation_test.mbt

use ./validation.{validate_agent_create, validate_agent_update}
use ./types.{AgentCreateInput, AgentUpdateInput}
use ../common/enums.{AgentRole, AgentAdapterType}

test "valid agent create passes validation" {
  let input = {
    name: "Alice",
    role: Engineer,
    title: "Backend Developer",
    reports_to: Some("uuid-123"),
    adapter_type: ClaudeLocal,
    budget_monthly_cents: 5000000
  }
  let result = validate_agent_create(input)
  assert result.is_ok()
}

test "empty name fails validation" {
  let input = {
    name: "",
    role: Engineer,
    title: "Backend Developer",
    reports_to: None,
    adapter_type: ClaudeLocal,
    budget_monthly_cents: 5000000
  }
  let result = validate_agent_create(input)
  assert result.is_err()
}

test "negative budget fails validation" {
  let input = {
    name: "Bob",
    role: CEO,
    title: "Chief Executive",
    reports_to: None,
    adapter_type: Process,
    budget_monthly_cents: -100
  }
  let result = validate_agent_create(input)
  assert result.is_err()
}

test "long name fails validation" {
  let long_name = "A" * 101
  let input = {
    name: long_name,
    role: Engineer,
    title: "Backend Developer",
    reports_to: None,
    adapter_type: ClaudeLocal,
    budget_monthly_cents: 5000000
  }
  let result = validate_agent_create(input)
  assert result.is_err()
}
```

- [ ] **Step 7: 运行测试验证**

Run: `cd moonbit-core && moon test`
Expected: 所有 Agent 状态机和验证测试通过

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

use ../common/enums.{IssueStatus, IssuePriority, GoalLevel}

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

- [ ] **Step 2: 实现 Issue 状态机**

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
      (InReview, Done) => true
      (InReview, InProgress) => true
      (InReview, Todo) => true
      (Blocked, Todo) => true
      (Blocked, Cancelled) => true
      (Done, Cancelled) => false // 终态不可转换
      (Cancelled, _) => false // 终态不可转换
      // 其他转换非法
      _ => false
    }
  }
  
  fn apply_transition(self: Self, from: IssueStatus, to: IssueStatus) -> Result[IssueStatus, String] {
    if self.can_transition(from, to) {
      Ok(to)
    } else {
      Err("Cannot transition Issue from \(from) to \(to)")
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
      reason: Some("Cannot transition from \(request.from_status) to \(request.to_status)")
    }
  }
}
```

- [ ] **Step 3: 实现 Issue 验证器**

```moonbit
// moonbit-core/src/domain/issue/validation.mbt

use ./types.{IssueCreateInput, ValidationError}
use ../common/enums.{IssueStatus, IssuePriority}

pub fn validate_issue_create(input: IssueCreateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors: List[ValidationError] = Nil
  
  // 标题验证
  if input.title.length() == 0 {
    errors = Cons({ field: "title", message: "Issue title is required" }, errors)
  }
  if input.title.length() > 500 {
    errors = Cons({ field: "title", message: "Issue title must be less than 500 characters" }, errors)
  }
  
  // 初始状态验证
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

test "issue can transition from backlog to todo" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Backlog, Todo)
}

test "issue can transition from backlog to cancelled" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Backlog, Cancelled)
}

test "issue can transition from todo to in_progress" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Todo, InProgress)
}

test "issue can transition from todo to blocked" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Todo, Blocked)
}

test "issue can transition from in_progress to in_review" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InProgress, InReview)
}

test "issue can transition from in_progress to blocked" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InProgress, Blocked)
}

test "issue can transition from in_review to done" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InReview, Done)
}

test "issue can transition from in_review to in_progress" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(InReview, InProgress)
}

test "issue can transition from blocked to todo" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Blocked, Todo)
}

test "issue can transition from blocked to cancelled" {
  let sm = IssueStateMachine {}
  assert sm.can_transition(Blocked, Cancelled)
}

test "issue cannot transition from done" {
  let sm = IssueStateMachine {}
  assert !sm.can_transition(Done, InProgress)
  assert !sm.can_transition(Done, Todo)
  assert !sm.can_transition(Done, Cancelled)
}

test "issue cannot transition from cancelled" {
  let sm = IssueStateMachine {}
  assert !sm.can_transition(Cancelled, Todo)
  assert !sm.can_transition(Cancelled, InProgress)
}

test "transition_issue returns allowed result" {
  let sm = IssueStateMachine {}
  let request = {
    from_status: Todo,
    to_status: InProgress,
    assignee_agent_id: Some("uuid-123")
  }
  let result = transition_issue(sm, request)
  assert result.allowed
  assert result.next_status == Some(InProgress)
}

test "transition_issue returns rejected result" {
  let sm = IssueStateMachine {}
  let request = {
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
Expected: 所有 Issue 状态机和验证测试通过

---

### Task 5: Company/Goal/Project 领域模型

**Files:**
- Create: `moonbit-core/src/domain/company/types.mbt`
- Create: `moonbit-core/src/domain/company/validation.mbt`
- Create: `moonbit-core/src/domain/goal/types.mbt`
- Create: `moonbit-core/src/domain/goal/validation.mbt`
- Create: `moonbit-core/src/domain/project/types.mbt`
- Create: `moonbit-core/src/domain/project/validation.mbt`

- [ ] **Step 1: 定义 Company 类型**

```moonbit
// moonbit-core/src/domain/company/types.mbt

use ../common/enums.{CompanyStatus}

pub struct Company {
  id: String
  name: String
  status: CompanyStatus
  issue_prefix: String
  budget_monthly_cents: Int
  spent_monthly_cents: Int
  require_board_approval_for_new_agents: Bool
} deriving (Debug)

pub struct CompanyCreateInput {
  name: String
  issue_prefix: String
  budget_monthly_cents: Int
  require_board_approval_for_new_agents: Bool
} deriving (Debug)
```

- [ ] **Step 2: 实现 Company 验证器**

```moonbit
// moonbit-core/src/domain/company/validation.mbt

use ./types.{CompanyCreateInput, ValidationError}

pub fn validate_company_create(input: CompanyCreateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors: List[ValidationError] = Nil
  
  if input.name.length() == 0 {
    errors = Cons({ field: "name", message: "Company name is required" }, errors)
  }
  if input.name.length() > 100 {
    errors = Cons({ field: "name", message: "Company name must be less than 100 characters" }, errors)
  }
  
  if input.issue_prefix.length() == 0 {
    errors = Cons({ field: "issuePrefix", message: "Issue prefix is required" }, errors)
  }
  if input.issue_prefix.length() > 10 {
    errors = Cons({ field: "issuePrefix", message: "Issue prefix must be less than 10 characters" }, errors)
  }
  
  if input.budget_monthly_cents < 0 {
    errors = Cons({ field: "budgetMonthlyCents", message: "Budget cannot be negative" }, errors)
  }
  
  if errors == Nil {
    Ok(Unit)
  } else {
    Err(errors)
  }
}
```

- [ ] **Step 3: 定义 Goal 类型**

```moonbit
// moonbit-core/src/domain/goal/types.mbt

use ../common/enums.{GoalLevel, GoalStatus}

pub struct Goal {
  id: String
  company_id: String
  title: String
  description: Option[String]
  level: GoalLevel
  parent_id: Option[String]
  owner_agent_id: Option[String]
  status: GoalStatus
} deriving (Debug)

pub struct GoalCreateInput {
  title: String
  description: Option[String]
  level: GoalLevel
  parent_id: Option[String]
  owner_agent_id: Option[String]
} deriving (Debug)
```

- [ ] **Step 4: 实现 Goal 验证器**

```moonbit
// moonbit-core/src/domain/goal/validation.mbt

use ./types.{GoalCreateInput, ValidationError}
use ../common/enums.GoalLevel

pub fn validate_goal_create(input: GoalCreateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors: List[ValidationError] = Nil
  
  if input.title.length() == 0 {
    errors = Cons({ field: "title", message: "Goal title is required" }, errors)
  }
  if input.title.length() > 200 {
    errors = Cons({ field: "title", message: "Goal title must be less than 200 characters" }, errors)
  }
  
  // Company 级别目标不能有父目标
  match (input.level, input.parent_id) {
    (Company, Some(_)) => {
      errors = Cons({ field: "parentId", message: "Company-level goals cannot have a parent" }, errors)
    }
    _ => {}
  }
  
  if errors == Nil {
    Ok(Unit)
  } else {
    Err(errors)
  }
}
```

- [ ] **Step 5: 定义 Project 类型**

```moonbit
// moonbit-core/src/domain/project/types.mbt

use ../common/enums.ProjectStatus

pub struct Project {
  id: String
  company_id: String
  name: String
  description: Option[String]
  status: ProjectStatus
  lead_agent_id: Option[String]
  color: String
} deriving (Debug)

pub struct ProjectCreateInput {
  name: String
  description: Option[String]
  lead_agent_id: Option[String]
  color: String
} deriving (Debug)
```

- [ ] **Step 6: 实现 Project 验证器**

```moonbit
// moonbit-core/src/domain/project/validation.mbt

use ./types.{ProjectCreateInput, ValidationError}

pub fn validate_project_create(input: ProjectCreateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors: List[ValidationError] = Nil
  
  if input.name.length() == 0 {
    errors = Cons({ field: "name", message: "Project name is required" }, errors)
  }
  if input.name.length() > 200 {
    errors = Cons({ field: "name", message: "Project name must be less than 200 characters" }, errors)
  }
  
  if errors == Nil {
    Ok(Unit)
  } else {
    Err(errors)
  }
}
```

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
} deriving (Eq, Debug, String)

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
  start: String // ISO 8601
  end: String   // ISO 8601
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

- [ ] **Step 3: 实现月度窗口计算**

```moonbit
// moonbit-core/src/budget/window.mbt

use ./types.BudgetWindow

// UTC 日历月窗口
// 开始: 当月 1 日 00:00:00 UTC
// 结束: 下月 1 日 00:00:00 UTC
pub fn current_utc_month_window(now_iso: String) -> BudgetWindow {
  // 简化实现: 解析 ISO 字符串,计算当月 1 日和下月 1 日
  // 实际实现需使用 MoonBit 时间库
  // 这里返回占位符,实际需解析 now_iso
  {
    start: "2026-04-01T00:00:00Z",
    end: "2026-05-01T00:00:00Z"
  }
}
```

- [ ] **Step 4: 编写预算测试**

```moonbit
// moonbit-core/src/budget/enforcement_test.mbt

use ./enforcement.check_budget_enforcement
use ./types.{BudgetEnforcementInput, EnforcementLevel}

test "budget not configured returns blocked" {
  let input = {
    company_id: "uuid-1",
    agent_id: "uuid-2",
    current_spent_cents: 1000000,
    budget_monthly_cents: 0
  }
  let result = check_budget_enforcement(input)
  assert result.blocked
  assert result.warning_level == Some(Hard)
}

test "budget exhausted returns blocked" {
  let input = {
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

test "approaching budget limit returns soft warning" {
  let input = {
    company_id: "uuid-1",
    agent_id: "uuid-2",
    current_spent_cents: 4500000,
    budget_monthly_cents: 5000000
  }
  let result = check_budget_enforcement(input)
  assert !result.blocked
  assert result.warning_level == Some(Soft)
  assert result.threshold >= 0.8
  assert result.threshold < 1.0
}

test "within budget returns no warning" {
  let input = {
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

test "over budget by 10%" {
  let input = {
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
  occurred_at: String // ISO 8601
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

- [ ] **Step 2: 实现成本聚合**

```moonbit
// moonbit-core/src/cost/aggregation.mbt

use ./types.{CostEvent, CostSummary, CostAggregationRequest}

pub fn aggregate_costs(events: List[CostEvent]) -> CostSummary {
  let mut total_cost = 0
  let mut total_input = 0
  let mut total_output = 0
  let mut count = 0
  let mut by_provider: Map[String, Int] = Map::new()
  let mut by_model: Map[String, Int] = Map::new()
  
  fn process_event(
    event: CostEvent,
    acc: (Int, Int, Int, Int, Map[String, Int], Map[String, Int])
  ) -> (Int, Int, Int, Int, Map[String, Int], Map[String, Int]) {
    let (cost, input, output, cnt, providers, models) = acc
    
    // 更新总计
    let new_cost = cost + event.cost_cents
    let new_input = input + event.input_tokens
    let new_output = output + event.output_tokens
    let new_count = cnt + 1
    
    // 更新 provider 统计
    let provider_total = match providers.get(event.provider) {
      Some(v) => v + event.cost_cents
      None => event.cost_cents
    }
    let new_providers = providers.set(event.provider, provider_total)
    
    // 更新 model 统计
    let model_total = match models.get(event.model) {
      Some(v) => v + event.cost_cents
      None => event.cost_cents
    }
    let new_models = models.set(event.model, model_total)
    
    (new_cost, new_input, new_output, new_count, new_providers, new_models)
  }
  
  let (final_cost, final_input, final_output, final_count, final_providers, final_models) = 
    events.fold(process_event, (0, 0, 0, 0, Map::new(), Map::new()))
  
  {
    total_cost_cents: final_cost,
    total_input_tokens: final_input,
    total_output_tokens: final_output,
    event_count: final_count,
    by_provider: final_providers,
    by_model: final_models
  }
}
```

- [ ] **Step 3: 验证构建**

Run: `cd moonbit-core && moon build`
Expected: 构建成功

---

### Task 8: MoonBit HTTP 服务端

**Files:**
- Create: `moonbit-core/src/http/router.mbt`
- Create: `moonbit-core/src/http/handlers.mbt`
- Create: `moonbit-core/src/http/server.mbt`
- Create: `moonbit-core/bin/server.mbt`

> **注意:** MoonBit 0.1 可能没有内置 HTTP 服务器。此任务需要根据 MoonBit HTTP 库的实际可用性调整实现策略。如果 HTTP 库不可用,则降级为:
> 1. 使用 `stdio` 通信 (JSON-RPC over stdin/stdout)
> 2. 或使用 Node.js FFI 包装 HTTP 层

- [ ] **Step 1: 调研 MoonBit HTTP 生态**

Run: 检查 MoonBit package registry 或标准库是否有 HTTP 服务器支持
Decision point: 
- 如果有 HTTP 库 → 继续 Step 2
- 如果没有 → 使用 stdio JSON-RPC 方案,更新设计文档

- [ ] **Step 2: 定义路由 (假设有 HTTP 库)**

```moonbit
// moonbit-core/src/http/router.mbt

pub enum HttpMethod {
  GET
  POST
  PUT
  DELETE
} deriving (Eq, Debug)

pub struct Route {
  method: HttpMethod
  path: String
  handler: Fn(Request) -> Response
}

pub struct Router {
  routes: List[Route]
}

impl Router {
  pub fn new() -> Router {
    { routes: Nil }
  }
  
  pub fn get(mut self: Self, path: String, handler: Fn(Request) -> Response) -> Router {
    let route = { method: GET, path, handler }
    { routes: Cons(route, self.routes) }
  }
  
  pub fn post(mut self: Self, path: String, handler: Fn(Request) -> Response) -> Router {
    let route = { method: POST, path, handler }
    { routes: Cons(route, self.routes) }
  }
  
  pub fn handle_request(self: Self, method: HttpMethod, path: String, body: String) -> Response {
    // 简单路由匹配
    fn find_route(routes: List[Route], m: HttpMethod, p: String) -> Option[Fn(Request) -> Response] {
      match routes {
        Nil => None
        Cons(route, rest) => {
          if route.method == m && route.path == p {
            Some(route.handler)
          } else {
            find_route(rest, m, p)
          }
        }
      }
    }
    
    match find_route(self.routes, method, path) {
      Some(handler) => handler({ method, path, body })
      None => { status: 404, headers: Map::new(), body: "{\"error\":\"Not Found\"}" }
    }
  }
}

pub struct Request {
  method: HttpMethod
  path: String
  body: String
}

pub struct Response {
  status: Int
  headers: Map[String, String]
  body: String
}
```

- [ ] **Step 3: 实现请求处理器**

```moonbit
// moonbit-core/src/http/handlers.mbt

use ../domain/agent/state_machine.AgentStateMachine
use ../domain/agent/validation.validate_agent_create
use ../domain/issue/state_machine.{IssueStateMachine, transition_issue}
use ../budget/enforcement.check_budget_enforcement
use ../cost/aggregation.aggregate_costs
use ./router.{Request, Response}
use ../lib/http.json_response

pub fn health_handler(req: Request) -> Response {
  json_response(200, "{\"status\":\"ok\"}")
}

pub fn validate_agent_handler(req: Request) -> Response {
  // 解析 JSON 请求体
  // 调用 validate_agent_create
  // 返回验证结果
  // 实际实现需解析 req.body 为 AgentCreateInput
  let result = validate_agent_create(/* parsed_input */)
  
  match result {
    Ok(_) => json_response(200, "{\"valid\":true,\"errors\":[]}")
    Err(errors) => {
      // 序列化错误列表
      json_response(422, "{\"valid\":false,\"errors\":[...]}")
    }
  }
}

pub fn transition_issue_handler(req: Request) -> Response {
  let sm = IssueStateMachine {}
  // 解析 transition request
  // 调用 transition_issue
  // 返回结果
  json_response(200, "{\"allowed\":true,\"nextStatus\":\"in_progress\"}")
}

pub fn budget_enforce_handler(req: Request) -> Response {
  // 解析 budget enforcement input
  // 调用 check_budget_enforcement
  // 返回结果
  json_response(200, "{\"blocked\":false,\"threshold\":0.9,\"warningLevel\":\"soft\"}")
}
```

- [ ] **Step 4: 实现 HTTP 服务器入口**

```moonbit
// moonbit-core/bin/server.mbt

use ../src/http/router.{Router, HttpMethod}
use ../src/http.handlers.{health_handler, validate_agent_handler, transition_issue_handler, budget_enforce_handler}

fn main {
  let router = Router::new()
    .get("/api/moonbit/health", health_handler)
    .post("/api/moonbit/agents/validate", validate_agent_handler)
    .post("/api/moonbit/issues/transition", transition_issue_handler)
    .post("/api/moonbit/budget/enforce", budget_enforce_handler)
  
  // 启动 HTTP 服务器
  // 实际实现依赖 MoonBit HTTP 库
  // println("MoonBit Domain Core listening on port 3200")
  // router.listen(3200)
  
  // 临时实现: 打印就绪信号
  println("READY")
  
  // 保持进程运行
  loop {
    // 等待请求
  }
}
```

- [ ] **Step 5: 验证构建**

Run: `cd moonbit-core && moon build`
Expected: 构建成功,打印 "READY"

---

### Task 9: TypeScript HTTP 客户端

**Files:**
- Create: `server/src/services/moonbit-client.ts`
- Test: `tests/moonbit-integration/agent-validation.test.ts`

- [ ] **Step 1: 创建 TypeScript HTTP 客户端**

```typescript
// server/src/services/moonbit-client.ts

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

export interface MoonBitClientConfig {
  baseUrl: string;
  timeoutMs?: number;
}

export class MoonBitClient {
  private baseUrl: string;
  private timeoutMs: number;
  
  constructor(config: MoonBitClientConfig) {
    this.baseUrl = config.baseUrl.replace(/\/$/, '');
    this.timeoutMs = config.timeoutMs ?? 1000;
  }
  
  async health(): Promise<boolean> {
    try {
      const res = await fetch(`${this.baseUrl}/api/moonbit/health`, {
        signal: AbortSignal.timeout(this.timeoutMs)
      });
      return res.ok;
    } catch {
      return false;
    }
  }
  
  async validateAgent(data: Record<string, unknown>): Promise<ValidationResult> {
    const res = await fetch(`${this.baseUrl}/api/moonbit/agents/validate`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
      signal: AbortSignal.timeout(this.timeoutMs)
    });
    
    if (!res.ok) {
      throw new Error(`MoonBit service error: ${res.status}`);
    }
    
    return res.json();
  }
  
  async transitionIssue(request: TransitionRequest): Promise<TransitionResult> {
    const res = await fetch(`${this.baseUrl}/api/moonbit/issues/transition`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(request),
      signal: AbortSignal.timeout(this.timeoutMs)
    });
    
    if (!res.ok) {
      throw new Error(`MoonBit service error: ${res.status}`);
    }
    
    return res.json();
  }
  
  async checkBudget(input: BudgetEnforcementInput): Promise<BudgetEnforcementResult> {
    const res = await fetch(`${this.baseUrl}/api/moonbit/budget/enforce`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(input),
      signal: AbortSignal.timeout(this.timeoutMs)
    });
    
    if (!res.ok) {
      throw new Error(`MoonBit service error: ${res.status}`);
    }
    
    return res.json();
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
  
  beforeAll(async () => {
    // 从环境变量或配置读取 MoonBit 服务地址
    const baseUrl = process.env.MOONBIT_SERVICE_URL || 'http://localhost:3200';
    client = new MoonBitClient({ baseUrl, timeoutMs: 2000 });
  });
  
  it('should pass health check', async () => {
    const healthy = await client.health();
    expect(healthy).toBe(true);
  });
  
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
  
  it('should reject an agent with empty name', async () => {
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
  
  it('should reject an agent with negative budget', async () => {
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
Expected: 测试通过 (需要 MoonBit 服务运行)

---

### Task 10: 进程管理器

**Files:**
- Create: `server/src/services/moonbit-process-manager.ts`
- Modify: `server/src/routes/index.ts` (启动 MoonBit 子进程)
- Modify: `package.json` (新增 scripts)

- [ ] **Step 1: 创建进程管理器**

```typescript
// server/src/services/moonbit-process-manager.ts

import { spawn, ChildProcess } from 'child_process';
import { MoonBitClient } from './moonbit-client';
import detectPort from 'detect-port';

export interface MoonBitConfig {
  enabled: boolean;
  binaryPath: string;
  port?: number;
  timeoutMs?: number;
  healthCheckIntervalMs?: number;
  maxRestarts?: number;
  restartWindowMs?: number;
}

const DEFAULT_CONFIG: Required<MoonBitConfig> = {
  enabled: true,
  binaryPath: 'moonbit-core/bin/server',
  port: 0,
  timeoutMs: 1000,
  healthCheckIntervalMs: 10000,
  maxRestarts: 3,
  restartWindowMs: 60000
};

export class MoonBitProcessManager {
  private child: ChildProcess | null = null;
  private port: number = 0;
  private restartCount: number = 0;
  private lastRestartTime: number = 0;
  private healthCheckInterval: NodeJS.Timeout | null = null;
  private readyPromise: Promise<void>;
  private resolveReady: () => void;
  private rejectReady: (err: Error) => void;
  
  private config: Required<MoonBitConfig>;
  private client: MoonBitClient | null = null;
  
  constructor(config: Partial<MoonBitConfig> = {}) {
    this.config = { ...DEFAULT_CONFIG, ...config };
    this.readyPromise = new Promise((resolve, reject) => {
      this.resolveReady = resolve;
      this.rejectReady = reject;
    });
  }
  
  getClient(): MoonBitClient | null {
    return this.client;
  }
  
  async start(): Promise<void> {
    if (!this.config.enabled) {
      console.log('[MoonBit] Disabled in config');
      return;
    }
    
    // 选择空闲端口
    this.port = this.config.port === 0 
      ? await detectPort(3200)
      : this.config.port;
    
    console.log(`[MoonBit] Starting on port ${this.port}`);
    
    // 启动子进程
    this.child = spawn(this.config.binaryPath, [
      '--port', String(this.port)
    ], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: { ...process.env, PAPERCLIP_MOONBIT_PORT: String(this.port) },
      shell: process.platform === 'win32'
    });
    
    // 监听 stdout 中的 "READY" 信号
    this.child.stdout?.on('data', (data: Buffer) => {
      const output = data.toString();
      console.log(`[MoonBit] ${output}`);
      
      if (output.includes('READY')) {
        this.client = new MoonBitClient({
          baseUrl: `http://localhost:${this.port}`,
          timeoutMs: this.config.timeoutMs
        });
        this.resolveReady();
        this.startHealthCheck();
      }
    });
    
    // 监听 stderr
    this.child.stderr?.on('data', (data: Buffer) => {
      console.error(`[MoonBit] ${data.toString()}`);
    });
    
    // 监听退出
    this.child.on('exit', (code: number | null, signal: string | null) => {
      console.log(`[MoonBit] Exited with code ${code}, signal ${signal}`);
      this.handleCrash();
    });
    
    // 等待就绪 (30s 超时)
    const timeout = setTimeout(() => {
      this.rejectReady(new Error('MoonBit service failed to start within 30s'));
    }, 30000);
    
    try {
      await this.readyPromise;
      clearTimeout(timeout);
      console.log(`[MoonBit] Service ready on port ${this.port}`);
    } catch (err) {
      this.stop();
      throw err;
    }
  }
  
  private startHealthCheck(): void {
    this.healthCheckInterval = setInterval(async () => {
      try {
        const healthy = await this.client?.health();
        if (!healthy) {
          await this.handleCrash();
        }
      } catch (err) {
        console.error('[MoonBit] Health check failed:', err);
      }
    }, this.config.healthCheckIntervalMs);
  }
  
  private async handleCrash(): Promise<void> {
    const now = Date.now();
    
    // 检查重启窗口期
    if (now - this.lastRestartTime > this.config.restartWindowMs) {
      this.restartCount = 0;
    }
    
    // 检查重启次数
    if (this.restartCount >= this.config.maxRestarts) {
      console.error('[MoonBit] Max restart attempts reached, giving up');
      this.stop();
      return;
    }
    
    this.lastRestartTime = now;
    this.restartCount++;
    
    console.log(`[MoonBit] Restarting (${this.restartCount}/${this.config.maxRestarts})`);
    
    // 重置 ready promise
    this.readyPromise = new Promise((resolve, reject) => {
      this.resolveReady = resolve;
      this.rejectReady = reject;
    });
    
    await this.start();
  }
  
  async stop(): Promise<void> {
    if (this.healthCheckInterval) {
      clearInterval(this.healthCheckInterval);
      this.healthCheckInterval = null;
    }
    
    if (this.child) {
      console.log('[MoonBit] Sending SIGTERM');
      this.child.kill('SIGTERM');
      
      // 等待 5s 后强制杀死
      await new Promise(resolve => setTimeout(resolve, 5000));
      
      if (this.child.exitCode === null) {
        console.log('[MoonBit] Force killing');
        this.child.kill('SIGKILL');
      }
      
      this.child = null;
    }
    
    this.client = null;
  }
  
  async waitForReady(): Promise<void> {
    return this.readyPromise;
  }
}
```

- [ ] **Step 2: 修改路由启动逻辑**

```typescript
// server/src/routes/index.ts (在 Express 启动前新增)

import { MoonBitProcessManager } from '../services/moonbit-process-manager';

let moonbitManager: MoonBitProcessManager | null = null;

export async function startServer(app: Express, port: number) {
  // 启动 MoonBit 子进程
  moonbitManager = new MoonBitProcessManager({
    enabled: process.env.MOONBIT_ENABLED !== 'false',
    port: process.env.MOONBIT_PORT ? parseInt(process.env.MOONBIT_PORT) : 0
  });
  
  try {
    await moonbitManager.start();
    console.log('[Server] MoonBit domain core started');
  } catch (err) {
    console.warn('[Server] MoonBit failed to start, continuing without it:', err);
  }
  
  // 启动 Express
  app.listen(port, () => {
    console.log(`[Server] Express listening on port ${port}`);
  });
}

// 优雅关闭
process.on('SIGTERM', async () => {
  console.log('[Server] SIGTERM received, shutting down gracefully');
  if (moonbitManager) {
    await moonbitManager.stop();
  }
  process.exit(0);
});
```

- [ ] **Step 3: 验证进程管理**

Run: `pnpm dev`
Expected: 
- MoonBit 子进程启动
- 打印 "READY" 信号
- 健康检查正常运行
- 手动 kill MoonBit 进程后自动重启 (3 次内)

---

### Task 11: 集成到 Express 服务层

**Files:**
- Modify: `server/src/services/agents.ts` (集成 MoonBit 验证)
- Modify: `server/src/services/issues.ts` (集成 MoonBit 状态机)

- [ ] **Step 1: 修改 Agent 服务集成 MoonBit 验证**

```typescript
// server/src/services/agents.ts (修改 create 方法)

import { MoonBitClient } from './moonbit-client';

// 在 agentService 工厂函数中注入 MoonBit 客户端
export function agentService(db: Db, moonbitClient?: MoonBitClient) {
  return {
    // ... 其他方法
    
    async create(companyId: string, data: AgentCreateInput) {
      // 如果 MoonBit 可用,先使用 MoonBit 验证
      if (moonbitClient) {
        try {
          const validation = await moonbitClient.validateAgent({
            name: data.name,
            role: data.role,
            title: data.title,
            reportsTo: data.reportsTo,
            adapterType: data.adapterType,
            budgetMonthlyCents: data.budgetMonthlyCents
          });
          
          if (!validation.valid) {
            throw new Error(`Validation failed: ${JSON.stringify(validation.errors)}`);
          }
        } catch (err) {
          // MoonBit 调用失败,降级到 TypeScript 验证
          console.warn('[AgentService] MoonBit validation failed, falling back to TS:', err);
        }
      }
      
      // 原有的 TypeScript 创建逻辑
      // ...
    }
  };
}
```

- [ ] **Step 2: 修改 Issue 服务集成 MoonBit 状态机**

```typescript
// server/src/services/issues.ts (修改状态转换方法)

import { MoonBitClient } from './moonbit-client';

// 在 issueService 工厂函数中注入 MoonBit 客户端
export function issueService(db: Db, moonbitClient?: MoonBitClient) {
  return {
    // ... 其他方法
    
    async transitionIssue(issueId: string, transition: { toStatus: IssueStatus }) {
      const issue = await this.getById(issueId);
      if (!issue) {
        throw new Error('Issue not found');
      }
      
      // 如果 MoonBit 可用,使用 MoonBit 状态机验证
      if (moonbitClient) {
        try {
          const result = await moonbitClient.transitionIssue({
            fromStatus: issue.status,
            toStatus: transition.toStatus,
            assigneeAgentId: issue.assigneeAgentId
          });
          
          if (!result.allowed) {
            throw new ConflictError(result.reason || 'Invalid state transition');
          }
        } catch (err) {
          // MoonBit 调用失败,降级到 TypeScript 状态机
          console.warn('[IssueService] MoonBit transition failed, falling back to TS:', err);
          assertTransition(issue.status, transition.toStatus);
        }
      } else {
        // 原有 TypeScript 状态机
        assertTransition(issue.status, transition.toStatus);
      }
      
      // 执行转换
      // ...
    }
  };
}
```

- [ ] **Step 3: 验证集成**

Run: `pnpm dev`
Expected: 
- Agent 创建时调用 MoonBit 验证
- Issue 转换时调用 MoonBit 状态机
- MoonBit 不可用时自动降级到 TypeScript 实现

---

### Task 12: 全量验证与文档

**Files:**
- Create: `docs/moonbit-migration.md`
- Modify: `AGENTS.md` (新增 MoonBit 开发流程)

- [ ] **Step 1: 运行全量测试**

Run: `pnpm -r typecheck && pnpm test:run && pnpm build`
Expected: 
- TypeScript 类型检查通过
- 所有现有测试通过
- 构建成功

- [ ] **Step 2: 运行 MoonBit 测试**

Run: `cd moonbit-core && moon test`
Expected: 所有 MoonBit 测试通过

- [ ] **Step 3: 创建迁移文档**

```markdown
# docs/moonbit-migration.md

# MoonBit 迁移指南

## 概述

本文档描述将 Paperclip 后端核心领域模型从 TypeScript 迁移到 MoonBit 的过程。

## 架构

- MoonBit 作为独立进程运行
- 通过 HTTP/JSON REST 与 Express 通信
- 配置来源: `~/.paperclip/config.json`

## 开发流程

### 启动开发环境

```bash
# 一键启动 (Express + MoonBit)
pnpm dev
```

### 单独启动 MoonBit

```bash
pnpm dev:moonbit
```

### 构建 MoonBit

```bash
pnpm build:moonbit
```

### 运行 MoonBit 测试

```bash
pnpm test:moonbit
```

## 迁移状态

| 领域 | 状态 | 备注 |
|------|------|------|
| 常量/枚举 | ✅ 完成 | 所有核心枚举已迁移 |
| Agent 状态机 | ✅ 完成 | 100% 测试覆盖 |
| Issue 状态机 | ✅ 完成 | 100% 测试覆盖 |
| 预算计算 | ✅ 完成 | 与 TS 行为一致 |
| Company/Goal/Project | ✅ 完成 | 基础验证器 |
| HTTP 服务端 | ⚠️ 部分 | 依赖 MoonBit HTTP 库 |

## 故障排除

### MoonBit 无法启动

1. 检查 MoonBit 版本: `moon version`
2. 检查二进制路径: `moonbit-core/bin/server`
3. 查看日志: `~/.paperclip/logs/moonbit.log`

### 类型不一致

MoonBit 与 TypeScript 类型通过 JSON Schema 对齐。如果发现问题,检查:
- `moonbit-core/src/domain/*/types.mbt`
- `packages/shared/src/types/*.ts`
```

- [ ] **Step 4: 修改 AGENTS.md 新增 MoonBit 开发规范**

在 `AGENTS.md` 中新增章节:

```markdown
## 12. MoonBit 开发规范

MoonBit 用于核心领域模型和计算密集型逻辑。

### 12.1 代码组织

- 领域模型: `moonbit-core/src/domain/`
- 计算逻辑: `moonbit-core/src/budget/`, `moonbit-core/src/cost/`
- HTTP 层: `moonbit-core/src/http/`

### 12.2 命名约定

| 类型 | 约定 | 示例 |
|------|------|------|
| 枚举 | PascalCase | `AgentStatus`, `IssuePriority` |
| 结构体 | PascalCase | `Agent`, `IssueCreateInput` |
| 函数 | snake_case | `validate_agent_create`, `check_budget_enforcement` |
| 字段 | snake_case | `company_id`, `budget_monthly_cents` |

### 12.3 测试要求

- 每个状态机必须有完整的转换测试
- 每个验证器必须有正/反测试用例
- 集成测试必须验证 TypeScript ↔ MoonBit 行为一致性

### 12.4 构建与验证

```bash
# 构建 MoonBit
cd moonbit-core && moon build

# 运行测试
cd moonbit-core && moon test

# 全量验证
pnpm -r typecheck && pnpm test:run && pnpm build
```
```

- [ ] **Step 5: 最终验证**

Run: `pnpm dev`
Expected: 
- Express + MoonBit 同时启动
- 访问 `/api/health` 返回正常
- MoonBit 端点可访问

---

## Risks and Mitigations

| 风险 | 影响 | 概率 | 缓解策略 |
|------|------|------|---------|
| MoonBit HTTP 库不可用 | 无法实现 HTTP 服务端 | 中 | 降级为 stdio JSON-RPC 通信 |
| 类型同步困难 | TS/MoonBit 类型不一致 | 中 | 建立 JSON Schema 中间表示 |
| 进程管理复杂 | 子进程崩溃/重启逻辑难维护 | 低 | 参考 PM2 模式,充分测试 |
| 迁移周期过长 | 项目维护成本高 | 中 | 分阶段验证,每阶段可独立发布 |
| Windows 兼容性 | spawn 行为差异 | 低 | 使用 `shell: true` 选项 |

---

## Verification Steps

### 单元测试验证
```bash
# MoonBit 测试
cd moonbit-core && moon test

# TypeScript 测试
pnpm test:run
```

### 集成测试验证
```bash
# 启动 MoonBit 服务
pnpm dev:moonbit

# 运行集成测试
pnpm test:run tests/moonbit-integration/
```

### E2E 验证
```bash
# 完整流程测试
pnpm dev
curl http://localhost:3100/api/health
curl http://localhost:3100/api/companies
```

### 进程管理验证
```bash
# 手动 kill MoonBit 进程
kill -9 <moonbit_pid>

# 验证自动重启 (3 次内)
# 查看日志确认重启
```

---

## Available-Agent-Types Roster

| Agent Type | Recommended For | Reasoning Level | Why |
|------------|----------------|-----------------|-----|
| general-purpose | MoonBit 代码编写、领域模型迁移 | high | 需要理解 MoonBit 语法和类型系统 |
| Explore | 代码库调研、现有模式查找 | low | 简单搜索任务 |
| general-purpose | TypeScript HTTP 客户端开发 | medium | 标准 TypeScript 任务 |
| general-purpose | 进程管理器开发 | medium | Node.js child_process 标准用法 |

### Follow-up Staffing Guidance

**Lane 1: MoonBit 核心 (Tasks 1-8)**
- 1x general-purpose agent (high reasoning)
- 负责: 模块搭建、领域模型、状态机、验证器、HTTP 服务端

**Lane 2: TypeScript 集成 (Tasks 9-11)**
- 1x general-purpose agent (medium reasoning)
- 负责: HTTP 客户端、进程管理器、Express 集成

**Lane 3: 测试与文档 (Task 12)**
- 1x general-purpose agent (medium reasoning)
- 负责: 全量测试、文档编写

### Launch Hints

```bash
# Ralph 执行
$ralph .omx/plans/moonbit-backend-migration.md

# Team 并行执行
$team .omx/plans/moonbit-backend-migration.md --lanes 3

# Team 验证路径
# 1. Team 完成所有 Tasks 后运行:
pnpm -r typecheck && pnpm test:run && cd moonbit-core && moon test
# 2. Ralph 验证端到端流程:
pnpm dev
curl http://localhost:3100/api/health
```

---

*Plan created: 2026-04-08*
*Based on design: docs/superpowers/specs/2026-04-08-moonbit-backend-migration-design.md*
