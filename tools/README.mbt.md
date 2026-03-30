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