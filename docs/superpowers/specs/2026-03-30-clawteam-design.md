# ClawTeam 技术设计规格

> **版本**: 1.0.0  
> **日期**: 2026-03-26  
> **状态**: 草稿  
> **基于**: MoonClaw (重构与精简)

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
| 前端技术 | Vue 3.5 + Pinia + @pinia/colada + Tailwind CSS (基于原型 UI.html 设计) |
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

**核心原则：助手只协调，员工负责思考**

```
┌─────────────────────────────────────────────────────────────────────┐
│                        角色职责定义                                  │
├─────────────────────────────────────────────────────────────────────┤
│  助手 (Assistant) - 协调器，不思考                                   │
│  • 接收用户需求                                                      │
│  • 分配任务给合适的员工                                              │
│  • 收集员工反馈并返回给用户确认                                      │
│  • 识别并行任务并分发                                                │
│  • 安排监工审核                                                      │
│  • 处理审核结果（通过→下一步，失败→让员工修正）                       │
├─────────────────────────────────────────────────────────────────────┤
│  员工 (Worker) - 执行者，负责思考                                    │
│  • 需求分析与明确                                                    │
│  • 开发计划制定                                                      │
│  • 任务分解                                                          │
│  • 代码生成、测试、重构等具体工作                                    │
│  • 修正审核发现的问题                                                │
├─────────────────────────────────────────────────────────────────────┤
│  监工 (Supervisor) - 审核者                                          │
│  • 审核员工工作成果                                                  │
│  • 检查是否符合标准                                                  │
│  • 返回审核结果（通过/失败+问题）                                    │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.3 完整工作流示例

```
用户: "帮我做一个用户管理系统"

Step 1: 助手接收模糊需求 → 分配给员工A明确需求
─────────────────────────────────────────────────────────
助手调用: 员工A
  任务: "用户提出了'做一个用户管理系统'的需求，请分析并明确具体需求"

员工A 返回:
  - 用户管理：注册、登录、个人信息管理
  - 权限管理：角色定义、权限分配
  - 安全要求：密码加密、会话管理
  - 请用户确认以上需求是否正确

Step 2: 助手返回明确需求 → 用户确认
─────────────────────────────────────────────────────────
助手输出:
  "我分析了您的需求，请确认：
   1. 用户管理：注册、登录、个人信息管理
   2. 权限管理：角色定义、权限分配
   3. 安全要求：密码加密、会话管理
   
   请确认是否正确？"

用户确认: "正确，开始开发"

Step 3: 助手分配给员工B → 生成开发计划
─────────────────────────────────────────────────────────
助手调用: 员工B
  任务: "根据以下需求生成开发计划：
         1. 用户管理：注册、登录、个人信息管理
         2. 权限管理：角色定义、权限分配
         3. 安全要求：密码加密、会话管理"

员工B 返回:
  开发计划:
    Phase 1: 基础架构搭建
    Phase 2: 用户模块开发
    Phase 3: 权限模块开发
    Phase 4: 安全模块开发
    Phase 5: 集成测试

Step 4: 助手返回开发计划 → 用户确认
─────────────────────────────────────────────────────────
助手输出: "开发计划已生成，请确认后开始执行"
用户确认: "开始"

Step 5: 助手分配给员工C → 生成任务列表
─────────────────────────────────────────────────────────
助手调用: 员工C
  任务: "根据开发计划生成详细任务列表，标注哪些任务可以并行"

员工C 返回:
  任务列表:
    [并行] T1: 数据库设计
    [并行] T2: API 框架搭建
    [并行] T3: 前端项目初始化
    [串行] T4: 用户模块开发（依赖T1,T2）
    [串行] T5: 权限模块开发（依赖T4）
    [串行] T6: 安全模块开发（依赖T4）
    [串行] T7: 集成测试（依赖T5,T6）

Step 6: 助手识别并行任务 → 分发给员工D/E/F
─────────────────────────────────────────────────────────
助手分析: T1、T2、T3 可并行执行

助手调用: 员工D
  任务: "T1: 数据库设计，包含用户表、角色表、权限表"

助手调用: 员工E
  任务: "T2: API 框架搭建，使用 Rust + Actix-web"

助手调用: 员工F
  任务: "T3: 前端项目初始化，使用 React + TypeScript"

员工D/E/F 返回: 各自完成

Step 7: 助手安排监工审核
─────────────────────────────────────────────────────────
助手调用: 监工A
  任务: "审核 T1、T2、T3 的完成情况"

监工A 返回:
  T1: ✓ 通过
  T2: ✓ 通过
  T3: ✗ 问题 - 缺少 ESLint 配置

Step 8: 助手处理审核结果
─────────────────────────────────────────────────────────
助手判断: T3 有问题，安排员工F修正

助手调用: 员工F
  任务: "T3 审核发现问题：缺少 ESLint 配置，请补充"

员工F 返回: 已补充 ESLint 配置

助手再次调用监工A: 审核修正后的 T3
监工A 返回: ✓ 通过

Step 9: 助手继续下一阶段任务
─────────────────────────────────────────────────────────
助手分析: T4 依赖已满足，可以开始

助手调用: 员工D
  任务: "T4: 用户模块开发..."

