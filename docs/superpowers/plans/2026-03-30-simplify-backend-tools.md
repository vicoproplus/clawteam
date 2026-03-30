# 后端简化：仅保留 CLI 调用与渠道通讯 实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 简化后端工具系统，仅保留 execute_command（bash CLI调用）工具，保留 Feishu/Weixin 渠道通讯功能，删除所有文件操作工具。

**架构：** 移除 10 个工具包（read_file、write_to_file、apply_patch、replace_in_file、list_files、search_files、read_multiple_files、todo、list_jobs、wait_job），仅保留 execute_command。修改 agent 初始化逻辑，移除文件管理器和 todo 依赖。

**技术栈：** MoonBit

---

## 文件结构

### 将删除的目录（10个工具包）

| 目录 | 说明 |
|------|------|
| `tools/read_file/` | 文件读取工具 |
| `tools/read_multiple_files/` | 批量文件读取工具 |
| `tools/write_to_file/` | 文件写入工具 |
| `tools/replace_in_file/` | 文件搜索替换工具 |
| `tools/apply_patch/` | 补丁应用工具 |
| `tools/list_files/` | 目录列表工具 |
| `tools/search_files/` | 文件搜索工具 |
| `tools/todo/` | 任务管理工具 |
| `tools/list_jobs/` | 后台任务列表工具 |
| `tools/wait_job/` | 后台任务等待工具 |

### 将修改的文件

| 文件 | 修改内容 |
|------|----------|
| `clawteam.mbt` | 移除 setup_agent 中的工具注册，仅保留 execute_command |
| `job/analysis.mbt` | 移除 setup_analysis_agent 中的工具注册 |
| `tools/README.mbt.md` | 更新文档，仅描述 execute_command 工具 |
| `moon.pkg` (根目录) | 移除已删除工具的 import |

### 保留的文件

| 目录/文件 | 说明 |
|-----------|------|
| `tools/execute_command/` | Bash CLI 执行工具（保留） |
| `tool/` | 工具抽象层（保留） |
| `channel/` | 渠道抽象层（保留） |
| `channels/feishu/` | 飞书渠道实现（保留） |
| `channels/weixin/` | 微信渠道实现（保留） |

---

## 任务 1：更新 clawteam.mbt 中的工具注册

**文件：**
- 修改：`E:\moonbit\clawteam\clawteam.mbt:23-35`

**当前代码：**
```moonbit
fn setup_agent(agent : @agent.Agent, cwd~ : String) -> Unit {
  let file_manager = @file.manager(cwd~)
  let todo_list = @todo.new(uuid=agent.uuid, cwd=agent.cwd)
  agent.add_tools([
    @execute_command.new(agent.cwd).to_agent_tool(),
    @list_files.new(file_manager).to_agent_tool(),
    @read_file.new(file_manager).to_agent_tool(),
    @todo.new_tool(todo_list).to_agent_tool(),
    @search_files.new(agent.cwd).to_agent_tool(),
  ])
  // GPT 5.1+ uses apply_patch only, other models use meta_write_to_file
  if agent.model.supports_apply_patch {
    agent.add_tool(@apply_patch.new(agent.cwd))
  } else {
    agent.add_tool(@write_to_file.new(agent.cwd))
  }
}
```

- [ ] **步骤 1：修改 setup_agent 函数，仅保留 execute_command**

```moonbit
fn setup_agent(agent : @agent.Agent, cwd~ : String) -> Unit {
  agent.add_tools([
    @execute_command.new(agent.cwd).to_agent_tool(),
  ])
}
```

- [ ] **步骤 2：移除顶部相关 import（如有）**

检查并移除 `@file`、`@todo`、`@list_files`、`@read_file`、`@search_files`、`@apply_patch`、`@write_to_file` 的 import。

- [ ] **步骤 3：运行 moon check 验证编译**

运行：`moon check`
预期：无错误或仅显示工具删除后的缺失引用错误

- [ ] **步骤 4：Commit**

```bash
git add clawteam.mbt
git commit -m "refactor: simplify agent setup to only use execute_command tool"
```

---

## 任务 2：更新 job/analysis.mbt 中的工具注册

