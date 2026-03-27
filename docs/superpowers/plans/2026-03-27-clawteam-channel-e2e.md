# ClawTeam 渠道集成与端到端测试 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 完成 ClawTeam 的渠道集成（Web UI、飞书）、WebSocket 实时通信、Gateway RPC 接口扩展，以及端到端测试。

**架构：** 基于 MoonClaw channel/gateway 模块，扩展 RPC 接口支持 Agent 调度，实现 WebSocket 事件广播，编写 E2E 测试。

**技术栈：** MoonBit Native, Feishu API, WebSocket, Playwright (E2E)

---

## 前置依赖

- **计划 A** 已完成：后端核心调度层可用
- **计划 B** 已完成：前端 UI 扩展可用

---

## 文件结构

### 新建/修改文件

```
clawteam/
├── gateway/
│   └── server/
│       ├── gateway.mbt          # 修改：添加 Scheduler 集成
│       ├── scheduler_ext.mbt    # 新增：调度器扩展
│       └── ws_events.mbt        # 新增：WebSocket 事件广播
├── channels/
│   └── feishu/
│       └── handler.mbt          # 新增：ClawTeam 特定消息处理
├── cmd/
│   └── gateway/
│       └── main.mbt             # 修改：初始化调度器
├── tests/
│   ├── e2e/
│   │   ├── basic_flow.test.ts   # E2E 基础流程测试
│   │   └── playwright.config.ts
│   └── integration/
│       └── scheduler.test.mbt   # 调度器集成测试
└── docs/
    └── api/
        └── rpc.md               # RPC 接口文档
```

---

## 任务分解

### 任务 1：扩展 Gateway 集成 Scheduler

**文件：**
- 修改：`gateway/server/gateway.mbt`
- 创建：`gateway/server/scheduler_ext.mbt`

- [ ] **步骤 1：创建 scheduler_ext.mbt**

```moonbit
/// Scheduler 扩展（挂载到 Gateway）
pub struct SchedulerExtension {
  pub config : @config.ClawTeamConfig
  pub dispatcher : @scheduler.Dispatcher
  pub router : @scheduler.AgentRouter
  pub logger : @pino.Logger
  pub audit : @logger.AuditStore
}

pub fn SchedulerExtension::new(
  config : @config.ClawTeamConfig,
  logger : @pino.Logger,
) -> SchedulerExtension {
  SchedulerExtension::{
    config,
    dispatcher: @scheduler.Dispatcher::new(config, logger),
    router: @scheduler.AgentRouter::new(config, logger),
    logger,
    audit: @logger.AuditStore::new(config.audit, logger),
  }
}

/// 处理用户消息
pub async fn SchedulerExtension::handle_user_message(
  self : &SchedulerExtension,
  session_id : String,
  message : String,
  user_id : String?,
  channel : String?,
) -> Result[@scheduler.CliCallResult, ClawTeamError] {
  // 记录审计日志
  self.audit.log_output_captured(
    session_id,
    "user_message",
    message,
  )
  
  // 构建派发上下文
  let context = @scheduler.DispatchContext::{
    session_id,
    user_id,
    channel,
    timestamp: @clock.now_ms(),
    metadata: {},
  }
  
  // 派发到助手
  let result = self.dispatcher.dispatch_to_assistant(message, context)?
  
  // 记录结果
  self.audit.log_cli_completed(
    session_id,
    "assistant-1",
    result.status,
    result.elapsed,
  )
  
  Ok(result)
}

/// 处理 Skill 调用
pub async fn SchedulerExtension::handle_skill_call(
  self : &SchedulerExtension,
  session_id : String,
  skill_id : String,
  variables : Map[String, String],
  user_id : String?,
) -> Result[@scheduler.CliCallResult, ClawTeamError] {
  // 查找 Skill
  let mut skill : @config.Skill? = None
  for s in self.config.skills {
    if s.id == skill_id {
      skill = Some(s)
      break
    }
  }
  
  let skill = skill?
  
  // 渲染消息模板
  let template = @scheduler.MessageTemplate::new(skill.template.message_template)
  let template = template.with_variables(variables)
  let message = template.render()
  
  // 派发
  self.handle_user_message(session_id, message, user_id, None)
}
```

- [ ] **步骤 2：修改 gateway.mbt 添加 scheduler 字段**