... (继续循环直到所有任务完成)
```

### 2.4 角色协作流程图

---

## 3. 目录结构

> **更新 (2026-03-30)**：已删除 10 个工具目录，仅保留 `tools/execute_command/`

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
├── channel/              # 复用 MoonClaw - 渠道抽象
│   ├── types.mbt         # Channel trait 定义
│   ├── extension.mbt     # ChannelExtension trait
│   └── extension_registry.mbt # 扩展注册
│
├── channels/             # 复用 MoonClaw - 渠道实现
│   ├── feishu/           # 飞书渠道实现
│   └── weixin/           # 微信渠道实现
│
├── tools/                # 【精简】仅保留 CLI 执行
│   ├── execute_command/  # Bash CLI 执行工具（唯一保留）
│   └── README.mbt.md     # 工具文档
│
├── tool/                 # 工具抽象层（保留最小抽象）
│   ├── tool.mbt          # Tool trait 定义
│   └── jsonschema.mbt    # JSON Schema
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

### 已删除的工具目录

```
tools/
├── read_file/            # ❌ 已删除
├── read_multiple_files/  # ❌ 已删除
├── write_to_file/        # ❌ 已删除
├── replace_in_file/      # ❌ 已删除
├── apply_patch/          # ❌ 已删除
├── list_files/           # ❌ 已删除
├── search_files/         # ❌ 已删除
├── todo/                 # ❌ 已删除
├── list_jobs/            # ❌ 已删除
└── wait_job/             # ❌ 已删除
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
| Vue | ^3.5.x | UI 框架 |
| Pinia | ^3.0.x | 状态管理 |
| @pinia/colada | ^0.12.x | 数据获取与缓存 |
| Vue Router | ^4.5.x | 路由管理 |
| Tailwind CSS | ^4.1.x | 样式系统 |
| Shiki | ^3.15.0 | 代码高亮 |
| Monaco Editor | ^0.55.1 | 代码编辑 |
| Vite | ^7.2.x | 构建工具 |
| @vueuse/core | ^13.0.x | Composition API 工具集 |

### 6.2 目录结构

```
ui/
├── clawteam-web/                   # 【新增】Vue 3 前端应用
│   ├── package.json
│   ├── vite.config.ts
│   ├── tailwind.config.ts
│   ├── tsconfig.json
│   ├── index.html
│   └── src/
│       ├── main.ts                 # 应用入口
│       ├── App.vue                 # 根组件
│       ├── router/
│       │   └── index.ts            # 路由配置
│       ├── stores/                 # Pinia 状态管理
│       │   ├── index.ts            # Store 入口
│       │   ├── useChatStore.ts     # 聊天状态
│       │   ├── useAgentStore.ts    # Agent 管理
│       │   ├── useExpertStore.ts   # 专家管理
│       │   ├── useSkillStore.ts    # 技能商店
│       │   ├── useSettingsStore.ts # 设置状态
│       │   └── useToastStore.ts    # Toast 通知
│       ├── composables/            # Composition API
│       │   ├── useChat.ts          # 聊天逻辑
│       │   ├── useAgent.ts         # Agent 操作
│       │   ├── useExpert.ts        # 专家操作
│       │   ├── useToast.ts         # Toast 通知
│       │   └── useWebSocket.ts     # WebSocket 连接
│       ├── components/
│       │   ├── layout/
│       │   │   ├── AppLayout.vue       # 主布局
│       │   │   ├── GlobalNav.vue       # 左侧全局导航
│       │   │   ├── ContextSidebar.vue  # 上下文侧边栏
│       │   │   └── TopBar.vue          # 顶部栏
│       │   ├── chat/
│       │   │   ├── ChatView.vue        # 聊天视图
│       │   │   ├── MessageList.vue     # 消息列表
│       │   │   ├── MessageBubble.vue   # 消息气泡
│       │   │   ├── ChatInput.vue       # 输入框
│       │   │   └── FileStatus.vue      # 文件状态指示
│       │   ├── agent/
│       │   │   ├── AgentView.vue       # 员工管理视图
│       │   │   ├── AgentCard.vue       # Agent 卡片
│       │   │   ├── AgentEditModal.vue  # Agent 编辑弹窗
│       │   │   ├── ExpertPanel.vue     # 专家中心面板
│       │   │   ├── ExpertCard.vue      # 专家卡片 (白底)
│       │   │   ├── ExpertModal.vue     # 添加专家弹窗
│       │   │   └── CustomExpertCard.vue # 自定义专家卡片
│       │   ├── skill/
│       │   │   ├── SkillView.vue       # 技能商店视图
│       │   │   ├── SkillCard.vue       # 技能卡片
│       │   │   └── SkillFilters.vue    # 技能筛选
│       │   ├── settings/
│       │   │   ├── SettingsView.vue    # 设置视图
│       │   │   ├── AccountSettings.vue # 账号设置
│       │   │   ├── AppearanceSettings.vue # 外观设置
│       │   │   ├── CliSettings.vue     # CLI 配置
│       │   │   ├── CliCard.vue         # CLI 工具卡片
│       │   │   └── ShellCard.vue       # 终端卡片
│       │   └── common/
│       │       ├── Toast.vue           # Toast 通知
│       │       ├── Modal.vue           # 通用弹窗
│       │       ├── Button.vue          # 按钮组件
│       │       └── Pagination.vue      # 分页组件
│       ├── types/
│       │   ├── agent.ts            # Agent 类型
│       │   ├── expert.ts           # 专家类型
│       │   ├── skill.ts            # 技能类型
│       │   ├── message.ts          # 消息类型
│       │   ├── cli.ts              # CLI 配置类型
│       │   └── settings.ts         # 设置类型
│       ├── api/
│       │   ├── client.ts           # API 客户端
│       │   ├── chat.ts             # 聊天 API
│       │   ├── agent.ts            # Agent API
│       │   └── skill.ts            # 技能 API
│       └── assets/
│           └── styles/
│               └── main.css        # 全局样式
│
└── rabbita-job/                    # 复用 MoonClaw Job UI
```

### 6.3 UI 布局

基于原型设计，采用三栏布局：全局导航 | 上下文侧边栏 | 主内容区

