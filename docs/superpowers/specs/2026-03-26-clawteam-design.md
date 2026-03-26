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

### 4.1 CLI 工具定义

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

/// CLI 工具配置
pub struct CliToolConfig {
  tool_type : CliToolType
  command : String
  args : Array[String]
  env : Map[String, String]
  cwd : String?
  ready_patterns : Array[String]      // 就绪提示符，如 ["╭─"]
  working_patterns : Array[String]    // 工作中标识，如 ["Thinking..."]
  output_patterns : OutputPatterns?
  timeout? : Int
}

/// CLI 工具注册表
pub struct CliToolRegistry {
  tools : Map[String, CliToolConfig]  // key: 工具名称，如 "opencode"
}
```

### 4.2 Agent 角色与配置

```moonbit
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

/// Agent 配置
pub struct AgentConfig {
  id : String
  name : String
  role : AgentRole
  cli_tool : String              // 引用 CliToolRegistry 中的工具名称
  args : Array[String]           // 额外参数（覆盖默认）
  env : Map[String, String]      // 额外环境变量（覆盖默认）
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
│  ┌────────┐│  │ [助手-主助手/OpenCode] 收到任务，正在分析...           │ │
│  │助手    ││  │   → 派发给 [员工-代码生成器/OpenCode] 执行代码生成    │ │
│  │主助手  ││  ├──────────────────────────────────────────────────────┤ │
│  │● 决策中││  │ [员工-代码生成器/OpenCode] 正在生成 user_api.rs...    │ │
│  │OpenCode││  ├──────────────────────────────────────────────────────┤ │
│  │✓ 必须  ││  │ [监工-代码审核/Gemini] 开始审核...                    │ │
│  └────────┘│  ├──────────────────────────────────────────────────────┤ │
│  ┌────────┐│  │ [助手-主助手/OpenCode] 任务完成，审核通过              │ │
│  │员工    ││  └──────────────────────────────────────────────────────┘ │
│  │代码生成││                                                            │
│  │● 执行中││  输入框                                                    │
│  │OpenCode││  ┌──────────────────────────────────────────────────────┐ │
│  │✓ 可选  ││  │ [输入消息...]                    [发送] [选择Skill ▼] │ │
│  └────────┘│  └──────────────────────────────────────────────────────┘ │
│  ┌────────┐│                                                            │
│  │监工    ││  CLI 输出面板                                              │
│  │代码审核││  ┌──────────────────────────────────────────────────────┐ │
│  │○ 等待  ││  │ $ opencode generate-api                               │ │
│  │Gemini  ││  │ Generating user_api.rs...                              │ │
│  │✓ 可选  ││  │ ✓ Complete                                             │ │
│  └────────┘│  └──────────────────────────────────────────────────────┘ │
│            │                                                            │
│  [+ 添加]  │                                                            │
├────────────┴────────────────────────────────────────────────────────────┤
│  [📋 原始输出] [🔍 搜索] [⏸️ 暂停] [⏹️ 终止] [📋 审计日志]              │
└─────────────────────────────────────────────────────────────────────────┘

Agent 配置弹窗:
┌─────────────────────────────────────────┐
│ 配置 Agent                              │
├─────────────────────────────────────────┤
│ 名称: [主助手          ]                │
│ 角色: [助手 ▼] (助手/员工/监工)          │
│                                         │
│ CLI 工具: [OpenCode ▼]                  │
│   ┌─────────────────────────────────┐   │
│   │ ○ Claude Code                   │   │
│   │ ○ Codex                         │   │
│   │ ○ Gemini                        │   │
│   │ ● OpenCode                      │   │
│   │ ○ Qwen                          │   │
│   │ ○ OpenClaw                      │   │
│   └─────────────────────────────────┘   │
│                                         │
│ 自动启动: [✓]                           │
│                                         │
│         [取消]  [保存]                  │
└─────────────────────────────────────────┘
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
  "cliTools": {
    "claude": {
      "command": "claude",
      "readyPatterns": ["╭─"],
      "workingPatterns": ["Thinking...", "Working..."]
    },
    "codex": {
      "command": "codex",
      "readyPatterns": [">"],
      "workingPatterns": []
    },
    "gemini": {
      "command": "gemini",
      "readyPatterns": [">"],
      "workingPatterns": []
    },
    "opencode": {
      "command": "opencode",
      "readyPatterns": [">"],
      "workingPatterns": ["Processing..."]
    },
    "qwen": {
      "command": "qwen",
      "readyPatterns": [">"],
      "workingPatterns": []
    },
    "openclaw": {
      "command": "openclaw",
      "readyPatterns": [">"],
      "workingPatterns": []
    }
  },
  "agents": [
    {
      "id": "assistant-1",
      "name": "主助手",
      "role": "assistant",
      "cliTool": "opencode",
      "args": [],
      "env": {},
      "cwd": null,
      "autoStart": true,
      "enabled": true,
      "roleConfig": {
        "availableWorkers": ["worker-1", "worker-2"],
        "availableSupervisors": ["supervisor-1"],
        "dispatchStrategy": "capability-match"
      },
      "skills": []
    },
    {
      "id": "worker-1",
      "name": "代码生成器",
      "role": "worker",
      "cliTool": "opencode",
      "args": [],
      "enabled": true,
      "autoStart": false,
      "roleConfig": {
        "assistantId": "assistant-1",
        "capabilities": ["code-generation", "refactoring", "testing"]
      },
      "skills": []
    },
    {
      "id": "worker-2",
      "name": "Codex 执行者",
      "role": "worker",
      "cliTool": "codex",
      "args": [],
      "enabled": true,
      "autoStart": false,
      "roleConfig": {
        "assistantId": "assistant-1",
        "capabilities": ["code-generation", "debugging"]
      },
      "skills": []
    },
    {
      "id": "supervisor-1",
      "name": "代码审核",
      "role": "supervisor",
      "cliTool": "gemini",
      "args": [],
      "enabled": true,
      "autoStart": false,
      "roleConfig": {
        "assistantId": "assistant-1",
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

## 11. 错误处理

### 11.1 错误类型定义

```moonbit
pub enum ClawTeamError {
  // 配置错误
  ConfigNotFound(String)
  ConfigInvalid(String)
  AgentNotFound(String)
  SkillNotFound(String)
  
  // 进程错误
  ProcessStartFailed(String)
  ProcessTimeout(String)
  ProcessCrashed(Int)           // 退出码
  
  // 调度错误
  NoAvailableAgent
  AgentBusy(String)
  DispatchFailed(String)
  
  // 渠道错误
  ChannelError(String)
  WebhookInvalid(String)
  
  // 审计错误
  AuditLogWriteFailed(String)
}

pub fn ClawTeamError::to_message(self : ClawTeamError) -> String {
  match self {
    ConfigNotFound(path) => "配置文件未找到: \{path}"
    ConfigInvalid(msg) => "配置文件无效: \{msg}"
    AgentNotFound(id) => "Agent 未找到: \{id}"
    SkillNotFound(id) => "Skill 未找到: \{id}"
    ProcessStartFailed(cmd) => "进程启动失败: \{cmd}"
    ProcessTimeout(timeout) => "进程超时 (\{timeout}ms)"
    ProcessCrashed(code) => "进程崩溃，退出码: \{code}"
    NoAvailableAgent => "无可用 Agent"
    AgentBusy(id) => "Agent 忙碌: \{id}"
    DispatchFailed(msg) => "派发失败: \{msg}"
    ChannelError(msg) => "渠道错误: \{msg}"
    WebhookInvalid(msg) => "Webhook 无效: \{msg}"
    AuditLogWriteFailed(msg) => "审计日志写入失败: \{msg}"
  }
}
```

### 11.2 错误处理策略

| 错误类型 | 处理策略 | 用户反馈 |
|----------|----------|----------|
| 配置错误 | 启动时检查，失败则退出 | 控制台错误信息 |
| 进程启动失败 | 记录审计日志，返回错误 | UI 显示错误提示 |
| 进程超时 | 终止进程，记录审计 | UI 显示超时提示 |
| Agent 忙碌 | 加入队列等待 | UI 显示排队状态 |
| 渠道错误 | 记录日志，重试（可配置） | 渠道错误回执 |

### 11.3 重试策略

```moonbit
pub struct RetryConfig {
  max_attempts : Int
  backoff_ms : Int
  backoff_multiplier : Double
}

let DEFAULT_RETRY = RetryConfig::{
  max_attempts: 3,
  backoff_ms: 1000,
  backoff_multiplier: 2.0,
}

pub async fn with_retry[T](
  config : RetryConfig,
  f : async () -> Result[T, ClawTeamError],
) -> Result[T, ClawTeamError] {
  let mut attempt = 0
  let mut delay = config.backoff_ms
  loop {
    match f() {
      Ok(result) => return Ok(result)
      Err(error) => {
        attempt += 1
        if attempt >= config.max_attempts {
          return Err(error)
        }
        @clock.sleep(delay)
        delay = (delay.to_double() * config.backoff_multiplier).to_int()
      }
    }
  }
}
```

---

## 12. Agent 协作流程详解

### 12.1 调用链设计

ClawTeam 通过多次 CLI 调用实现 Agent 协作，每次调用传入对应 Agent 的角色提示词和上下文：

```
用户输入 → 助手 CLI 调用 → 助手分析决策 → 员工 CLI 调用 → 员工执行任务 → 监工 CLI 调用 → 监工审核
```

#### 调用链示例

```
Step 1: 助手接收用户输入
─────────────────────────────────────────────────────────
终端调用: opencode -p "[助手 agent-prompt], [用户输入]"

CLI 命令:
  opencode 
  -p "你是一个助手角色，负责理解用户需求并分配任务给合适的员工。
      当前可用员工: [员工A, 员工B]
      当前可用监工: [监工A]
      
      用户输入: 生成用户管理模块的 CRUD 接口"
      
助手分析结果:
  - 意图: 代码生成
  - 需要审核: 是
  - 分配任务: 员工A 执行代码生成，监工A 审核


Step 2: 助手派发任务给员工A
─────────────────────────────────────────────────────────
终端调用: opencode -p "[员工A agent-prompt], [skill], [助手分配任务1]"

CLI 命令:
  opencode 
  -p "你是一个员工角色，负责执行助手分配的任务。
      你的能力: [code-generation, refactoring, testing]
      
      使用技能: generate-api
      
      助手分配任务: 为用户管理模块生成 CRUD 接口，包含:
        - GET /users - 获取用户列表
        - POST /users - 创建用户
        - PUT /users/:id - 更新用户
        - DELETE /users/:id - 删除用户"
      
员工A 执行结果:
  - 生成文件: user_api.rs, user_service.rs, user_test.rs
  - 状态: 完成


Step 3: 助手派发任务给监工A
─────────────────────────────────────────────────────────
终端调用: gemini -p "[监工A agent-prompt], [审核任务]"

CLI 命令:
  gemini 
  -p "你是一个监工角色，负责审核员工的工作成果。
      审核标准: [code-style, type-safety, test-coverage]
      
      审核对象: 员工A 生成的用户管理 CRUD 接口
      文件列表: user_api.rs, user_service.rs, user_test.rs"
      
监工A 审核结果:
  - 代码风格: ✓ 通过
  - 类型安全: ✓ 通过
  - 测试覆盖: ✓ 通过
  - 结论: 通过


Step 4: 助手汇总结果反馈用户
─────────────────────────────────────────────────────────
助手输出:
  "已完成用户管理模块的 CRUD 接口生成:
   - 生成了 3 个文件
   - 代码审核通过
   - 文件位置: src/user/"
```

### 12.2 Agent Prompt 模板

每个 Agent 角色有对应的系统提示词模板：

```moonbit
/// Agent 角色提示词模板
pub struct AgentPromptTemplate {
  role : AgentRole
  system_prompt : String
  available_tools : Array[String]
}

/// 助手提示词模板
let ASSISTANT_PROMPT_TEMPLATE = """
你是一个助手角色，负责：
1. 理解用户需求
2. 分析任务复杂度
3. 决定任务分配策略
4. 汇总员工和监工的反馈

当前可用员工: {{available_workers}}
当前可用监工: {{available_supervisors}}
派发策略: {{dispatch_strategy}}

请分析用户输入，决定如何分配任务。
"""

/// 员工提示词模板
let WORKER_PROMPT_TEMPLATE = """
你是一个员工角色，负责：
1. 执行助手分配的任务
2. 使用指定技能完成任务
3. 向助手反馈执行结果

你的能力: {{capabilities}}
使用技能: {{skill}}

请执行助手分配的任务。
"""

/// 监工提示词模板
let SUPERVISOR_PROMPT_TEMPLATE = """
你是一个监工角色，负责：
1. 审核员工的工作成果
2. 检查是否符合标准
3. 向助手反馈审核结果

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
```

### 12.3 CLI 调用封装

```moonbit
/// CLI 调用参数
pub struct CliCallParams {
  cli_tool : String              // 工具名称，如 "opencode"
  agent_prompt : String          // Agent 角色提示词
  context : String               // 上下文（用户输入/任务/审核对象）
  skill? : String                // 使用的技能（可选）
  variables : Map[String, String] // 模板变量
}

/// 构建 CLI 命令
pub fn build_cli_command(
  tool_config : CliToolConfig,
  params : CliCallParams,
) -> Array[String] {
  let prompt = render_prompt(params.agent_prompt, params.variables)
  let full_prompt = match params.skill {
    Some(skill) => "\{prompt}\n\n使用技能: \{skill}\n\n\{params.context}"
    None => "\{prompt}\n\n\{params.context}"
  }
  
  let mut args = tool_config.args.to_array()
  args.push("-p")
  args.push(full_prompt)
  args
}

/// 执行 Agent CLI 调用
pub async fn execute_agent_call(
  agent : AgentConfig,
  cli_registry : CliToolRegistry,
  context : String,
  skill? : String,
) -> Result[CliCallResult, ClawTeamError] {
  // 获取 CLI 工具配置
  let tool_config = cli_registry.tools.get(agent.cli_tool)?
  
  // 构建 prompt
  let prompt_template = get_prompt_template(agent.role)
  let variables = build_variables(agent)
  let agent_prompt = render_prompt(prompt_template, variables)
  
  // 构建 CLI 参数
  let params = CliCallParams::{
    cli_tool: agent.cli_tool,
    agent_prompt,
    context,
    skill?,
    variables: {},
  }
  
  // 执行 CLI
  let command = build_cli_command(tool_config, params)
  let result = @spawn.spawn(
    tool_config.command,
    command,
    cwd=agent.cwd,
    env=merge_env(tool_config.env, agent.env),
  )
  
  Ok(CliCallResult::{
    stdout: result.stdout,
    stderr: result.stderr,
    status: result.status,
  })
}
```

### 12.4 消息流转格式

```moonbit
pub async fn assistant_decision_loop(
  assistant : AgentConfig,
  message : String,
  context : DispatchContext,
) -> Result[AgentDecision, ClawTeamError] {
  // 1. 分析用户意图
  let intent = analyze_intent(message)
  
  // 2. 确定需要的执行步骤
  let steps = match intent {
    Intent::CodeGeneration => [
      Step::Dispatch("worker", "生成代码"),
      Step::Dispatch("supervisor", "审核代码"),
    ]
    Intent::CodeReview => [
      Step::Dispatch("supervisor", "审核代码"),
    ]
    Intent::Refactoring => [
      Step::Dispatch("worker", "重构代码"),
      Step::Dispatch("supervisor", "验证重构"),
    ]
    Intent::Question => [
      Step::DirectReply,
    ]
  }
  
  // 3. 选择合适的 Worker/Supervisor
  let selected_agents = select_agents(assistant, steps)
  
  Ok(AgentDecision::{ steps, selected_agents })
}
```

### 12.2 消息流转格式

```
┌─────────────────────────────────────────────────────────────┐
│ 消息流转示例                                                 │
├─────────────────────────────────────────────────────────────┤
│ [用户] 生成用户管理模块的 CRUD 接口                          │
│                                                             │
│ ──→ [助手-Claude] 分析意图 → 需要代码生成 + 审核             │
│     派发任务: 代码生成 → codex-worker                        │
│                                                             │
│ ──→ [员工-Codex] 执行: 生成 CRUD 代码                       │
│     输出: user_api.rs, user_service.rs                      │
│     状态: 完成 → 通知助手                                    │
│                                                             │
│ ──→ [助手-Claude] 收到完成通知 → 派发审核任务                │
│     派发任务: 代码审核 → gemini-supervisor                   │
│                                                             │
│ ──→ [监工-Gemini] 执行: 审核代码                            │
│     检查: 代码风格 ✓ 类型安全 ✓ 测试覆盖 ✓                  │
│     状态: 通过 → 通知助手                                    │
│                                                             │
│ ──→ [助手-Claude] 汇总结果 → 反馈用户                       │
│     输出: 已生成 3 个文件，审核通过                          │
└─────────────────────────────────────────────────────────────┘
```

### 12.3 状态同步机制

```moonbit
/// Agent 状态广播
pub struct AgentStatusBroadcast {
  agent_id : String
  old_status : TerminalSessionStatus
  new_status : TerminalSessionStatus
  timestamp : Int
  reason : String?
}

/// 消息状态更新
pub struct MessageStatusUpdate {
  message_id : String
  old_status : MessageStatus
  new_status : MessageStatus
  linked_agent : String?
  timestamp : Int
}

/// UI 订阅事件
pub enum UiEvent {
  AgentStatusChanged(AgentStatusBroadcast)
  MessageStatusChanged(MessageStatusUpdate)
  NewMessage(ChatMessage)
  CliOutput(CliOutputChunk)
  AuditEntry(AuditLogEntry)
}
```

---

## 13. 部署与运维

### 13.1 启动命令

```bash
# 启动 Gateway 服务
clawteam gateway --port 3000 --home ~/.clawteam

# 指定配置文件
clawteam gateway --config /path/to/clawteam.json

# 开发模式（启用调试日志）
clawteam gateway --dev

# 后台运行
clawteam gateway --daemon
```

### 13.2 环境变量

| 变量名 | 说明 | 默认值 |
|--------|------|--------|
| `CLAWTEAM_HOME` | 配置目录 | `~/.clawteam` |
| `CLAWTEAM_PORT` | Gateway 端口 | `3000` |
| `CLAWTEAM_LOG_LEVEL` | 日志级别 | `info` |
| `CLAWTEAM_AUDIT_ENABLED` | 审计日志开关 | `true` |

### 13.3 日志轮转

```
~/.clawteam/logs/
├── audit-2026-03-26.jsonl
├── audit-2026-03-25.jsonl
├── gateway.log
└── gateway.log.1
```

---

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 1.0.0 | 2026-03-26 | 初始设计文档 |
| 1.0.1 | 2026-03-26 | 补充错误处理、Agent 协作流程、部署运维章节 |
| 1.0.2 | 2026-03-26 | 将 CLI 工具与 Agent 角色解耦，支持每个角色选择任意 CLI 工具 |