在 Gateway 结构体中添加：

```moonbit
pub struct Gateway {
  // ... 现有字段 ...
  scheduler_ext : @scheduler_ext.SchedulerExtension?
}
```

- [ ] **步骤 3：在 Gateway 初始化中创建 Scheduler**

```moonbit
// 在 Gateway::new 中添加
let scheduler_ext = match @config.ConfigLoader::new(logger).load(home, cwd) {
  Ok(config) => Some(@scheduler_ext.SchedulerExtension::new(config, logger))
  Err(_) => None
}
```

- [ ] **步骤 4：运行 moon check**

运行：`moon check gateway/`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add gateway/server/scheduler_ext.mbt gateway/server/gateway.mbt
git commit -m "feat: integrate scheduler extension into gateway"
```

---

### 任务 2：扩展 RPC 接口

**文件：**
- 修改：`gateway/server/rpc.mbt`
- 创建：`docs/api/rpc.md`

- [ ] **步骤 1：添加新的 RPC 方法**

```moonbit
/// RPC: 获取 Agent 列表
fn handle_agents_list(
  gateway : &Gateway,
  _params : Json,
) -> Result[Json, HttpError> {
  let agents = match gateway.scheduler_ext {
    Some(ext) => ext.config.agents.to_json()
    None => [].to_json()
  }
  Ok(agents)
}

/// RPC: 获取 Skill 列表
fn handle_skills_list(
  gateway : &Gateway,
  _params : Json,
) -> Result[Json, HttpError> {
  let skills = match gateway.scheduler_ext {
    Some(ext) => ext.config.skills.to_json()
    None => [].to_json()
  }
  Ok(skills)
}

/// RPC: 发送消息
async fn handle_message_send(
  gateway : &Gateway,
  params : Json,
) -> Result[Json, HttpError> {
  let session_id = match params.get("sessionId").to_string() {
    Some(id) => id
    None => return Err(json_error(400, "Missing sessionId", @json.from_string!("{}")))
  }
  
  let content = match params.get("content").to_string() {
    Some(c) => c
    None => return Err(json_error(400, "Missing content", @json.from_string!("{}")))
  }
  
  let skill_id = params.get("skillId").to_string()
  
  match gateway.scheduler_ext {
    Some(ext) => {
      let result = if skill_id.is_some() {
        ext.handle_skill_call(session_id, skill_id.unwrap(), {}, None)
      } else {
        ext.handle_user_message(session_id, content, None, None)
      }
      
      match result {
        Ok(r) => Ok(@json.from_string!("{\"messageId\": \"\{@uuid.generate()}\", \"status\": \"dispatched\"}"))
        Err(e) => Err(json_error(500, e.to_message(), @json.from_string!("{}")))
      }
    }
    None => Err(json_error(503, "Scheduler not available", @json.from_string!("{}")))
  }
}
```

- [ ] **步骤 2：注册 RPC 方法**

```moonbit
// 在 RPC 路由注册中添加
registry.register("agents.list", handle_agents_list)
registry.register("skills.list", handle_skills_list)
registry.register("messages.send", handle_message_send)
```

- [ ] **步骤 3：创建 RPC 文档 docs/api/rpc.md**

```markdown
# ClawTeam RPC API

## agents.list

获取所有 Agent 配置列表。

**方法名:** `agents.list`

**参数:** 无

**返回:**
```json
[
  {
    "id": "assistant-1",
    "name": "主助手",
    "role": "assistant",
    "cliTool": "opencode",
    "enabled": true
  }
]
```

## skills.list

获取所有 Skill 配置列表。

**方法名:** `skills.list`

**参数:** 无

**返回:**
```json
[
  {
    "id": "generate-api",
    "name": "Generate API",
    "description": "Generate REST API endpoints"
  }
]
```

## messages.send

发送消息到指定会话。

**方法名:** `messages.send`

**参数:**
```json
{
  "sessionId": "session-123",
  "content": "生成用户管理模块",
  "skillId": "generate-api"  // 可选
}
```

**返回:**
```json
{
  "messageId": "msg-456",
  "status": "dispatched"
}
```
```

- [ ] **步骤 4：运行检查**

运行：`moon check gateway/`
预期：PASS

- [ ] **步骤 5：Commit**

```bash
git add gateway/server/rpc.mbt docs/api/rpc.md
git commit -m "feat: add RPC endpoints for agents, skills, messages"
```

---

### 任务 3：WebSocket 事件广播

**文件：**
- 创建：`gateway/server/ws_events.mbt`

- [ ] **步骤 1：创建 ws_events.mbt**

```moonbit
/// WebSocket 事件广播器
pub struct WsEventBroadcaster {
  connections : Map[String, WebSocketConnection]
  logger : @pino.Logger
}

pub struct WebSocketConnection {
  id : String
  session_id : String
  conn : @httpx.WebSocket
}

/// WebSocket 事件类型
pub enum WsEvent {
  AgentStatusChanged(AgentStatusEvent)
  MessageStatusChanged(MessageStatusEvent)
  NewMessage(NewMessageEvent)
  CliOutput(CliOutputEvent)
  AuditEntry(AuditEvent)
} derive(ToJson)

pub struct AgentStatusEvent {
  agent_id : String
  old_status : String
  new_status : String
  timestamp : Int
} derive(ToJson)

pub struct MessageStatusEvent {
  message_id : String
  old_status : String
  new_status : String
  timestamp : Int
} derive(ToJson)

pub struct NewMessageEvent {
  message : @types.ChatMessage
} derive(ToJson)

pub struct CliOutputEvent {
  session_id : String
  agent_id : String
  output_type : String
  content : String
  timestamp : Int
} derive(ToJson)

pub struct AuditEvent {
  entry : @logger.AuditLogEntry
} derive(ToJson)

pub fn WsEventBroadcaster::new(logger : @pino.Logger) -> WsEventBroadcaster {
  WsEventBroadcaster::{
    connections: {},
    logger,
  }
}

/// 添加连接
pub fn WsEventBroadcaster::add_connection(
  self : &mut WsEventBroadcaster,
  session_id : String,
  conn : @httpx.WebSocket,
) -> String {
  let id = @uuid.generate()
  self.connections.set(id, WebSocketConnection::{
    id,
    session_id,
    conn,
  })
  self.logger.info("WebSocket connected: \{id} for session \{session_id}")
  id
}

/// 移除连接
pub fn WsEventBroadcaster::remove_connection(
  self : &mut WsEventBroadcaster,
  connection_id : String,
) -> Unit {
  self.connections.remove(connection_id)
  self.logger.info("WebSocket disconnected: \{connection_id}")
}

/// 广播事件到会话
pub async fn WsEventBroadcaster::broadcast_to_session(
  self : &WsEventBroadcaster,
  session_id : String,
  event : WsEvent,
) -> Unit {
  let event_json = event.to_json().stringify()
  
  for id, conn in self.connections {
    if conn.session_id == session_id {
      conn.conn.send(event_json)
    }
  }
}

/// 广播事件到所有连接
pub async fn WsEventBroadcaster::broadcast_all(
  self : &WsEventBroadcaster,
  event : WsEvent,
) -> Unit {
  let event_json = event.to_json().stringify()
  
  for id, conn in self.connections {
    conn.conn.send(event_json)
  }
}
```

- [ ] **步骤 2：在 Gateway 中集成 WebSocket**

```moonbit
// 添加字段
pub struct Gateway {
  // ...
  ws_broadcaster : @ws_events.WsEventBroadcaster
}

// 初始化
let ws_broadcaster = @ws_events.WsEventBroadcaster::new(logger)

// WebSocket 路由
fn handle_websocket(
  gateway : &mut Gateway,
  request : @httpx.Request,
) -> @httpx.Response {
  let session_id = request.query.get("sessionId").unwrap_or("default")
  
  // 升级为 WebSocket
  let ws = @httpx.WebSocket::upgrade(request)
  let conn_id = gateway.ws_broadcaster.add_connection(session_id, ws)
  
  // 处理消息
  loop {
    match ws.receive() {
      Ok(msg) => {
        // 处理客户端消息
      }
      Err(_) => {
        gateway.ws_broadcaster.remove_connection(conn_id)
        break
      }
    }
  }
  
  @httpx.Response::empty()
}
```

- [ ] **步骤 3：运行检查**

运行：`moon check gateway/`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add gateway/server/ws_events.mbt
git commit -m "feat: add WebSocket event broadcaster"
```

---

### 任务 4：Feishu 渠道集成

**文件：**
- 修改：`channels/feishu/channel.mbt`
- 创建：`channels/feishu/handler.mbt`

- [ ] **步骤 1：创建 handler.mbt**

```moonbit
/// Feishu 消息处理器
pub struct FeishuHandler {
  scheduler_ext : @scheduler_ext.SchedulerExtension
  logger : @pino.Logger
}

pub fn FeishuHandler::new(
  scheduler_ext : @scheduler_ext.SchedulerExtension,
  logger : @pino.Logger,
) -> FeishuHandler {
  FeishuHandler::{ scheduler_ext, logger }
}

/// 处理飞书消息事件
pub async fn FeishuHandler::handle_message(
  self : &FeishuHandler,
  event : FeishuMessageEvent,
) -> Result[FeishuResponse, String> {
  let user_id = event.sender.id
  let message = event.message.content
  
  self.logger.info("Feishu message from \{user_id}: \{message}")
  
  // 使用 open_id 作为 session_id
  let session_id = "feishu-\{event.sender.open_id}"
  
  // 派发消息
  match self.scheduler_ext.handle_user_message(
    session_id,
    message,
    Some(user_id),
    Some("feishu"),
  ) {
    Ok(result) => {
      // 返回飞书消息
      Ok(FeishuResponse::text(result.stdout))
    }
    Err(e) => {
      Ok(FeishuResponse::text("处理失败: \{e.to_message()}"))
    }
  }
}

/// 飞书响应
pub enum FeishuResponse {
  Text(String)
  Card(Json)
}

impl FeishuResponse {
  pub fn text(content : String) -> FeishuResponse {
    FeishuResponse::Text(content)
  }
  
  pub fn to_json(self : FeishuResponse) -> Json {
    match self {
      FeishuResponse::Text(t) => @json.from_string!("{\"msg_type\":\"text\",\"content\":{\"text\":\"\{t.escape()}\"}}")
      FeishuResponse::Card(c) => @json.from_string!("{\"msg_type\":\"interactive\",\"card\":\{c.stringify()}}")
    }
  }
}
```

- [ ] **步骤 2：修改 channel.mbt 集成处理器**

```moonbit
// 在 FeishuChannel 中添加处理器字段
pub struct FeishuChannel {
  // ...
  handler : @handler.FeishuHandler?
}

// 在消息处理中调用
fn handle_im_message(
  channel : &FeishuChannel,
  event : Json,
) -> Json {
  match channel.handler {
    Some(h) => {
      let msg_event = parse_message_event(event)
      match h.handle_message(msg_event) {
        Ok(response) => response.to_json()
        Err(e) => @json.from_string!("{}")
      }
    }
    None => @json.from_string!("{}")
  }
}
```

- [ ] **步骤 3：运行检查**

运行：`moon check channels/feishu/`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add channels/feishu/handler.mbt channels/feishu/channel.mbt
git commit -m "feat: integrate scheduler with feishu channel"
```

---

### 任务 5：集成测试

**文件：**
- 创建：`tests/integration/scheduler.test.mbt`
- 创建：`tests/integration/moon.pkg`

- [ ] **步骤 1：创建 moon.pkg**

```moonbit
import {
  "moonbitlang/core/test",
  "clawteam/clawteam/config",
  "clawteam/clawteam/scheduler",
  "clawteam/clawteam/internal/pino",
  "clawteam/clawteam/clock",
}
```

- [ ] **步骤 2：创建 scheduler.test.mbt**

```moonbit
test "scheduler extension can be created" {
  let logger = @pino.Logger::new("test")
  let loader = @config.ConfigLoader::new(logger)
  let config = loader.default_config("/tmp/test")
  
  let ext = @scheduler_ext.SchedulerExtension::new(config, logger)
  
  // 验证配置加载正确
  assert!(ext.config.version == "1.0.0")
  assert!(ext.config.agents.length() > 0)
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

test "prompt template renders correctly" {
  let template = "Hello {{name}}, you have {{count}} tasks"
  let variables = { "name": "Agent", "count": "3" }
  
  let result = @scheduler.render_prompt(template, variables)
  
  inspect!(result, content="Hello Agent, you have 3 tasks")
}

test "dispatcher creates cli call params" {
  let logger = @pino.Logger::new("test")
  let config = @config.ConfigLoader::new(logger).default_config("/tmp/test")
  let dispatcher = @scheduler.Dispatcher::new(config, logger)
  
  // 验证创建成功
  assert!(dispatcher.config.agents.length() > 0)
}
```

- [ ] **步骤 3：运行集成测试**

运行：`moon test tests/integration/`
预期：PASS

- [ ] **步骤 4：Commit**

```bash
git add tests/integration/
git commit -m "test: add scheduler integration tests"
```

---

### 任务 6：端到端测试 (Playwright)

**文件：**
- 创建：`tests/e2e/playwright.config.ts`
- 创建：`tests/e2e/basic_flow.test.ts`
- 创建：`tests/e2e/package.json`

- [ ] **步骤 1：创建 package.json**

```json
{
  "name": "clawteam-e2e-tests",
  "version": "1.0.0",
  "scripts": {
    "test": "playwright test",
    "test:ui": "playwright test --ui",
    "report": "playwright show-report"
  },
  "devDependencies": {
    "@playwright/test": "^1.50.0",
    "@types/node": "^24.10.0"
  }
}
```

- [ ] **步骤 2：创建 playwright.config.ts**

```typescript
import { defineConfig, devices } from '@playwright/test'

export default defineConfig({
  testDir: './',
  fullyParallel: true,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 2 : 0,
  workers: process.env.CI ? 1 : undefined,
  reporter: 'html',
  use: {
    baseURL: 'http://localhost:5174',
    trace: 'on-first-retry',
  },
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    },
  ],
  webServer: {
    command: 'cd ../../ui/clawteam-chat && pnpm dev',
    url: 'http://localhost:5174',
    reuseExistingServer: !process.env.CI,
  },
})
```

- [ ] **步骤 3：创建 basic_flow.test.ts**

```typescript
import { test, expect } from '@playwright/test'

test.describe('ClawTeam Basic Flow', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/')
  })

  test('should load main page', async ({ page }) => {
    // 等待页面加载
    await expect(page.locator('text=ClawTeam')).toBeVisible()
  })

  test('should display agent panel', async ({ page }) => {
    // 等待 Agent 面板加载
    await expect(page.locator('text=Agent 成员')).toBeVisible()
    
    // 应该有至少一个助手
    await expect(page.locator('text=助手')).toBeVisible()
  })

  test('should send message', async ({ page }) => {
    // 找到输入框
    const input = page.locator('input[placeholder="输入消息..."]')
    await expect(input).toBeVisible()
    
    // 输入消息
    await input.fill('测试消息')
    
    // 点击发送按钮
    await page.locator('button:has-text("Send")').click()
    
    // 消息应该显示在列表中
    await expect(page.locator('text=测试消息')).toBeVisible()
  })

  test('should show skill selector', async ({ page }) => {
    // 点击 Skill 选择器
    await page.locator('button:has-text("选择 Skill")').click()
    
    // 应该显示技能菜单
    // 注：这需要后端返回技能列表
  })

  test('should toggle output panel', async ({ page }) => {
    // 点击显示输出按钮
    await page.locator('text=显示输出').click()
    
    // 输出面板应该显示
    await expect(page.locator('text=CLI 输出')).toBeVisible()
    
    // 再次点击隐藏
    await page.locator('text=隐藏输出').click()
    
    // 输出面板应该隐藏
    await expect(page.locator('text=CLI 输出')).not.toBeVisible()
  })
})