```
┌────────────────────────────────────────────────────────────────────────────┐
│                          ClawTeam Operator UI                              │
├────────┬─────────────────┬────────────────────────────────────────────────┤
│ 全局   │ 上下文侧边栏    │              主内容区                           │
│ 导航   │ (Context Nav)   │                                                │
│ 64px   │ 256px           │                                                │
├────────┼─────────────────┼────────────────────────────────────────────────┤
│        │                 │                                                │
│ ┌────┐ │ [频道]          │  顶部栏 (TopBar)                               │
│ │头像│ │ ┌─────────────┐ │  ┌────────────────────────────────────────────┐│
│ └────┘ │ │ docs 频道   │ │  │ docs                    [日期]             ││
│ ────   │ └─────────────┘ │  └────────────────────────────────────────────┘│
│        │                 │                                                │
│ [聊天] │ [会话记录]      │  ┌────────────────────────────────────────────┐│
│ [员工] │  ┌──────────┐   │  │                                            ││
│ [商店] │  │ 昨天     │   │  │  聊天消息区域                              ││
│ [文件] │  │ 上周     │   │  │                                            ││
│        │  └──────────┘   │  │  [用户] hi                                 ││
│  ────  │                 │  │                                            ││
│ [帮助] │                 │  │  [WorkBuddy] 我来看看工作区...              ││
│ [设置] │                 │  │    📄 已读取 BOOTSTRAP.md                  ││
│        │                 │  │    📄 已读取 IDENTITY.md                   ││
└────────┴─────────────────┴──┴────────────────────────────────────────────┘│
                            │                                                │
                            │  ┌────────────────────────────────────────────┐│
                            │  │ [@] [📎]                                   ││
                            │  │ [输入消息...                    ] [发送]   ││
                            │  └────────────────────────────────────────────┘│
                            └────────────────────────────────────────────────┘
```

### 6.4 视图设计

#### 6.4.1 聊天视图 (ChatView)

```vue
<!-- ChatView.vue - 聊天主界面 -->
<template>
  <div class="flex flex-col h-full">
    <!-- 消息列表 -->
    <MessageList :messages="messages" class="flex-1 overflow-y-auto" />
    
    <!-- 输入区域 -->
    <div class="p-6 pt-0">
      <ChatInput 
        v-model="inputText"
        :loading="isSending"
        @send="handleSend"
      />
    </div>
  </div>
</template>
```

#### 6.4.2 员工管理视图 (AgentView)

```vue
<!-- AgentView.vue - 员工管理/专家中心 -->
<template>
  <div class="flex flex-col h-full">
    <!-- 顶部工具栏 -->
    <div class="staff-topbar">
      <h2>员工管理</h2>
      <div class="right">
        <div class="sort-group">排序 <SortBadge /></div>
        <button @click="showAddAgent">添加员工</button>
      </div>
    </div>
    
    <!-- 三列布局：助手 | 监工 | 自定义专家 -->
    <div class="staff-cols">
      <!-- 助手列 -->
      <div class="staff-col">
        <StaffColTitle label="助手" dot-color="#3b82f6" :count="assistants.length" />
        <AgentCard v-for="a in assistants" :key="a.id" :agent="a" @edit="editAgent" />
        <EmptyCol v-if="!assistants.length" message="暂无助手" />
      </div>
      
      <!-- 监工列 -->
      <div class="staff-col">
        <StaffColTitle label="监工" dot-color="#f97316" :count="supervisors.length" />
        <AgentCard v-for="s in supervisors" :key="s.id" :agent="s" @edit="editAgent" />
        <EmptyCol v-if="!supervisors.length" message="暂无监工" />
      </div>
      
      <!-- 自定义专家列 -->
      <div class="staff-col">
        <StaffColTitle label="自定义专家" />
        <CustomExpertCard @click="showAddExpert" />
      </div>
    </div>
    
    <!-- 专家中心面板 -->
    <ExpertPanel 
      :experts="filteredExperts"
      :categories="categories"
      @add="addExpert"
    />
  </div>
</template>
```

#### 6.4.3 技能商店视图 (SkillView)

```vue
<!-- SkillView.vue - 技能商店 -->
<template>
  <div class="p-6">
    <!-- 搜索和筛选 -->
    <div class="flex items-center gap-3 mb-4">
      <SearchBar v-model="searchQuery" placeholder="搜索技能..." />
      <SkillTabs v-model="activeTab" :tabs="['商店', '我的技能', '项目技能']" />
    </div>
    
    <!-- 筛选标签 -->
    <SkillFilters v-model="activeFilter" :filters="filters" />
    
    <!-- 技能列表 -->
    <div class="skill-grid">
      <SkillCard 
        v-for="skill in filteredSkills" 
        :key="skill.id"
        :skill="skill"
        @install="installSkill"
      />
    </div>
    
    <!-- 分页 -->
    <Pagination :total="totalPages" v-model="currentPage" />
  </div>
</template>
```

#### 6.4.4 设置视图 (SettingsView)

```vue
<!-- SettingsView.vue - 设置 -->
<template>
  <div class="settings-layout">
    <!-- 设置导航 -->
    <nav class="settings-nav">
      <div class="section-title">用户设置</div>
      <SettingsNavItem icon="user" label="我的账号" to="account" />
      
      <div class="section-title">应用设置</div>
      <SettingsNavItem icon="palette" label="外观" to="appearance" />
      <SettingsNavItem icon="terminal" label="CLI配置" to="cli" />
    </nav>
    
    <!-- 设置内容 -->
    <div class="settings-body">
      <AccountSettings v-if="activeTab === 'account'" />
      <AppearanceSettings v-if="activeTab === 'appearance'" />
      <CliSettings v-if="activeTab === 'cli'" />
    </div>
  </div>
</template>
```

### 6.5 Pinia Store 设计

