# MoonBit 后端迁移设计文档

> **创建日期**: 2026-04-08
> **状态**: 待实施
> **作者**: AI Assistant (用户确认)
> **Commit**: 9cfa37fce3c25988917b0631147413d171bf4e32

---

## 目录

- [1. 概述](#1-概述)
- [2. 架构设计](#2-架构设计)
- [3. 目录结构](#3-目录结构)
- [4. HTTP 通信协议](#4-http-通信协议)
- [5. MoonBit 领域模型实现](#5-moonbit-领域模型实现)
- [6. 进程管理与部署](#6-进程管理与部署)
- [7. 迁移路线图](#7-迁移路线图)
- [8. 风险与缓解](#8-风险与缓解)
- [9. 验证策略](#9-验证策略)

---

## 1. 概述

### 1.1 目标

将 Paperclip 后端核心领域模型、预算计算引擎、共享验证器从 TypeScript 迁移到 MoonBit，减少 pnpm 包依赖，提升类型安全性和计算性能。

### 1.2 迁移策略

**渐进式迁移** - 保持现有 TypeScript 后端运行，用 MoonBit 重写核心领域模型和服务层，通过 HTTP/JSON REST 与现有系统集成。

**迁移顺序**: A → B → C → 其他服务
- **A**: 核心领域模型 + 状态机 (Company, Agent, Issue, Goal, Project)
- **B**: 预算计算引擎 + 成本聚合
- **C**: 共享类型 + 验证器
- **其他**: 插件系统、心跳引擎 (评估后决定)

### 1.3 通信机制

**独立进程 + HTTP/JSON REST**
- MoonBit 编译为独立二进制，作为子进程运行
- 通过 HTTP/JSON REST 与 Express 通信
- 配置来源统一: `~/.paperclip/config.json`

### 1.4 预期收益

| 收益 | 说明 |
|------|------|
| 减少依赖 | 约 20-30% pnpm 包减少 (移除 shared 验证器/类型包) |
| 类型安全 | MoonBit 强类型系统替代 TypeScript + Zod |
| 计算性能 | 预算计算、成本聚合等纯函数逻辑性能提升 |
| 可维护性 | 领域模型集中，业务逻辑与 IO 分离 |

---

## 2. 架构设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                     UI Layer (React)                        │
│                   保持不变                                   │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│              API Layer (Express/TypeScript)                  │
│  保留: 路由、认证、文件IO、适配器调用、插件系统               │
│  移除: 纯计算逻辑、状态机、领域验证                           │
└───────────────┬─────────────────────────┬───────────────────┘
                │                         │
                │ HTTP/JSON REST          │ 直接调用
                ▼                         ▼
┌───────────────────────┐   ┌─────────────────────────────┐
│  MoonBit Domain Core  │   │  TypeScript Service Layer   │
│  (独立进程)           │   │  (保留部分)                  │
│                       │   │                              │
│  - 领域模型           │   │  - 数据库访问                │
│  - 状态机             │   │  - HTTP 路由                 │
│  - 验证逻辑           │   │  - 适配器编排                │
│  - 预算计算          │   │  - 插件系统                  │
│  - 成本聚合          │   │  - 文件系统操作              │
└───────┬───────────────┘   └─────────────────────────────┘
        │
        │ Drizzle ORM (直接连接)
        ▼
┌─────────────────────────────────────────────────────────────┐
│                    PostgreSQL / PGlite                       │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| MoonBit 直连数据库 | 是 | 避免 TypeScript 层做数据聚合，减少网络往返 |
| Express 保留路由 | 是 | UI 契约不变，渐进迁移 |
| 通信协议 | HTTP/JSON | 简单、可调试、与现有风格一致 |
| 配置来源 | `~/.paperclip/config.json` | 统一配置管理 |
| 进程管理 | Express 启动/监控 MoonBit | 单一入口，生命周期绑定 |

---

## 3. 目录结构

### 3.1 新增目录结构

```
E:\moonbit\clawteam\
├── server/                    # 保持不变 (Express)
├── ui/                        # 保持不变 (React)
├── packages/
│   ├── db/                    # 保持不变 (Drizzle Schema)
│   ├── shared/                # 逐步迁移到 MoonBit
│   ├── adapters/              # 保持不变
│   └── adapter-utils/         # 保持不变
│
├── moonbit-core/              # 【新增】MoonBit 核心领域
│   ├── moon.mod.json          # MoonBit 模块定义
│   ├── moon.pkg.json          # 包配置
│   │
│   ├── src/
│   │   ├── domain/            # 领域模型
│   │   │   ├── company/       # Company 实体 + 验证
│   │   │   ├── agent/         # Agent 实体 + 状态机
│   │   │   ├── issue/         # Issue 实体 + 状态机
│   │   │   ├── goal/          # Goal 实体 + 层级
│   │   │   └── project/       # Project 实体
│   │   │
│   │   ├── budget/            # 预算计算引擎
│   │   │   ├── policy.mbt     # 预算策略评估
│   │   │   ├── enforcement.mbt # 预算执行门控
│   │   │   └── window.mbt     # 月度窗口计算
│   │   │
│   │   ├── cost/              # 成本聚合
│   │   │   ├── aggregation.mbt
│   │   │   └── metrics.mbt
│   │   │
│   │   ├── validation/        # 共享验证器 (替代 Zod)
│   │   │   ├── agent.mbt
│   │   │   ├── issue.mbt
│   │   │   └── company.mbt
│   │   │
│   │   └── constants/         # 枚举常量 (替代 shared/constants.ts)
│   │       ├── issue_status.mbt
│   │       ├── agent_role.mbt
│   │       └── budget.mbt
│   │
│   └── bin/
│       └── server.mbt         # HTTP 服务入口 (启动 API 服务器)
│
└── docs/
    └── moonbit-migration.md   # 迁移文档
```

### 3.2 MoonBit 模块定义

```json
// moonbit-core/moon.mod.json
{
  "name": "paperclip/moonbit-core",
  "version": "0.1.0",
  "readme": "README.md",
  "repository": "",
  "license": "MIT",
  "source": "src"
}
```

### 3.3 关键设计点

| 方面 | 说明 |
|------|------|
| **包管理** | 不使用 pnpm，独立 MoonBit 模块 |
| **数据库访问** | MoonBit 通过 HTTP 客户端或直接连接 (取决于生态成熟度) |
| **类型契约** | MoonBit 的类型定义与 TypeScript 共享契约通过 JSON Schema 对齐 |
| **构建产物** | `moonbit build` → 二进制，由 Express 进程管理 |

---

## 4. HTTP 通信协议

### 4.1 API 端点

```
Base Path: /api/moonbit

# 健康检查
GET  /api/moonbit/health

# 领域模型验证
POST /api/moonbit/agents/validate       → 验证 Agent 创建/更新数据
POST /api/moonbit/issues/validate       → 验证 Issue 数据
POST /api/moonbit/companies/validate    → 验证 Company 数据

# 状态机操作
POST /api/moonbit/issues/:id/transition  → 状态转换验证
POST /api/moonbit/agents/:id/transition  → Agent 状态转换

# 预算计算
POST /api/moonbit/budget/enforce        → 预算执行检查
GET  /api/moonbit/budget/window          → 当前月度窗口
POST /api/moonbit/cost/aggregate        → 成本聚合计算

# 领域查询 (读模型)
GET  /api/moonbit/companies/:id          → 获取 Company
GET  /api/moonbit/agents/:id             → 获取 Agent
GET  /api/moonbit/issues/:id             → 获取 Issue
```

### 4.2 请求/响应示例

**验证 Agent**:
```json
// POST /api/moonbit/agents/validate
// Request:
{
  "name": "Alice",
  "role": "engineer",
  "title": "Backend Developer",
  "reportsTo": "uuid-xxx",
  "adapterType": "claude_local",
  "budgetMonthlyCents": 5000000
}

// Response 200:
{
  "valid": true,
  "errors": []
}

// Response 422:
{
  "valid": false,
  "errors": [
    { "field": "role", "message": "Invalid role: must be one of [ceo, cto, engineer, ...]" }
  ]
}
```

**状态转换**:
```json
// POST /api/moonbit/issues/:id/transition
// Request:
{
  "fromStatus": "todo",
  "toStatus": "in_progress",
  "assigneeAgentId": "uuid-xxx"
}

// Response 200:
{
  "allowed": true,
  "nextStatus": "in_progress"
}

// Response 409:
{
  "allowed": false,
  "reason": "Cannot transition from 'done' to 'in_progress'"
}
```

**预算执行**:
```json
// POST /api/moonbit/budget/enforce
// Request:
{
  "companyId": "uuid-xxx",
  "agentId": "uuid-yyy",
  "currentSpentCents": 4500000,
  "budgetMonthlyCents": 5000000
}

// Response 200:
{
  "blocked": false,
  "threshold": 0.9,
  "warningLevel": "soft"
}
```

### 4.3 错误语义

| HTTP 状态码 | 含义 |
|------------|------|
| `200` | 成功 |
| `400` | 请求格式错误 |
| `409` | 状态冲突 (状态机拒绝) |
| `422` | 验证失败 |
| `500` | MoonBit 服务内部错误 |

### 4.4 超时与重试

| 操作类型 | 超时 | 重试策略 |
|---------|------|---------|
| 验证 | 100ms | 不重试 |
| 状态转换 | 200ms | 不重试 |
| 预算计算 | 500ms | 1 次重试 |
| 成本聚合 | 1000ms | 1 次重试 |

---

## 5. MoonBit 领域模型实现

### 5.1 核心实体定义 (示例: Agent)

```moonbit
// moonbit-core/src/domain/agent/agent.mbt

enum AgentStatus {
  Idle
  Running
  Paused
  Error
  Terminated
  PendingApproval
} deriving (Eq, Debug)

enum AgentRole {
  CEO
  CTO
  Engineer
  ProductManager
  Designer
  QAEngineer
} deriving (Eq, Debug)

struct Agent {
  id: String
  company_id: String
  name: String
  role: AgentRole
  title: String
  status: AgentStatus
  reports_to: Option[String]
  adapter_type: String
  budget_monthly_cents: Int
  spent_monthly_cents: Int
} deriving (Debug)

// 状态转换规则
fn can_transition(self : AgentStatus, to : AgentStatus) -> Bool {
  match (self, to) {
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
    (_, Terminated) => true
    _ => false
  }
}

// 验证器
fn validate_agent(data : AgentCreateInput) -> Result[Unit, List[ValidationError]] {
  let mut errors : List[ValidationError] = Nil
  
  if String::is_empty(data.name) {
    errors = Cons({ field: "name", message: "Agent name is required" }, errors)
  }
  
  if data.budget_monthly_cents < 0 {
    errors = Cons({ field: "budgetMonthlyCents", message: "Budget cannot be negative" }, errors)
  }
  
  if errors == Nil {
    Ok(Unit)
  } else {
    Err(errors)
  }
}
```

### 5.2 状态机统一模型

```moonbit
// moonbit-core/src/domain/common/state_machine.mbt

trait StateMachine {
  type State
  type Event
  
  fn can_transition(self : Self.State, event : Self.Event) -> Bool
  fn apply_transition(self : Self.State, event : Self.Event) -> Self.State
}

// Issue 状态机实现
impl StateMachine for IssueStateMachine {
  type State = IssueStatus
  type Event = IssueTransitionEvent
  
  fn can_transition(self : IssueStatus, event : IssueTransitionEvent) -> Bool {
    match (self, event.target_status) {
      (Backlog, Todo) => true
      (Backlog, Cancelled) => true
      (Todo, InProgress) => true
      (Todo, Blocked) => true
      (InProgress, InReview) => true
      (InProgress, Blocked) => true
      (InReview, Done) => true
      (InReview, InProgress) => true
      (Blocked, Todo) => true
      (Blocked, Cancelled) => true
      _ => false
    }
  }
}
```

### 5.3 预算计算引擎

```moonbit
// moonbit-core/src/budget/enforcement.mbt

enum EnforcementLevel {
  Soft
  Hard
} deriving (Eq, Debug)

struct BudgetEnforcementResult {
  blocked: Bool
  threshold: Float
  warning_level: Option[EnforcementLevel]
  message: Option[String]
}

fn check_budget_enforcement(
  spent_cents: Int,
  budget_cents: Int,
  action: String
) -> BudgetEnforcementResult {
  if budget_cents <= 0 {
    { blocked: true, threshold: 1.0, warning_level: Some(Hard), 
      message: Some("Budget not configured") }
  } else {
    let ratio = spent_cents.to_double() /. budget_cents.to_double()
    
    if ratio >= 1.0 {
      { blocked: true, threshold: ratio, warning_level: Some(Hard),
        message: Some("Budget exhausted") }
    } else if ratio >= 0.8 {
      { blocked: false, threshold: ratio, warning_level: Some(Soft),
        message: Some("Approaching budget limit") }
    } else {
      { blocked: false, threshold: ratio, warning_level: None,
        message: None }
    }
  }
}
```

### 5.4 HTTP 服务端实现

```moonbit
// moonbit-core/bin/server.mbt

fn main() {
  let port = get_config_port()
  
  let router = Router::new()
    .get("/api/moonbit/health", health_handler)
    .post("/api/moonbit/agents/validate", validate_agent_handler)
    .post("/api/moonbit/issues/validate", validate_issue_handler)
    .post("/api/moonbit/issues/:id/transition", transition_issue_handler)
    .post("/api/moonbit/budget/enforce", budget_enforce_handler)
    .post("/api/moonbit/cost/aggregate", cost_aggregate_handler)
  
  println("MoonBit Domain Core listening on port {port}")
  router.listen(port)
}
```

### 5.5 TypeScript 客户端 (Express 调用方)

```typescript
// server/src/services/moonbit-client.ts

interface MoonBitClient {
  validateAgent(data: AgentCreateInput): Promise<ValidationResult>;
  transitionIssue(issueId: string, transition: TransitionRequest): Promise<TransitionResult>;
  checkBudget(enforcement: BudgetEnforcementInput): Promise<BudgetEnforcementResult>;
}

class MoonBitHttpClient implements MoonBitClient {
  constructor(private baseUrl: string, private timeoutMs: number = 1000) {}
  
  async validateAgent(data: AgentCreateInput): Promise<ValidationResult> {
    const res = await fetch(`${this.baseUrl}/api/moonbit/agents/validate`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
      signal: AbortSignal.timeout(this.timeoutMs)
    });
    
    if (!res.ok) throw new Error(`MoonBit service error: ${res.status}`);
    return res.json();
  }
}
```

---

## 6. 进程管理与部署

### 6.1 进程管理架构

```
┌─────────────────────────────────────────────────┐
│                  Express (主进程)                │
│                                                 │
│  ┌───────────────────────────────────────────┐  │
│  │  MoonBitProcessManager                     │  │
│  │                                           │  │
│  │  - 启动: spawn('moonbit-core/bin/server') │  │
│  │  - 健康检查: 定时 GET /health             │  │
│  │  - 重启: 崩溃时自动重启 (最大 3 次)       │  │
│  │  - 关闭: SIGTERM → 等待 5s → SIGKILL      │  │
│  │  - 日志: 转发到 pino logger               │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
         │
         │ HTTP (localhost)
         ▼
┌─────────────────────────────────────────────────┐
│           MoonBit Core (子进程)                  │
│                                                 │
│  - 监听 localhost:动态端口                       │
│  - 启动时发送就绪信号                            │
│  - 接收到 SIGTERM 优雅关闭                       │
│  - stdout/stderr 管道到父进程                    │
└─────────────────────────────────────────────────┘
```

### 6.2 配置管理

**配置位置**: `~/.paperclip/config.json` 或项目本地 `.paperclip/config.json`

**新增配置节**:
```json
{
  "moonbit": {
    "enabled": true,
    "binaryPath": "moonbit-core/bin/server",
    "port": 0,
    "timeoutMs": 1000,
    "healthCheckIntervalMs": 10000,
    "maxRestarts": 3,
    "restartWindowMs": 60000
  }
}
```

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `enabled` | 是否启用 MoonBit 核心 | `true` |
| `binaryPath` | MoonBit 二进制路径 | `moonbit-core/bin/server` |
| `port` | HTTP 端口 (0=自动分配) | `0` |
| `timeoutMs` | 请求超时 | `1000` |
| `healthCheckIntervalMs` | 健康检查间隔 | `10000` |
| `maxRestarts` | 最大重启次数 | `3` |
| `restartWindowMs` | 重启窗口期 | `60000` |

### 6.3 启动流程

```typescript
// server/src/services/moonbit-process-manager.ts

class MoonBitProcessManager {
  private child: ChildProcess | null = null;
  private port: number = 0;
  private restartCount: number = 0;
  private lastRestartTime: number = 0;
  
  async start(config: MoonBitConfig): Promise<void> {
    this.port = config.port || await findFreePort();
    
    this.child = spawn(config.binaryPath, [
      '--port', String(this.port),
      '--config', getConfigPath()
    ], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: { ...process.env, PAPERCLIP_MOONBIT_PORT: String(this.port) }
    });
    
    await this.waitForReady();
    this.startHealthCheck();
  }
  
  private async waitForReady(): Promise<void> {
    // 监听 stdout 中的 "READY" 信号
    // 超时 30s 后抛出错误
  }
  
  private startHealthCheck(): void {
    setInterval(async () => {
      try {
        await fetch(`http://localhost:${this.port}/api/moonbit/health`);
      } catch (e) {
        await this.handleCrash();
      }
    }, config.healthCheckIntervalMs);
  }
}
```

### 6.4 构建与开发流程

**开发模式**:
```bash
# 1. 构建 MoonBit 代码
cd moonbit-core && moon build

# 2. 启动 Express + MoonBit 子进程
cd .. && pnpm dev
```

**生产模式**:
```bash
# 1. 构建 MoonBit 二进制
cd moonbit-core && moon build --release

# 2. 构建 TypeScript
pnpm build

# 3. 启动
pnpm start
```

**package.json 脚本新增**:
```json
{
  "scripts": {
    "build:moonbit": "cd moonbit-core && moon build",
    "build:moonbit:watch": "cd moonbit-core && moon build --watch",
    "dev:moonbit": "cd moonbit-core && moon run server",
    "test:moonbit": "cd moonbit-core && moon test"
  }
}
```

### 6.5 Windows 兼容性

| 方面 | 说明 |
|------|------|
| MoonBit 二进制 | Windows `.exe` 支持 |
| 进程管理 | `child_process.spawn` 跨平台 |
| 路径处理 | 使用 `path.join` 处理路径分隔符 |
| 端口分配 | Windows 端口分配与 Linux 一致 |

---

## 7. 迁移路线图

### 7.1 阶段划分

```
Phase 0: 基础设施 (约 5 天)
├─ MoonBit 环境搭建
├─ HTTP 框架选型与验证
└─ 进程管理器开发

Phase 1: 领域模型 (约 11 天) - A
├─ 常量迁移
├─ Agent 实体 + 状态机
├─ Issue 实体 + 状态机
├─ Company/Goal/Project
└─ TypeScript 集成测试

Phase 2: 预算与成本 (约 5 天) - B
├─ 预算计算引擎
├─ 成本聚合
└─ 验证器迁移

Phase 3: 共享契约 (约 5 天) - C
├─ 类型定义同步
└─ Zod → MoonBit 验证器

Phase 4: 其他服务 (约 5 天)
├─ 插件系统迁移评估
├─ 心跳引擎迁移评估
└─ 全量回归测试
```

### 7.2 Phase 1 详细任务清单

| 序号 | 任务 | 验证标准 |
|------|------|---------|
| 1.1 | 创建 `moonbit-core` 模块结构 | `moon build` 成功 |
| 1.2 | 迁移 `constants.ts` 到 MoonBit | 所有枚举值一致 |
| 1.3 | 实现 Agent 实体 + 状态机 | 状态转换测试 100% 通过 |
| 1.4 | 实现 Issue 实体 + 状态机 | 状态转换测试 100% 通过 |
| 1.5 | 实现 Company/Goal/Project 实体 | 字段验证一致 |
| 1.6 | 开发 HTTP 服务端 | `/health` 端点可用 |
| 1.7 | 开发 TypeScript HTTP 客户端 | 调用验证/状态转换成功 |
| 1.8 | 集成到 Express 服务层 | 现有 API 行为不变 |

---

## 8. 风险与缓解

| 风险 | 影响 | 概率 | 缓解策略 |
|------|------|------|---------|
| MoonBit HTTP 生态不成熟 | HTTP 框架可能不稳定 | 中 | 选择成熟框架，准备 fallback 到 TypeScript |
| 类型同步困难 | TS/MoonBit 类型不一致 | 中 | 建立 JSON Schema 中间表示，自动生成类型 |
| 进程管理复杂度 | 子进程崩溃/重启逻辑复杂 | 低 | 参考 PM2/Supervisor 模式 |
| 性能开销 | HTTP 调用延迟 | 低 | 本地调用通常 <5ms，可接受 |
| 迁移周期过长 | 项目维护成本高 | 中 | 分阶段验证，每阶段可独立发布 |

---

## 9. 验证策略

### 9.1 单元测试 (MoonBit)

```moonbit
// moonbit-core/src/domain/agent/agent_test.mbt
test "agent can transition from idle to running" {
  let agent = make_test_agent(status: Idle)
  assert can_transition(agent.status, Running)
}

test "agent cannot transition from terminated to anything" {
  let agent = make_test_agent(status: Terminated)
  assert !can_transition(agent.status, Running)
  assert !can_transition(agent.status, Idle)
}
```

### 9.2 集成测试 (TypeScript)

```typescript
// tests/moonbit-integration/agent-validation.test.ts
describe('MoonBit Agent Validation', () => {
  it('should reject invalid agent role', async () => {
    const result = await moonbitClient.validateAgent({
      name: 'Test',
      role: 'invalid_role',
    });
    
    expect(result.valid).toBe(false);
    expect(result.errors).toContainEqual(
      expect.objectContaining({ field: 'role' })
    );
  });
});
```

### 9.3 回归测试

- 运行现有 Vitest 套件，确保行为一致
- E2E Playwright 流程测试无破坏
- 所有 Phase 完成后的全量验证

---

## 附录: 减少 pnpm 依赖分析

### 当前状态

```
根级 + packages/* + server + ui + cli ≈ 10+ 个包
```

### 迁移后预期

| 包 | 变化 |
|----|------|
| `packages/shared` | **移除** → 迁移到 MoonBit |
| `packages/db` | **保留** (Drizzle Schema 不变) |
| `packages/adapters/*` | **保留** (适配器不变) |
| `packages/adapter-utils` | **保留** |
| `packages/plugins/*` | **保留** (插件系统不变) |
| `moonbit-core` | **新增** (替代 shared) |

### 减少的依赖

- `zod` → MoonBit 原生验证
- `shared/constants.ts` → MoonBit 枚举
- `shared/types/*` → MoonBit 类型

**预估减少**: 约 **20-30%** 的 pnpm 包依赖

---

*文档结束*