test.describe('Agent Panel', () => {
  test('should show agent status', async ({ page }) => {
    await page.goto('/')
    
    // 等待 Agent 加载
    await expect(page.locator('[class*="agent"]')).toBeVisible()
  })

  test('should click agent card', async ({ page }) => {
    await page.goto('/')
    
    // 点击 Agent 卡片
    const agentCard = page.locator('.p-3.rounded-lg.border').first()
    await agentCard.click()
    
    // 应该有选中状态
    await expect(agentCard).toHaveAttribute('class', /ring-2/)
  })
})
```

- [ ] **步骤 4：安装依赖**

```bash
cd tests/e2e
pnpm install
npx playwright install chromium
```

- [ ] **步骤 5：运行 E2E 测试**

运行：`pnpm test`
预期：大部分测试通过（部分需要后端）

- [ ] **步骤 6：Commit**

```bash
git add tests/e2e/
git commit -m "test: add Playwright E2E tests"
```

---

### 任务 7：文档与部署配置

**文件：**
- 创建：`docs/deployment.md`
- 创建：`scripts/start.sh`
- 创建：`scripts/start.ps1`

- [ ] **步骤 1：创建部署文档 docs/deployment.md**

```markdown
# ClawTeam 部署指南

## 环境要求

- MoonBit >= 0.1.0
- Node.js >= 20.0.0
- pnpm >= 9.0.0