#### 6.5.1 Agent Store

```typescript
// stores/useAgentStore.ts
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { Agent, AgentRole } from '@/types/agent'

export const useAgentStore = defineStore('agent', () => {
  // State
  const agents = ref<Agent[]>([
    { id: 1, name: '助手', type: 'assistant', desc: 'WorkBuddy', prompt: '...' }
  ])
  
  // Getters
  const assistants = computed(() => 
    agents.value.filter(a => a.type === 'assistant')
  )
  
  const supervisors = computed(() => 
    agents.value.filter(a => a.type === 'supervisor')
  )
  
  const members = computed(() => 
    agents.value.filter(a => a.type === 'member')
  )
  
  // Actions
  function addAgent(agent: Omit<Agent, 'id'>) {
    const id = Math.max(...agents.value.map(a => a.id), 0) + 1
    agents.value.push({ ...agent, id })
  }
  
  function updateAgent(id: number, updates: Partial<Agent>) {
    const index = agents.value.findIndex(a => a.id === id)
    if (index !== -1) {
      agents.value[index] = { ...agents.value[index], ...updates }
    }
  }
  
  function removeAgent(id: number) {
    agents.value = agents.value.filter(a => a.id !== id)
  }
  
  return {
    agents,
    assistants,
    supervisors,
    members,
    addAgent,
    updateAgent,
    removeAgent
  }
})
```

#### 6.5.2 Expert Store

```typescript
// stores/useExpertStore.ts
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { Expert } from '@/types/expert'

export const useExpertStore = defineStore('expert', () => {
  // State
  const experts = ref<Expert[]>([
    { id: 1, name: 'Kai', role: '内容创作专家', category: '内容创作', 
      desc: '...', tags: ['文案', '品牌'], added: true },
    // ...
  ])
  
  const activeCategory = ref('全部')
  const searchQuery = ref('')
  
  // Getters
  const categories = computed(() => {
    const cats = new Set(experts.value.map(e => e.category))
    return ['全部', ...cats]
  })
  
  const filteredExperts = computed(() => {
    return experts.value.filter(e => {
      const matchCategory = activeCategory.value === '全部' || 
        e.category === activeCategory.value
      const q = searchQuery.value.toLowerCase()
      const matchSearch = !q || 
        e.name.toLowerCase().includes(q) ||
        e.role.toLowerCase().includes(q)
      return matchCategory && matchSearch
    })
  })
  
  const addedExperts = computed(() => 
    experts.value.filter(e => e.added)
  )
  
  // Actions
  function addExpertToProject(id: number) {
    const expert = experts.value.find(e => e.id === id)
    if (expert) expert.added = true
  }
  
  function createExpert(expert: Omit<Expert, 'id' | 'added'>) {
    const id = Math.max(...experts.value.map(e => e.id), 0) + 1
    experts.value.unshift({ ...expert, id, added: true })
  }
  
  return {
    experts,
    activeCategory,
    searchQuery,
    categories,
    filteredExperts,
    addedExperts,
    addExpertToProject,
    createExpert
  }
})
```

#### 6.5.3 Chat Store

```typescript
// stores/useChatStore.ts
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { Message, FileStatus } from '@/types/message'

export const useChatStore = defineStore('chat', () => {
  // State
  const messages = ref<Message[]>([])
  const inputText = ref('')
  const isSending = ref(false)
  const currentChannel = ref<string | null>(null)
  
  // Actions
  async function sendMessage(content: string) {
    if (!content.trim() || isSending.value) return
    
    // 添加用户消息
    messages.value.push({
      id: Date.now(),
      role: 'user',
      content,
      timestamp: Date.now()
    })
    
    isSending.value = true
    inputText.value = ''
    
    try {
      // 调用 API 发送消息
      const response = await chatApi.sendMessage({
        channel: currentChannel.value,
        message: content
      })
      
      // 添加助手响应
      messages.value.push({
        id: Date.now() + 1,
        role: 'assistant',
        content: response.content,
        fileStatuses: response.fileStatuses,
        timestamp: Date.now()
      })
    } finally {
      isSending.value = false
    }
  }
  
  function clearMessages() {
    messages.value = []
  }
  
  return {
    messages,
    inputText,
    isSending,
    currentChannel,
    sendMessage,
    clearMessages
  }
})
```

#### 6.5.4 Settings Store

```typescript
// stores/useSettingsStore.ts
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { CliConfig, ShellConfig } from '@/types/settings'

export const useSettingsStore = defineStore('settings', () => {
  // State
  const theme = ref<'dark' | 'light' | 'system'>('dark')
  const selectedCli = ref<string>('iflow')
  const selectedShell = ref<string>('cmd')
  
  const cliTools = ref<CliConfig[]>([
    { id: 'gemini', name: 'Gemini CLI', command: 'gemini', installed: false },
    { id: 'codex', name: 'Codex', command: 'codex', installed: false },
    { id: 'claude', name: 'Claude Code', command: 'claude', installed: true },
    { id: 'opencode', name: 'opencode', command: 'opencode', installed: false },
    { id: 'qwen', name: 'Qwen Code', command: 'qwen', installed: false },
    { id: 'openclaw', name: 'OpenClaw', command: 'openclaw', installed: false },
    { id: 'iflow', name: 'iflow', command: 'iflow -p', installed: true, custom: true }
  ])
  
  const shells = ref<ShellConfig[]>([
    { id: 'default', name: '系统默认', path: '使用系统默认 shell' },
    { id: 'powershell', name: 'Windows PowerShell', path: 'C:\\Windows\\System32\\...' },
    { id: 'cmd', name: 'Command Prompt', path: 'C:\\Windows\\system32\\cmd.exe' }
  ])
  
  const pathPool = ref<string[]>([])
  
  // Actions
  function setTheme(newTheme: typeof theme.value) {
    theme.value = newTheme
    document.documentElement.classList.toggle('dark', newTheme === 'dark')
  }
  
  function selectCli(id: string) {
    selectedCli.value = id
  }
  
  function selectShell(id: string) {
    selectedShell.value = id
  }
  
  function addPath(path: string) {
    if (path && !pathPool.value.includes(path)) {
      pathPool.value.push(path)
    }
  }
  
  function removePath(path: string) {
    pathPool.value = pathPool.value.filter(p => p !== path)
  }
  
  return {
    theme,
    selectedCli,
    selectedShell,
    cliTools,
    shells,
    pathPool,
    setTheme,
    selectCli,
    selectShell,
    addPath,
    removePath
  }
})
```