**文件：**
- 修改：`E:\moonbit\clawteam\job\analysis.mbt:164-179`

**当前代码：**
```moonbit
fn setup_analysis_agent(agent : @agent.Agent, cwd : String) -> Unit {
  let file_manager = @file.manager(cwd~)
  let todo_list = @todo.new(uuid=agent.uuid, cwd=agent.cwd)
  agent.add_tools([
    @execute_command.new(cwd).to_agent_tool(),
    @list_files.new(file_manager).to_agent_tool(),
    @read_file.new(file_manager).to_agent_tool(),
    @todo.new_tool(todo_list).to_agent_tool(),
    @search_files.new(agent.cwd).to_agent_tool(),
  ])
  if agent.model.supports_apply_patch {
    agent.add_tool(@apply_patch.new(agent.cwd))
  } else {
    agent.add_tool(@write_to_file.new(agent.cwd))
  }
}
```

- [ ] **步骤 1：修改 setup_analysis_agent 函数**

```moonbit
fn setup_analysis_agent(agent : @agent.Agent, cwd : String) -> Unit {
  agent.add_tools([
    @execute_command.new(cwd).to_agent_tool(),
  ])
}
```

- [ ] **步骤 2：运行 moon check 验证编译**

运行：`moon check`
预期：无错误

- [ ] **步骤 3：Commit**

```bash
git add job/analysis.mbt
git commit -m "refactor: simplify analysis agent to only use execute_command tool"
```

---

## 任务 3：删除文件操作工具目录

**文件：**
- 删除：`E:\moonbit\clawteam\tools\read_file\` 目录
- 删除：`E:\moonbit\clawteam\tools\read_multiple_files\` 目录
- 删除：`E:\moonbit\clawteam\tools\write_to_file\` 目录
- 删除：`E:\moonbit\clawteam\tools\replace_in_file\` 目录
- 删除：`E:\moonbit\clawteam\tools\apply_patch\` 目录
- 删除：`E:\moonbit\clawteam\tools\list_files\` 目录
- 删除：`E:\moonbit\clawteam\tools\search_files\` 目录
- 删除：`E:\moonbit\clawteam\tools\todo\` 目录
- 删除：`E:\moonbit\clawteam\tools\list_jobs\` 目录
- 删除：`E:\moonbit\clawteam\tools\wait_job\` 目录

- [ ] **步骤 1：删除 read_file 目录**

运行：`Remove-Item -Recurse -Force tools\read_file`
预期：目录已删除

- [ ] **步骤 2：删除 read_multiple_files 目录**

运行：`Remove-Item -Recurse -Force tools\read_multiple_files`
预期：目录已删除

- [ ] **步骤 3：删除 write_to_file 目录**

运行：`Remove-Item -Recurse -Force tools\write_to_file`
预期：目录已删除

- [ ] **步骤 4：删除 replace_in_file 目录**

运行：`Remove-Item -Recurse -Force tools\replace_in_file`
预期：目录已删除

- [ ] **步骤 5：删除 apply_patch 目录**

运行：`Remove-Item -Recurse -Force tools\apply_patch`
预期：目录已删除

- [ ] **步骤 6：删除 list_files 目录**

运行：`Remove-Item -Recurse -Force tools\list_files`
预期：目录已删除

- [ ] **步骤 7：删除 search_files 目录**

运行：`Remove-Item -Recurse -Force tools\search_files`
预期：目录已删除

- [ ] **步骤 8：删除 todo 目录**

运行：`Remove-Item -Recurse -Force tools\todo`
预期：目录已删除

- [ ] **步骤 9：删除 list_jobs 目录**

运行：`Remove-Item -Recurse -Force tools\list_jobs`
预期：目录已删除

- [ ] **步骤 10：删除 wait_job 目录**

运行：`Remove-Item -Recurse -Force tools\wait_job`
预期：目录已删除

- [ ] **步骤 11：Commit 删除操作**

```bash
git add -A tools/
git commit -m "refactor: remove file operation and task management tools"
```

---

## 任务 4：更新 tools/README.mbt.md 文档

**文件：**
- 修改：`E:\moonbit\clawteam\tools\README.mbt.md`

- [ ] **步骤 1：重写 README.mbt.md，仅描述 execute_command**

```markdown
# Tools Packages