## 本地开发

### 启动后端

\`\`\`bash
# 构建后端
moon build

# 启动 Gateway
moon run clawteam gateway --port 3000
\`\`\`

### 启动前端

\`\`\`bash
cd ui/clawteam-chat
pnpm install
pnpm dev
\`\`\`

## 生产部署

### 构建前端

\`\`\`bash
cd ui/clawteam-chat
pnpm build
\`\`\`

### 构建后端

\`\`\`bash
moon build --release
\`\`\`

### 配置文件

将 `clawteam.json` 放置在以下位置之一：
- `~/.clawteam/clawteam.json`
- `~/.clawteam/.clawteam/clawteam.json`
- `./clawteam.json`
- `./.clawteam/clawteam.json`

## 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| CLAWTEAM_HOME | 配置目录 | ~/.clawteam |
| CLAWTEAM_PORT | Gateway 端口 | 3000 |
| CLAWTEAM_LOG_LEVEL | 日志级别 | info |
```

- [ ] **步骤 2：创建启动脚本 scripts/start.sh**

```bash
#!/bin/bash

# 启动 ClawTeam 后端
echo "Starting ClawTeam Gateway..."
moon run clawteam gateway --port 3000 &
GATEWAY_PID=$!

# 启动前端
echo "Starting ClawTeam UI..."
cd ui/clawteam-chat
pnpm dev &
UI_PID=$!

# 等待中断
trap "kill $GATEWAY_PID $UI_PID; exit" INT TERM

wait
```