#### 6.5.5 Toast Store

```typescript
// stores/useToastStore.ts
import { defineStore } from 'pinia'
import { ref } from 'vue'

export type ToastType = 'success' | 'error' | 'info'

export interface Toast {
  id: number
  type: ToastType
  message: string
}

export const useToastStore = defineStore('toast', () => {
  const toasts = ref<Toast[]>([])
  let nextId = 1
  
  function show(type: ToastType, message: string, duration = 2500) {
    const id = nextId++
    toasts.value.push({ id, type, message })
    
    setTimeout(() => {
      remove(id)
    }, duration)
  }
  
  function remove(id: number) {
    toasts.value = toasts.value.filter(t => t.id !== id)
  }
  
  function success(message: string) { show('success', message) }
  function error(message: string) { show('error', message) }
  function info(message: string) { show('info', message) }
  
  return { toasts, show, remove, success, error, info }
})
```

### 6.6 组件设计

#### 6.6.1 AgentCard 组件

```vue
<!-- components/agent/AgentCard.vue -->
<template>
  <div class="agent-card group" @click="$emit('edit', agent)">
    <div class="agent-card-left">
      <div 
        class="w-10 h-10 rounded-full flex items-center justify-center text-white font-bold"
        :style="{ background: avatarGradient }"
      >
        {{ agent.name.charAt(0) }}
        <div v-if="agent.online" class="online-indicator" />
      </div>
      <div>
        <div class="agent-card-name">{{ agent.name }}</div>
        <div class="agent-card-desc">{{ agent.desc || '已激活' }}</div>
      </div>
    </div>
    <div class="agent-card-actions">
      <button title="对话" @click.stop="$emit('chat', agent)">
        <i class="far fa-comment-dots" />
      </button>
      <button title="提示词设置" @click.stop="$emit('edit', agent)">
        <i class="fa-solid fa-sliders-h" />
      </button>
      <button class="del" title="移除" @click.stop="$emit('remove', agent)">
        <i class="far fa-trash-alt" />
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { Agent } from '@/types/agent'

const props = defineProps<{ agent: Agent }>()
defineEmits<{
  edit: [agent: Agent]
  chat: [agent: Agent]
  remove: [agent: Agent]
}>()

const avatarGradient = computed(() => {
  const gradients = {
    assistant: 'linear-gradient(135deg, #d97706, #92400e)',
    supervisor: 'linear-gradient(135deg, #6b7280, #374151)',
    member: 'linear-gradient(135deg, #10b981, #047857)'
  }
  return gradients[props.agent.type] || gradients.member
})
</script>
```

#### 6.6.2 ExpertCard 组件 (白底设计)

```vue
<!-- components/agent/ExpertCard.vue -->
<template>
  <div class="expert-card" @click="$emit('add', expert)">
    <img 
      :src="expert.avatar || defaultAvatar" 
      :alt="expert.name"
      class="expert-avatar"
    >
    <div class="expert-card-name">{{ expert.name }}</div>
    <span class="expert-tag">{{ expert.role }}</span>
    <p class="expert-card-desc">{{ expert.desc }}</p>
    <button class="btn-add-expert">
      {{ expert.added ? '已添加' : '添加专家' }}
    </button>
  </div>
</template>

<script setup lang="ts">
import type { Expert } from '@/types/expert'

defineProps<{ expert: Expert }>()
defineEmits<{ add: [expert: Expert] }>()

const defaultAvatar = 'https://picsum.photos/112/112.jpg'
</script>

<style scoped>
.expert-card {
  background: #fff;
  color: #1f2937;
  border-radius: 12px;
  padding: 20px 16px 16px;
  display: flex;
  flex-direction: column;
  align-items: center;
  border: 1px solid #e5e7eb;
  transition: all 0.2s;
  cursor: pointer;
}

.expert-card:hover {
  transform: translateY(-3px);
  box-shadow: 0 12px 24px rgba(0, 0, 0, 0.12);
  border-color: #8b5cf6;
}

.expert-avatar {
  width: 56px;
  height: 56px;
  border-radius: 50%;
  object-fit: cover;
  border: 3px solid #f3f4f6;
  margin-bottom: 10px;
}

.expert-tag {
  background: #ede9fe;
  color: #6d28d9;
  padding: 3px 12px;
  border-radius: 20px;
  font-size: 11.5px;
  font-weight: 600;
  margin-bottom: 8px;
}

.btn-add-expert {
  width: 100%;
  padding: 8px;
  border-radius: 8px;
  font-size: 12.5px;
  font-weight: 500;
  background: #f3f4f6;
  color: #4b5563;
  border: 1px solid #e5e7eb;
  cursor: pointer;
  transition: all 0.15s;
}

.btn-add-expert:hover {
  background: #8b5cf6;
  color: #fff;
  border-color: #8b5cf6;
}
</style>
```

#### 6.6.3 CliCard 组件

