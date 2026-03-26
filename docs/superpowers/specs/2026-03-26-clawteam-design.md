# ClawTeam 技术设计规格

> **版本**: 1.0.0  
> **日期**: 2026-03-26  
> **状态**: 草稿  
> **基于**: MoonClaw (重构与精简) + Golutra (UI 参考)

---

## 目录

- [1. 项目概述](#1-项目概述)
- [2. 架构设计](#2-架构设计)
- [3. 目录结构](#3-目录结构)
- [4. 核心数据结构](#4-核心数据结构)
- [5. 核心数据流](#5-核心数据流)
- [6. UI 扩展设计](#6-ui-扩展设计)
- [7. 配置文件格式](#7-配置文件格式)
- [8. 复用策略](#8-复用策略)
- [9. 删除模块清单](#9-删除模块清单)
- [10. 测试策略](#10-测试策略)

---

## 1. 项目概述

### 1.1 项目定位

ClawTeam 是一个基于 MoonBit 构建的**纯调度层工具**，核心价值在于**通过聊天式交互统一编排外部 CLI 工具**（Claude Code、Codex、Gemini、OpenClaw、OpenCode、Qwen 等）。

### 1.2 核心原则

| 职责 | 归属 |
|------|------|
| 消息路由、任务拆解、PTY 调用、输出捕获、多渠道接入、定时触发、执行审计 | **ClawTeam 调度层** |
| 安全策略、工作空间管理、文件操作、模型调用、工具执行、状态维护 | **外部 CLI 执行层** |

### 1.3 设计决策

| 决策点 | 选择 |
|--------|------|
| 代码复用策略 | Fork 精简 - 从 MoonClaw 复制代码，删除不需要的模块 |
| PTY 实现 | 复用 MoonClaw spawn - 子进程 + 管道通信 |
| CLI 交互模式 | 混合模式 - 先支持一次性执行，交互式作为增强 |
| 前端技术 | 复用 MoonClaw UI (React 19 + Redux Toolkit) + 参考 Golutra 设计 |
| 渠道优先级 | Web UI → 飞书 → 微信 |
| Job 触发器 | 复用 MoonClaw 完整实现 |

---

## 2. 架构设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                      用户交互层                              │
│  ┌─────────┐ ┌─────────┐ ┌─────────────────────────────┐   │
│  │ Web UI  │ │ Feishu  │ │ Weixin (后续)               │   │
│  └────┬────┘ └────┬────┘ └─────────────────────────────┘   │
│       │           │                                         │
│  ┌────┴───────────┴────────────────────────────────────┐   │
│  │              Channel Manager                        │   │
│  │  (复用 MoonClaw channel/ + channels/)               │   │
│  └────────────────────┬───────────────────────────────┘   │
└───────────────────────┼─────────────────────────────────────┘
                        │ HTTP/WebSocket/SSE
┌───────────────────────┼─────────────────────────────────────┐
│                   Gateway 核心 (MoonBit Native)             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Session 管理  │  RPC 接口  │  消息路由  │  事件广播  │  │
│  │  (复用 MoonClaw gateway/)                             │  │
│  └───────────────────────┬──────────────────────────────┘  │
│                          │                                  │
│  ┌───────────────────────┴──────────────────────────────┐  │
│  │                   Scheduler (新增)                    │  │
│  │  • 消息 → 命令行模板填充                              │  │
│  │  • Agent 角色路由 (助手/员工/监工)                    │  │
│  │  • CLI 适配器注册表                                   │  │
│  │  • 输出解析器链                                       │  │
│  └───────────┬───────────────────────────┬──────────────┘  │
│              │                           │                  │
│  ┌───────────▼───────────┐ ┌─────────────▼──────────────┐  │
│  │    ACP Runtime        │ │     Job Runtime            │  │
│  │  (精简复用 acp/)      │ │  (复用 MoonClaw job/)      │  │
│  │  • CLI 进程调用       │ │  • 任务定义与调度          │  │
│  │  • 输出流捕获         │ │  • 工作流引擎              │  │
│  │  • 进程生命周期管理   │ │  • 制品存储                │  │
│  └───────────┬───────────┘ └────────────────────────────┘  │
│              │                                              │
│  ┌───────────▼───────────────────────────────────────────┐ │
│  │                 Logger (新增)                          │ │
│  │  • 执行审计日志                                        │ │
│  │  • 输出文本存储                                        │ │
│  │  • 元数据索引                                          │ │
│  └───────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                        │ spawn (子进程)
┌───────────────────────▼─────────────────────────────────────┐
│                外部 CLI 工具池 (完全自治)                    │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │ Claude Code │ │   Codex     │ │   Gemini    │           │
│  └─────────────┘ └─────────────┘ └─────────────┘           │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │  OpenClaw   │ │   Qwen      │ │  OpenCode   │           │
│  └─────────────┘ └─────────────┘ └─────────────┘           │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Agent 角色系统

```
用户: "生成用户管理模块"
         │
         ▼
┌─────────────────┐
│ 助手 (Assistant)│
│ 1. 理解需求      │
│ 2. 决策: 需要代码生成 + 审核
│ 3. 分配任务      │
└────────┬────────┘
         │
    ┌────┴────┐
    ▼         ▼
┌───────┐ ┌───────────┐
│ 员工   │ │ 监工(Supervisor) │
│(Worker)│ │ 等待审核   │
│ 执行   │ │           │
└───┬───┘ └─────┬─────┘
    │           │
    ▼           │
  完成反馈 ──────┼─────────┐
    │           ▼         │
    │     ┌───────────┐   │
    │     │ 监工      │   │
    │     │ 审核中    │   │
    │     └─────┬─────┘   │
    │           │         │
    │           ▼         │
    │     ┌───────────┐   │
    │     │ 审核结果  │   │
    │     └─────┬─────┘   │
    │           │         │
    └───────────┼─────────┘
                ▼
         ┌─────────────────┐
         │ 助手 (Assistant)│
         │ 汇总结果        │
         │ 反馈给用户      │
         └─────────────────┘
```

---

## 3. 目录结构

```
clawteam/
├── gateway/              # 复用 MoonClaw
│   ├── server/           # HTTP/RPC 服务、Session 管理
│   ├── client/           # Gateway 客户端 SDK
│   └── protocol/         # RPC 协议定义
│
├── job/                  # 复用 MoonClaw
│   ├── runtime.mbt       # 任务运行时管理
│   ├── workflow_engine.mbt # 工作流引擎
│   ├── executor.mbt      # 执行生命周期
│   └── system_types.mbt  # 数据模型
│
├── channel/              # 复用 MoonClaw
│   ├── types.mbt         # Channel trait 定义
│   ├── extension.mbt     # ChannelExtension trait
│   └── extension_registry.mbt # 扩展注册
│
├── channels/             # 复用 MoonClaw
│   ├── feishu/           # 飞书渠道实现
│   └── weixin/           # 微信渠道实现 (后续)
│
├── scheduler/            # 【新增】核心调度层
│   ├── mod.mbt           # 模块入口
│   ├── agent_router.mbt  # Agent 角色路由
│   ├── template.mbt      # 消息 → 命令行模板填充
│   └── dispatcher.mbt    # 调度执行器
│
├── parser/               # 【新增】输出解析
│   ├── mod.mbt           # 模块入口
│   └── regex_parser.mbt  # 正则解析器
│
├── logger/               # 【新增】执行审计
│   ├── mod.mbt           # 模块入口
│   ├── audit_store.mbt   # 审计日志存储
│   └── query.mbt         # 日志查询接口
│
├── acp/                  # 精简复用 MoonClaw
│   ├── runtime.mbt       # ACP 运行时 (仅保留 CLI 调用)
│   ├── runner.mbt        # CLI 进程执行
│   └── ui_events.mbt     # UI 事件发射
│
├── model/                # 精简复用 MoonClaw
│   └── loader.mbt        # 模型配置加载
│
├── ai/                   # 精简复用 MoonClaw
│   ├── message.mbt       # 消息类型定义
│   └── convert.mbt       # 消息格式转换
│
├── oauth/                # 精简复用 MoonClaw
│   └── codex.mbt         # Codex OAuth 认证
│
├── ui/                   # 复用 + 扩展 MoonClaw
│   ├── core/             # React 核心组件库
│   ├── web/              # Web 应用
│   ├── rabbita-job/      # Job 可视化
│   └── clawteam-chat/    # 【新增】多 Agent 聊天扩展
│
├── event/                # 复用 MoonClaw
│   └── event.mbt         # 事件类型定义
│
├── clock/                # 复用 MoonClaw
│   └── timestamp.mbt     # 时间戳工具
│
├── internal/             # 复用 MoonClaw 工具库
│   ├── httpx/            # HTTP 服务
│   ├── spawn/            # 进程调用
│   ├── pino/             # 日志系统
│   ├── fsx/              # 文件系统
│   ├── broadcast/        # 事件广播
│   ├── uuid/             # UUID 生成
│   └── ...
│
├── cmd/                  # 复用 MoonClaw
│   └── gateway/          # gateway 命令入口
│
└── clawteam.mbt          # 主入口
```

---

## 4. 核心数据结构

### 4.1 Agent 角色与配置

```moonbit
/// Agent 角色定义
pub enum AgentRole {
  Assistant    // 助手：理解需求、决策、分配任务（必须，默认）
  Worker       // 员工：执行任务、反馈给助手
  Supervisor   // 监工：核查任务完成情况、反馈给助手
} derive(ToJson, FromJson, Show, Eq)

/// 终端类型枚举
pub enum TerminalType {
  Claude
  Codex
  Gemini
  OpenCode
  Qwen
  OpenClaw
  Shell
} derive(ToJson, FromJson, Show, Eq)

/// 终端会话状态
pub enum TerminalSessionStatus {
  Pending   // 创建中
  Online    // 就绪，等待输入
  Working   // 执行中
  Offline   // 已退出
  Broken    // 启动失败
} derive(ToJson, FromJson, Show, Eq)

/// Agent 配置
pub struct AgentConfig {
  id : String
  name : String
  role : AgentRole
  terminal_type : TerminalType
  command : String?
  args : Array[String]
  env : Map[String, String]
  cwd : String?
  auto_start : Bool
  // 角色特定配置
  role_config : RoleConfig?
  // 关联的 Skills
  skills : Array[String]
  enabled : Bool
}

/// 角色特定配置
pub enum RoleConfig {
  Assistant(AssistantConfig)
  Worker(WorkerConfig)
  Supervisor(SupervisorConfig)
}

/// 助手配置
pub struct AssistantConfig {
  available_workers : Array[String]
  available_supervisors : Array[String]
  dispatch_strategy : DispatchStrategy
}

/// 员工配置
pub struct WorkerConfig {
  assistant_id : String?
  capabilities : Array[String]
}

/// 监工配置
pub struct SupervisorConfig {
  assistant_id : String?
  review_criteria : Array[String]
}

/// 派发策略
pub enum DispatchStrategy {
  RoundRobin      // 轮询
  LeastLoaded     // 最少负载
  CapabilityMatch // 能力匹配
  Manual          // 手动指定
} derive(ToJson, FromJson, Show, Eq)
```

### 4.2 Skill 系统

```moonbit
/// Skill 定义
pub struct Skill {
  id : String
  name : String
  name_key : String          // 国际化 key
  description : String
  icon : String
  color : String
  bg : String
  ring : String
  version : String
  tags : Array[String]
  applicable_roles : Array[AgentRole]
  template : SkillTemplate
}

/// Skill 执行模板
pub struct SkillTemplate {
  message_template : String
  variables : Array[SkillVariable]
  timeout_ms? : Int
}

pub struct SkillVariable {
  name : String
  label : String
  var_type : SkillVarType
  required : Bool
  default? : String
}

pub enum SkillVarType {
  Text
  Number
  File
  Directory
  Select(Array[String])
} derive(ToJson, FromJson, Show)
```

### 4.3 扩展 MoonClaw ACP

```moonbit
/// CLI 目标扩展配置（存储在 AcpTarget.metadata 中）
pub struct CliTargetMetadata {
  terminal_type : TerminalType
  ready_patterns : Array[String]      // 就绪提示符，如 ["╭─"]
  working_patterns : Array[String]    // 工作中标识，如 ["Thinking..."]
  output_patterns : OutputPatterns?
  auto_start : Bool
  dispatch_merge_window_ms : Int?
}

/// 输出解析模式
pub struct OutputPatterns {
  success_pattern? : String
  error_pattern? : String
  progress_pattern? : String
}

/// 调度器扩展（挂载到 Gateway）
pub struct SchedulerExtension {
  acp_runtime : @acp.AcpRuntime
  agent_router : AgentRouter
  status_poller : StatusPoller
}

/// Agent 路由
pub struct AgentRouter {
  default_assistant : String?
  agent_configs : Map[String, AgentConfig]
}

/// 会话状态轮询
pub struct StatusPoller {
  sessions : Map[String, SessionPollState]
  silence_timeout_ms : Int     // Working → Online 回落阈值 (默认 4500)
  idle_debounce_ms : Int       // 防抖间隔 (默认 1000)
}

pub struct SessionPollState {
  session_id : String
  last_output_at : Int
  last_status : TerminalSessionStatus
  working_since : Int?
}
```

### 4.4 审计日志

```moonbit
pub struct AuditLogEntry {
  id : String
  timestamp : Int
  event_type : AuditEventType
  session_id : String?
  target_id : String?
  job_id : String?
  run_id : String?
  user_id : String?
  channel : String?
  details : Json
}

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

---

## 5. 核心数据流

### 5.1 消息调度流程

```
用户消息 → Channel → Gateway → SchedulerExtension
                                    │
                                    ▼
                         AgentRouter 角色路由
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
                Assistant        Worker        Supervisor
                    │               │               │
                    └───────────────┼───────────────┘
                                    ▼
                         检查 Session 状态
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
                  Online         Working         Offline
                    │               │               │
                    ▼               ▼               ▼
              立即派发        加入队列等待     启动新进程
                    │               │               │
                    └───────────────┼───────────────┘
                                    ▼
                         @spawn.spawn() 执行 CLI
                                    │
                                    ▼
                         捕获 stdout/stderr
                                    │
                                    ▼
                         OutputPatterns 解析
                                    │
                                    ▼
                         更新 AcpRun 状态
                                    │
                                    ▼
                         广播事件到 UI/Channel
```

### 5.2 Job 工作流与 CLI 集成

```moonbit
/// 新增 Workflow Step Handler: cli_dispatch
/// 
/// 配置示例 (config 字段):
/// {
///   "targetId": "claude-assistant",
///   "messageTemplate": "基于 {{outline_path}} 撰写正文",
///   "variables": { "outline_path": "/tmp/outline.md" },
///   "waitForCompletion": true,
///   "timeoutMs": 120000
/// }

pub async fn cli_dispatch_handler(
  context : WorkflowStepContext,
) -> Result[WorkflowStepResult, String]
```

---

## 6. UI 扩展设计

### 6.1 技术栈

| 技术 | 版本 | 用途 |
|------|------|------|
| React | ^19.2.0 | UI 框架 |
| Redux Toolkit | ^2.10.1 | 状态管理 |
| Tailwind CSS | ^4.1.17 | 样式系统 |
| Radix UI | - | 组件库 |
| Shiki | ^3.15.0 | 代码高亮 |
| Monaco Editor | ^0.55.1 | 代码编辑 |
| Vite | ^7.2.2 | 构建工具 |

### 6.2 目录结构

```
ui/
├── core/                           # 复用 MoonClaw Core
│   └── src/
│       ├── components/             # 复用现有组件
│       └── features/
│           └── chat/               # 【新增】聊天编排模块
│
└── clawteam-chat/                  # 【新增】多 Agent 聊天扩展
    ├── package.json
    └── src/
        ├── components/
        │   ├── AgentPanel.tsx      # Agent 成员面板
        │   ├── AgentCard.tsx       # Agent 卡片
        │   ├── ChatInterface.tsx   # 聊天主界面
        │   ├── MessageList.tsx     # 消息列表
        │   ├── ChatInput.tsx       # 输入框
        │   ├── OutputPanel.tsx     # CLI 输出面板
        │   ├── SkillStore.tsx      # Skill 商店
        │   └── AgentConfig.tsx     # Agent 配置
        ├── store/
        │   ├── chatSlice.ts
        │   ├── agentSlice.ts
        │   ├── skillSlice.ts
        │   └── auditSlice.ts
        └── types/
            ├── agent.ts
            ├── skill.ts
            └── message.ts
```

### 6.3 UI 布局

```
┌─────────────────────────────────────────────────────────────────────────┐
│  ClawTeam Operator UI                    [Agent配置] [Skill商店] [设置]  │
├────────────┬────────────────────────────────────────────────────────────┤
│            │  聊天面板                                                  │
│  Agent     │  ┌──────────────────────────────────────────────────────┐ │
│  面板      │  │ [用户] 生成用户管理模块的 CRUD 接口                    │ │
│            │  ├──────────────────────────────────────────────────────┤ │
│  ┌────────┐│  │ [助手-Claude] 收到任务，正在分析...                    │ │
│  │助手    ││  │   → 派发给 [Codex-员工] 执行代码生成                  │ │
│  │Claude  ││  ├──────────────────────────────────────────────────────┤ │
│  │● 决策中││  │ [员工-Codex] 正在生成 user_api.rs...                  │ │
│  │✓ 必须  ││  ├──────────────────────────────────────────────────────┤ │
│  └────────┘│  │ [监工-Gemini] 开始审核...                             │ │
│  ┌────────┐│  ├──────────────────────────────────────────────────────┤ │
│  │员工    ││  │ [助手-Claude] 任务完成，审核通过                      │ │
│  │Codex   ││  └──────────────────────────────────────────────────────┘ │
│  │● 执行中││                                                            │
│  │✓ 可选  ││  输入框                                                    │
│  └────────┘│  ┌──────────────────────────────────────────────────────┐ │
│  ┌────────┐│  │ [输入消息...]                    [发送] [选择Skill ▼] │ │
│  │监工    ││  └──────────────────────────────────────────────────────┘ │
│  │Gemini  ││                                                            │
│  │○ 等待  ││  CLI 输出面板                                              │
│  │✓ 可选  ││  ┌──────────────────────────────────────────────────────┐ │
│  └────────┘│  │ $ claude generate-api                                 │ │
│            │  │ Generating user_api.rs...                              │ │
│  [+ 添加]  │  │ ✓ Complete                                             │ │
│            │  └──────────────────────────────────────────────────────┘ │
├────────────┴────────────────────────────────────────────────────────────┤
│  [📋 原始输出] [🔍 搜索] [⏸️ 暂停] [⏹️ 终止] [📋 审计日志]              │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 7. 配置文件格式

### 7.1 主配置文件 `~/.clawteam/clawteam.json`

```json
{
  "version": "1.0.0",
  "gateway": {
    "port": 3000,
    "host": "127.0.0.1"
  },
  "workspace": {
    "path": "~/projects/my-project",
    "name": "My Project"
  },
  "agents": [
    {
      "id": "claude-assistant",
      "name": "Claude",
      "role": "assistant",
      "terminalType": "claude",
      "command": "claude",
      "args": [],
      "env": {},
      "cwd": null,
      "autoStart": true,
      "enabled": true,
      "readyPatterns": ["╭─"],
      "workingPatterns": ["Thinking...", "Working..."],
      "roleConfig": {
        "availableWorkers": ["codex-worker", "qwen-worker"],
        "availableSupervisors": ["gemini-supervisor"],
        "dispatchStrategy": "capability-match"
      },
      "skills": []
    },
    {
      "id": "codex-worker",
      "name": "Codex",
      "role": "worker",
      "terminalType": "codex",
      "command": "codex",
      "args": [],
      "enabled": true,
      "autoStart": false,
      "readyPatterns": [">"],
      "workingPatterns": [],
      "roleConfig": {
        "assistantId": "claude-assistant",
        "capabilities": ["code-generation", "refactoring", "testing"]
      },
      "skills": []
    },
    {
      "id": "gemini-supervisor",
      "name": "Gemini",
      "role": "supervisor",
      "terminalType": "gemini",
      "command": "gemini",
      "args": [],
      "enabled": true,
      "autoStart": false,
      "readyPatterns": [">"],
      "workingPatterns": [],
      "roleConfig": {
        "assistantId": "claude-assistant",
        "reviewCriteria": ["code-style", "type-safety", "test-coverage"]
      },
      "skills": []
    }
  ],
  "skills": [
    {
      "id": "generate-api",
      "name": "Generate API",
      "nameKey": "skills.generateApi",
      "description": "Generate REST API endpoints",
      "icon": "code",
      "color": "#10b981",
      "bg": "#d1fae5",
      "ring": "#34d399",
      "version": "1.0.0",
      "tags": ["generation"],
      "applicableRoles": ["worker"],
      "template": {
        "messageTemplate": "Generate {{method}} {{path}} API endpoint for {{entity}}",
        "variables": [
          { "name": "method", "label": "HTTP Method", "type": "select", "options": ["GET", "POST", "PUT", "DELETE"], "required": true },
          { "name": "path", "label": "API Path", "type": "text", "required": true },
          { "name": "entity", "label": "Entity Name", "type": "text", "required": true }
        ]
      }
    }
  ],
  "channels": {
    "web": { "enabled": true },
    "feishu": { "enabled": false, "appId": "", "appSecret": "" },
    "weixin": { "enabled": false, "appId": "", "appSecret": "" }
  },
  "audit": {
    "enabled": true,
    "retentionDays": 30,
    "logPath": "~/.clawteam/logs/audit.jsonl"
  }
}
```

### 7.2 Job 定义文件 `~/.clawteam/jobs/daily-blog.json`

```json
{
  "id": "daily-blog",
  "kind": "scheduled",
  "enabled": true,
  "schedule": {
    "kind": "cron",
    "expression": "0 9 * * *"
  },
  "workflow": {
    "id": "blog-workflow",
    "steps": [
      {
        "id": "research",
        "kind": "cli_dispatch",
        "config": {
          "targetId": "claude-assistant",
          "messageTemplate": "调研 {{topic}} 最新进展并生成大纲",
          "variables": { "topic": "Rust 异步编程" },
          "waitForCompletion": true,
          "timeoutMs": 300000
        }
      },
      {
        "id": "draft",
        "kind": "cli_dispatch",
        "config": {
          "targetId": "codex-worker",
          "messageTemplate": "基于上一步的大纲撰写正文",
          "waitForCompletion": true,
          "timeoutMs": 600000
        }
      },
      {
        "id": "review",
        "kind": "cli_dispatch",
        "config": {
          "targetId": "gemini-supervisor",
          "messageTemplate": "审核文章内容准确性与可读性",
          "waitForCompletion": true,
          "timeoutMs": 180000
        }
      },
      {
        "id": "human-confirm",
        "kind": "human_approval",
        "config": {
          "message": "文章已生成，请确认是否发布",
          "timeoutMs": 3600000
        }
      },
      {
        "id": "publish",
        "kind": "cli_dispatch",
        "config": {
          "targetId": "qwen-worker",
          "messageTemplate": "格式化 Markdown 并发布到博客平台",
          "waitForCompletion": true,
          "timeoutMs": 60000
        }
      }
    ]
  }
}
```

---

## 8. 复用策略

### 8.1 完全复用（无修改）

| 模块 | 来源 | 说明 |
|------|------|------|
| `gateway/` | MoonClaw | HTTP/RPC 服务完整复用 |
| `job/` | MoonClaw | 任务调度与工作流完整复用 |
| `channel/` | MoonClaw | 渠道抽象完整复用 |
| `channels/feishu/` | MoonClaw | 飞书实现完整复用 |
| `channels/weixin/` | MoonClaw | 微信实现完整复用 |
| `event/` | MoonClaw | 事件系统完整复用 |
| `clock/` | MoonClaw | 时间工具完整复用 |
| `internal/` | MoonClaw | 工具库完整复用 |
| `cmd/` | MoonClaw | CLI 入口完整复用 |

### 8.2 精简复用（删除部分代码）

| 模块 | 来源 | 保留 | 删除 |
|------|------|------|------|
| `acp/` | MoonClaw | CLI 进程调用逻辑 | Codex 特定逻辑（保留通用部分） |
| `model/` | MoonClaw | 模型配置加载 | 模型选择逻辑 |
| `ai/` | MoonClaw | 消息类型定义 | LLM 调用逻辑 |
| `oauth/` | MoonClaw | Codex 认证 | 其他 OAuth |

### 8.3 扩展新增

| 模块 | 说明 |
|------|------|
| `scheduler/` | Agent 角色路由、消息模板填充、调度执行 |
| `parser/` | 输出解析（正则） |
| `logger/` | 执行审计日志 |
| `ui/clawteam-chat/` | 多 Agent 聊天 UI 扩展 |

---

## 9. 删除模块清单

| 模块 | 原因 |
|------|------|
| `agent/` | Agent 运行时 - 业务逻辑由外部 CLI 负责 |
| `tool/` | 工具抽象 - 业务逻辑由外部 CLI 负责 |
| `tools/` | 工具实现 - 业务逻辑由外部 CLI 负责 |
| `workspace/` | 工作空间管理 - 业务逻辑由外部 CLI 负责 |
| `security/` | 安全运行时 - 业务逻辑由外部 CLI 负责 |
| `file/` | 文件管理器 - 业务逻辑由外部 CLI 负责 |
| `prompt/` | 系统提示词 - 不需要 |
| `plugin/` | 插件运行时 - 暂不需要 |
| `skills/` | 技能加载器 - 用新 Skill 系统替代 |
| `onboarding/` | 用户引导 - 标记废弃 |

---

## 10. 测试策略

### 10.1 单元测试

| 测试范围 | 工具 | 覆盖目标 |
|----------|------|----------|
| Agent 路由 | MoonBit test + inspect | 80%+ |
| 消息模板填充 | MoonBit test + inspect | 90%+ |
| 输出解析 | MoonBit test + inspect | 85%+ |
| 审计日志 | MoonBit test + inspect | 80%+ |

### 10.2 集成测试

| 测试范围 | 工具 | 覆盖目标 |
|----------|------|----------|
| 多 Agent 调度 | CLI 桩 + 模拟 Channel | 关键路径 100% |
| 渠道消息转发 | 模拟 Webhook | 关键路径 100% |
| Job 触发 | Cron 桩 | 关键路径 100% |

### 10.3 端到端测试

| 测试范围 | 工具 | 覆盖目标 |
|----------|------|----------|
| 用户聊天→CLI 执行→输出反馈 | Playwright + 测试 CLI 桩 | 核心场景 3+ |

---

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 1.0.0 | 2026-03-26 | 初始设计文档 |