- [ ] **步骤 3：创建 Windows 启动脚本 scripts/start.ps1**

```powershell
# 启动 ClawTeam Gateway
Write-Host "Starting ClawTeam Gateway..."
Start-Process -NoNewWindow moon -ArgumentList "run", "clawteam", "gateway", "--port", "3000"

# 启动前端
Write-Host "Starting ClawTeam UI..."
Set-Location ui/clawteam-chat
Start-Process -NoNewWindow pnpm -ArgumentList "dev"

Write-Host "ClawTeam started. Press Ctrl+C to stop."
```

- [ ] **步骤 4：Commit**

```bash
git add docs/deployment.md scripts/
git commit -m "docs: add deployment guide and start scripts"
```

---

### 任务 8：最终集成与验证

- [ ] **步骤 1：运行完整后端测试**

运行：`moon check && moon test`
预期：PASS

- [ ] **步骤 2：运行前端类型检查**

运行：`cd ui && pnpm check`
预期：PASS

- [ ] **步骤 3：运行前端构建**

运行：`cd ui && pnpm build`
预期：成功

- [ ] **步骤 4：运行 E2E 测试**

运行：`cd tests/e2e && pnpm test`
预期：核心测试通过

- [ ] **步骤 5：运行 moon info 和 moon fmt**

```bash
moon info
moon fmt
```

- [ ] **步骤 6：检查 .mbti diff**

运行：`git diff *.mbti`
预期：无意外变更

- [ ] **步骤 7：Final Commit**

```bash
git add .
git commit -m "feat: complete ClawTeam channel integration and E2E tests"
```

---

## 验证清单

- [ ] 后端 `moon check` 通过
- [ ] 后端 `moon test` 通过
- [ ] 前端 `pnpm check` 通过
- [ ] 前端 `pnpm build` 成功
- [ ] RPC 接口可用
- [ ] WebSocket 连接正常
- [ ] E2E 核心测试通过
- [ ] 文档完整

---

## 完成标志

当三个计划全部完成后，ClawTeam 项目应具备：

1. **后端核心调度层** - 配置系统、Agent 路由、CLI 调度、输出解析、审计日志
2. **前端 UI** - 多 Agent 聊天界面、状态管理、WebSocket 通信
3. **渠道集成** - Web UI、飞书渠道、RPC 接口、端到端测试