```vue
<!-- components/settings/CliCard.vue -->
<template>
  <div 
    class="cli-card" 
    :class="{ active: selected, custom: cli.custom }"
    @click="$emit('select', cli.id)"
  >
    <i v-if="selected" class="fa-solid fa-circle-check check-icon" />
    <button class="menu-btn" @click.stop="$emit('menu', cli)">
      <i class="fa-solid fa-ellipsis-vertical" />
    </button>
    
    <div class="cli-icon" :class="iconClass">
      <i :class="cli.icon || 'fa-solid fa-terminal'" />
    </div>
    
    <div class="cli-name">{{ cli.name }}</div>
    <div class="cli-cmd">{{ cli.command }}</div>
    <div class="cli-type">{{ cli.custom ? '自定义终端' : '默认终端' }}</div>
    <div v-if="!cli.custom" class="cli-status" :class="cli.installed ? 'installed' : 'not-installed'">
      {{ cli.installed ? '已安装' : '未安装' }}
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { CliConfig } from '@/types/settings'

const props = defineProps<{ 
  cli: CliConfig
  selected: boolean 
}>()

defineEmits<{
  select: [id: string]
  menu: [cli: CliConfig]
}>()

const iconClass = computed(() => {
  if (props.selected) return 'selected'
  if (props.cli.installed) return 'installed'
  return 'default'
})
</script>
```

#### 6.6.4 Toast 组件

```vue
<!-- components/common/Toast.vue -->
<template>
  <Teleport to="body">
    <div class="toast-container">
      <TransitionGroup name="toast">
        <div 
          v-for="toast in toasts" 
          :key="toast.id"
          class="toast"
          :class="toast.type"
        >
          <i :class="iconClass(toast.type)" />
          <span>{{ toast.message }}</span>
        </div>
      </TransitionGroup>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useToastStore, type ToastType } from '@/stores/useToastStore'

const toastStore = useToastStore()
const { toasts } = storeToRefs(toastStore)

function iconClass(type: ToastType) {
  return {
    success: 'fa-solid fa-circle-check',
    error: 'fa-solid fa-circle-xmark',
    info: 'fa-solid fa-circle-info'
  }[type]
}
</script>

<style scoped>
.toast-container {
  position: fixed;
  top: 20px;
  right: 20px;
  z-index: 1000;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.toast {
  background: #1f2937;
  border: 1px solid #374151;
  border-radius: 10px;
  padding: 12px 18px;
  font-size: 13px;
  color: #e5e7eb;
  box-shadow: 0 8px 30px rgba(0, 0, 0, 0.4);
  display: flex;
  align-items: center;
  gap: 10px;
}

.toast.success i { color: #10b981; }
.toast.error i { color: #ef4444; }
.toast.info i { color: #0ea5e9; }

.toast-enter-active { animation: toastIn 0.3s ease; }
.toast-leave-active { animation: toastOut 0.3s ease; }

@keyframes toastIn {
  from { opacity: 0; transform: translateX(30px); }
  to { opacity: 1; transform: translateX(0); }
}

@keyframes toastOut {
  from { opacity: 1; transform: translateX(0); }
  to { opacity: 0; transform: translateX(30px); }
}
</style>
```

### 6.7 数据类型定义

```typescript
// types/agent.ts
export type AgentRole = 'assistant' | 'supervisor' | 'member'

export interface Agent {
  id: number
  name: string
  type: AgentRole
  desc?: string
  prompt: string
  online?: boolean
}

// types/expert.ts
export interface Expert {
  id: number
  name: string
  role: string
  category: string
  desc: string
  tags: string[]
  added: boolean
  avatar?: string
}

// types/skill.ts
export interface Skill {
  id: string
  name: string
  description: string
  icon: string
  color: string
  tags: string[]
  rating: number
  installed: boolean
}

// types/message.ts
export type MessageRole = 'user' | 'assistant'

export interface FileStatus {
  icon: string
  action: string
  path: string
  lines?: string
}

export interface Message {
  id: number
  role: MessageRole
  content: string
  fileStatuses?: FileStatus[]
  timestamp: number
}

// types/settings.ts
export interface CliConfig {
  id: string
  name: string
  command: string
  installed: boolean
  custom?: boolean
  icon?: string
}

export interface ShellConfig {
  id: string
  name: string
  path: string
}
```

### 6.8 路由配置