This directory contains tool implementations that the AI agent can use.

## Available Tools

| Tool | Description |
|------|-------------|
| `execute_command` | Run shell commands |

---

## execute_command

Execute shell commands with timeout and output capture.

```moonbit nocheck
pub struct CommandOutput {
  text : String
  truncated_lines : Int
  original_lines : Int
}

pub enum CommandResult {
  Completed(command~ : String, status~ : Int, stdout~ : String, stderr~ : String, max_output_lines~ : Int)
  TimedOut(command~ : String, timeout~ : Int, stdout~ : String, stderr~ : String, max_output_lines~ : Int)
  Background(command~ : String, job_id~ : @job.Id)
}

pub fn new(cwd : String) -> @tool.Tool[CommandResult]
```

**Parameters:**
- `command`: Shell command to execute (required)
- `timeout`: Timeout in milliseconds (default: 600000)
- `max_output_lines`: Maximum lines to capture (default: 100)
- `working_directory`: Working directory (default: cwd)
- `background`: Run in background (not fully supported)

---

## Tool Registration Pattern

```moonbit nocheck
// Create tool with context
let tool = @execute_command.new(cwd)

// Convert to agent tool
let agent_tool = tool.to_agent_tool()

// Add to agent
agent.add_tool(tool)
// or
agent.add_tools([tool.to_agent_tool()])
```
```

- [ ] **步骤 2：Commit**

```bash
git add tools/README.mbt.md
git commit -m "docs: update tools README to only document execute_command"
```

---

## 任务 5：更新根目录 moon.pkg 依赖

**文件：**
- 修改：`E:\moonbit\clawteam\moon.pkg`

- [ ] **步骤 1：检查 moon.pkg 中的工具导入**

运行：`moon check` 查看是否有未解决的导入错误

- [ ] **步骤 2：移除已删除工具的 import**

如果 moon.pkg 中包含已删除工具的导入，移除它们。

- [ ] **步骤 3：运行 moon check 验证**

运行：`moon check`
预期：无错误

- [ ] **步骤 4：Commit**

```bash
git add moon.pkg
git commit -m "refactor: remove deleted tool imports from moon.pkg"
```

---

## 任务 6：清理测试文件中的工具引用

**文件：**
- 检查并修改：`E:\moonbit\clawteam\agent\agent_test.mbt`
- 检查并修改：`E:\moonbit\clawteam\agent\agent_skill_test.mbt`

- [ ] **步骤 1：搜索测试文件中的工具引用**

运行搜索确认需要修改的测试文件

- [ ] **步骤 2：修改或删除相关测试**

根据测试内容决定修改或删除引用已删除工具的测试用例

- [ ] **步骤 3：运行 moon test 验证**

运行：`moon test`
预期：所有测试通过

- [ ] **步骤 4：Commit**

```bash
git add agent/agent_test.mbt agent/agent_skill_test.mbt
git commit -m "refactor: update tests to remove deleted tool references"
```

---

## 任务 7：最终验证

- [ ] **步骤 1：运行完整检查**

运行：`moon check && moon test`
预期：所有检查和测试通过

- [ ] **步骤 2：运行 moon info 更新接口**

运行：`moon info`
预期：无错误

- [ ] **步骤 3：格式化代码**

运行：`moon fmt`
预期：代码格式化完成

- [ ] **步骤 4：最终 Commit**

```bash
git add -A
git commit -m "refactor: complete backend simplification - only execute_command tool"
```

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 其他模块依赖已删除工具 | 编译失败 | 通过 moon check 定位并修复 |
| 测试用例引用已删除工具 | 测试失败 | 删除或修改相关测试 |
| 渠道功能受影响 | 功能缺失 | channel/ 和 channels/ 目录保持不变 |

---

## 预期结果

删除完成后，项目将：
1. 仅保留 `tools/execute_command/` 工具
2. 保留完整的 `channel/` 和 `channels/` 渠道通讯系统
3. Agent 初始化仅注册 execute_command 工具
4. 代码库更精简，专注于 CLI 调用编排