```typescript
// router/index.ts
import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      component: () => import('@/components/layout/AppLayout.vue'),
      children: [
        { path: '', redirect: '/chat' },
        { path: 'chat', name: 'chat', component: () => import('@/components/chat/ChatView.vue') },
        { path: 'agents', name: 'agents', component: () => import('@/components/agent/AgentView.vue') },
        { path: 'skills', name: 'skills', component: () => import('@/components/skill/SkillView.vue') },
        { 
          path: 'settings', 
          name: 'settings', 
          component: () => import('@/components/settings/SettingsView.vue'),
          children: [
            { path: '', redirect: 'account' },
            { path: 'account', name: 'settings-account', component: () => import('@/components/settings/AccountSettings.vue') },
            { path: 'appearance', name: 'settings-appearance', component: () => import('@/components/settings/AppearanceSettings.vue') },
            { path: 'cli', name: 'settings-cli', component: () => import('@/components/settings/CliSettings.vue') }
          ]
        }
      ]
    }
  ]
})

export default router
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

> **重要更新 (2026-03-30)**：经过简化重构，ClawTeam 现在是一个**纯调度层工具**，仅保留 Bash CLI 调用和渠道通讯功能。所有文件操作、任务管理等工具已删除，相关业务逻辑由外部 CLI 工具负责。

### 8.1 从 MoonClaw 迁移的功能

#### 保留的工具（仅 1 个）

| 模块 | 来源 | 说明 |
|------|------|------|
| `tools/execute_command/` | MoonClaw | Bash CLI 执行工具 - 核心调度能力 |

#### 保留的渠道通讯

| 模块 | 来源 | 说明 |
|------|------|------|
| `channel/` | MoonClaw | 渠道抽象层 - Channel trait、ChannelExtension、注册管理 |
| `channels/feishu/` | MoonClaw | 飞书渠道实现 - 消息收发、WebSocket、表情反应 |
| `channels/weixin/` | MoonClaw | 微信公众号渠道实现 - 消息收发、签名验证 |

#### 完全复用的基础模块

| 模块 | 来源 | 说明 |
|------|------|------|
| `gateway/` | MoonClaw | HTTP/RPC 服务完整复用 |
| `job/` | MoonClaw | 任务调度与工作流完整复用 |
| `event/` | MoonClaw | 事件系统完整复用 |
| `clock/` | MoonClaw | 时间工具完整复用 |
| `internal/` | MoonClaw | 工具库完整复用（httpx、spawn、pino、fsx、broadcast、uuid 等） |
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

> **重要更新 (2026-03-30)**：已完成工具系统简化，删除了所有文件操作和任务管理工具。

### 9.1 已删除的工具（2026-03-30）

| 工具 | 原路径 | 说明 |
|------|--------|------|
| `read_file` | `tools/read_file/` | 文件读取 - 由外部 CLI 负责 |
| `read_multiple_files` | `tools/read_multiple_files/` | 批量文件读取 - 由外部 CLI 负责 |
| `write_to_file` | `tools/write_to_file/` | 文件写入 - 由外部 CLI 负责 |
| `replace_in_file` | `tools/replace_in_file/` | 文件搜索替换 - 由外部 CLI 负责 |
| `apply_patch` | `tools/apply_patch/` | 补丁应用 - 由外部 CLI 负责 |
| `list_files` | `tools/list_files/` | 目录列表 - 由外部 CLI 负责 |
| `search_files` | `tools/search_files/` | 文件搜索 - 由外部 CLI 负责 |
| `todo` | `tools/todo/` | 任务管理 - 由外部 CLI 负责 |
| `list_jobs` | `tools/list_jobs/` | 后台任务列表 - 由外部 CLI 负责 |
| `wait_job` | `tools/wait_job/` | 后台任务等待 - 由外部 CLI 负责 |

### 9.2 保留的工具

| 工具 | 原路径 | 说明 |
|------|--------|------|
| `execute_command` | `tools/execute_command/` | Bash CLI 执行 - 核心调度能力 |

### 9.3 其他删除/废弃的模块

| 模块 | 原因 |
|------|------|
| `agent/` | Agent 运行时 - 业务逻辑由外部 CLI 负责 |
| `tool/` | 工具抽象 - 仅保留 execute_command 所需的最小抽象 |
| `workspace/` | 工作空间管理 - 业务逻辑由外部 CLI 负责 |
| `security/` | 安全运行时 - 业务逻辑由外部 CLI 负责 |
| `file/` | 文件管理器 - 业务逻辑由外部 CLI 负责 |
| `prompt/` | 系统提示词 - 不需要 |
| `plugin/` | 插件运行时 - 暂不需要 |
| `skills/` | 技能加载器 - 用新 Skill 系统替代 |
| `onboarding/` | 用户引导 - 标记废弃 |

### 9.4 设计原则

ClawTeam 作为纯调度层，遵循以下原则：

1. **只调度，不执行** - 所有业务逻辑由外部 CLI 工具负责
2. **只通讯，不处理** - 渠道层仅负责消息路由，不处理业务
3. **只记录，不决策** - 审计日志记录执行过程，决策由外部 CLI 负责

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
Step 1: 助手接收用户输入（包含所有可用技能）
─────────────────────────────────────────────────────────
终端调用: opencode -p "[助手 agent-prompt], [用户输入]"

CLI 命令:
  opencode 
  -p "你是一个助手角色，负责：
      1. 理解用户需求
      2. 分析任务复杂度，决定使用哪些技能
      3. 分配任务给合适的员工
      4. 汇总员工和监工的反馈
      
      当前可用员工: [员工A, 员工B]
      当前可用监工: [监工A]
      派发策略: 能力匹配
      
      当前可用技能: [generate-api, generate-tests, code-review, refactoring]
      
      用户输入: 生成用户管理模块的 CRUD 接口"
      
助手分析结果:
  - 意图: 代码生成 + 测试
  - 决定使用的技能: generate-api, generate-tests
  - 需要审核: 是
  - 分配任务:
    - 员工A: 生成 CRUD 接口代码
    - 员工B: 生成单元测试
    - 监工A: 审核代码和测试


Step 2: 助手派发任务给员工A（不含 skill 参数，任务已明确）
─────────────────────────────────────────────────────────
终端调用: opencode -p "[员工A agent-prompt], [助手分配的任务]"

CLI 命令:
  opencode 
  -p "你是一个员工角色，负责执行助手分配的任务。
      
      你的能力: [code-generation, refactoring, testing]
      
      助手分配的任务:
      为用户管理模块生成 CRUD 接口，包含以下端点：
        - GET /users - 获取用户列表
        - POST /users - 创建用户
        - PUT /users/:id - 更新用户
        - DELETE /users/:id - 删除用户
      
      输出要求：生成 Rust 代码，包含 API 处理函数和数据结构。"
      
员工A 执行结果:
  - 生成文件: user_api.rs, user_service.rs
  - 状态: 完成


Step 3: 助手派发任务给员工B
─────────────────────────────────────────────────────────
终端调用: opencode -p "[员工B agent-prompt], [助手分配的任务]"

CLI 命令:
  opencode 
  -p "你是一个员工角色，负责执行助手分配的任务。
      
      你的能力: [testing, debugging]
      
      助手分配的任务:
      为用户管理模块的 CRUD 接口生成单元测试，覆盖以下场景：
        - 正常的 CRUD 操作
        - 参数校验失败
        - 权限检查
      
      输出要求：生成 Rust 测试代码。"
      
员工B 执行结果:
  - 生成文件: user_test.rs
  - 状态: 完成


Step 4: 助手派发任务给监工A
─────────────────────────────────────────────────────────
终端调用: gemini -p "[监工A agent-prompt], [审核任务]"

CLI 命令:
  gemini 
  -p "你是一个监工角色，负责审核员工的工作成果。
      
      审核标准: [code-style, type-safety, test-coverage]
      
      审核对象:
      - 员工A 生成的用户管理 CRUD 接口 (user_api.rs, user_service.rs)
      - 员工B 生成的单元测试 (user_test.rs)"
      
监工A 审核结果:
  - 代码风格: ✓ 通过
  - 类型安全: ✓ 通过
  - 测试覆盖: ✓ 通过（覆盖率 85%）
  - 结论: 通过


Step 5: 助手汇总结果反馈用户
─────────────────────────────────────────────────────────
助手输出:
  "已完成用户管理模块的 CRUD 接口生成：
   - API 接口: user_api.rs, user_service.rs（员工A）
   - 单元测试: user_test.rs（员工B）
   - 代码审核: 通过（监工A）
   - 测试覆盖率: 85%"
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

/// 助手提示词模板（协调器，不思考）
let ASSISTANT_PROMPT_TEMPLATE = """
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
let WORKER_PROMPT_TEMPLATE = """
你是一个员工角色（执行者），负责：
1. 执行助手分配的任务
2. 进行思考、分析、规划、编码等工作
3. 向助手反馈执行结果

你的能力: {{capabilities}}

助手分配的任务: {{assigned_task}}

请认真思考并执行任务，返回详细结果。
"""

/// 监工提示词模板
let SUPERVISOR_PROMPT_TEMPLATE = """
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
```

### 12.3 CLI 调用封装

```moonbit
/// CLI 调用参数
pub struct CliCallParams {
  cli_tool : String              // 工具名称，如 "opencode"
  agent_prompt : String          // Agent 角色提示词
  context : String               // 上下文（用户输入/任务/审核对象）
  variables : Map[String, String] // 模板变量
}

/// 构建 CLI 命令
pub fn build_cli_command(
  tool_config : CliToolConfig,
  params : CliCallParams,
) -> Array[String] {
  let prompt = render_prompt(params.agent_prompt, params.variables)
  let full_prompt = "\{prompt}\n\n\{params.context}"
  
  let mut args = tool_config.args.to_array()
  args.push("-p")
  args.push(full_prompt)
  args
}

/// 执行助手 CLI 调用（包含所有技能）
pub async fn execute_assistant_call(
  agent : AgentConfig,
  cli_registry : CliToolRegistry,
  skills : Array[Skill],
  user_input : String,
) -> Result[CliCallResult, ClawTeamError] {
  let tool_config = cli_registry.tools.get(agent.cli_tool)?
  let prompt_template = ASSISTANT_PROMPT_TEMPLATE
  
  // 构建变量
  let skill_names = skills.map(fn(s) { s.name }).join(", ")
  let variables = {
    "available_workers": agent.role_config.available_workers.join(", "),
    "available_supervisors": agent.role_config.available_supervisors.join(", "),
    "dispatch_strategy": agent.role_config.dispatch_strategy.to_string(),
    "skills": skill_names,
  }
  
  let agent_prompt = render_prompt(prompt_template, variables)
  let params = CliCallParams::{
    cli_tool: agent.cli_tool,
    agent_prompt,
    context: user_input,
    variables: {},
  }
  
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

/// 执行员工/监工 CLI 调用（不含 skill，由助手决定任务）
pub async fn execute_agent_call(
  agent : AgentConfig,
  cli_registry : CliToolRegistry,
  assigned_task : String,
) -> Result[CliCallResult, ClawTeamError] {
  let tool_config = cli_registry.tools.get(agent.cli_tool)?
  let prompt_template = match agent.role {
    Worker => WORKER_PROMPT_TEMPLATE
    Supervisor => SUPERVISOR_PROMPT_TEMPLATE
    _ => return Err(ClawTeamError::InvalidRole)
  }
  
  // 构建变量
  let variables = match agent.role {
    Worker => {
      "capabilities": agent.role_config.capabilities.join(", "),
    }
    Supervisor => {
      "review_criteria": agent.role_config.review_criteria.join(", "),
    }
  }
  
  let agent_prompt = render_prompt(prompt_template, variables)
  let params = CliCallParams::{
    cli_tool: agent.cli_tool,
    agent_prompt,
    context: assigned_task,
    variables: {},
  }
  
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
| 1.0.3 | 2026-03-26 | 调整调用链设计：助手 prompt 包含所有技能，员工 prompt 不含 skill |
| 1.0.4 | 2026-03-26 | 重新定义角色职责：助手=协调器不思考，员工=执行者负责思考 |
| 1.1.0 | 2026-03-30 | 后端简化：删除 10 个工具目录，仅保留 execute_command；保留渠道通讯（Feishu/Weixin）；明确纯调度层定位 |
| **1.2.0** | **2026-03-30** | **前端重构：技术栈从 React 19 + Redux Toolkit 改为 Vue 3.5 + Pinia + @pinia/colada + Tailwind CSS；基于原型 UI.html 重新设计四视图布局（聊天、员工管理、技能商店、设置）；新增完整的 Pinia Store 设计和 Vue 组件设计** |
