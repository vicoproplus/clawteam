# Paperclip → MoonBit wasm-gc 迁移计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use executing-plans or subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 paperclip 项目的 4 个高频计算模块迁移到 MoonBit wasm-gc，通过 WebAssembly 提升核心算法执行性能。

**Architecture:** 在仓库根目录创建 MoonBit 项目，将 routine-variables、project-mentions、log-redaction、session-compaction 四个模块迁移为 MoonBit 代码，编译为 wasm-gc 目标。通过 Node.js wasm loader 加载，bridge 层实现透明替换，原始 TypeScript 代码保留在 `./paperclip/` 作为参考和降级方案。

**Tech Stack:** MoonBit 0.4+, wasm-gc 后端, Node.js WebAssembly API, TypeScript bridge 层, 纯手动字符串解析（无 FFI 依赖）

---

## RALPLAN-DR Summary

### Principles（原则）

1. **渐进式迁移**：从最简单模块开始，逐个验证，保留 TS fallback
2. **零 FFI 依赖**：所有字符串处理在 wasm 内部完成，避免跨调用开销
3. **测试驱动**：每个模块必须有对比测试，验证与 TS 输出一致
4. **性能可测量**：每个模块迁移后必须有基准测试对比 TS vs wasm
5. **原始代码只读**：`./paperclip/` 目录保持不变，仅作为参考

### Decision Drivers（决策驱动）

1. **性能优先**：迁移目标是显著提升性能，而非代码美观
2. **工程可控**：单包渐进式迁移，风险分散，可随时回退
3. **透明集成**：调用方无需关心底层实现，bridge 层自动切换

### Viable Options（可行方案）

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| **方案 A（已选）** | 单包渐进迁移 | 风险低，易验证，学习曲线平滑 | 周期较长，需维护双份代码 |
| 方案 B | 独立包并行迁移 | 模块化清晰，可并行 | 初始工作量大，管理复杂 |
| 方案 C | 只迁移核心算法 | 性能收益比高 | 接口设计复杂，序列化可能成瓶颈 |

**选择方案 A 的理由**：符合性能优化目标的同时保持工程可控，从最简单模块开始积累经验，逐步完善 wasm 基础设施。

---

## 需求摘要

### 迁移模块

| 优先级 | 模块 | 原始路径 | 行数 | 复杂度 |
|--------|------|----------|------|--------|
| P0 | routine-variables | `paperclip/packages/shared/src/routine-variables.ts` | ~60 | 低 |
| P0 | project-mentions | `paperclip/packages/shared/src/project-mentions.ts` | ~190 | 中 |
| P1 | log-redaction | `paperclip/packages/adapter-utils/src/log-redaction.ts` | ~90 | 中 |
| P1 | session-compaction | `paperclip/packages/adapter-utils/src/session-compaction.ts` | ~170 | 中 |

### 验收标准

1. **功能正确性**：每个 MoonBit 模块通过对比测试，输出与 TS 版本 100% 一致
2. **性能提升**：基准测试显示 wasm 版本相对 TS 版本有显著提升（记录具体数据）
3. **编译通过**：`moon check`、`moon test`、`moon build --target wasm-gc` 全部通过
4. **集成可用**：bridge 层正常工作，wasm 加载失败时自动 fallback 到 TS
5. **文档完整**：每个模块有 API 文档和性能对比报告

---

## 文件结构

### 新建文件

```
clawteam/
├── moon.mod.json                          # MoonBit 模块定义
├── moon.pkg.json                          # 根包配置
├── src/
│   ├── routine_variables.mbt              # Task 1
│   ├── project_mentions.mbt               # Task 2
│   ├── log_redaction.mbt                  # Task 3
│   ├── session_compaction.mbt             # Task 4
│   └── string_utils.mbt                   # 共享字符串工具（手动解析实现）
├── src_test/
│   ├── routine_variables_test.mbt         # Task 1 测试
│   ├── routine_variables_bench.mbt        # Task 1 性能基准
│   ├── project_mentions_test.mbt          # Task 2 测试
│   ├── project_mentions_bench.mbt         # Task 2 性能基准
│   ├── log_redaction_test.mbt             # Task 3 测试
│   ├── log_redaction_bench.mbt            # Task 3 性能基准
│   ├── session_compaction_test.mbt        # Task 4 测试
│   └── session_compaction_bench.mbt       # Task 4 性能基准
├── wasm_loader/
│   ├── loader.ts                          # wasm 加载器
│   ├── bindings.ts                        # 类型声明
│   ├── memory.ts                          # 内存管理工具
│   └── init.ts                            # 初始化入口
├── bridge/
│   └── wasm_bridge.ts                     # 统一桥接层
├── benchmark/
│   ├── routine_variables.bench.ts         # TS vs wasm 对比
│   ├── project_mentions.bench.ts
│   ├── log_redaction.bench.ts
│   └── session_compaction.bench.ts
└── docs/
    └── migration/
        ├── overview.md                    # 迁移概述
        └── performance-report.md          # 性能对比报告
```

### 参考文件（只读）

```
paperclip/
├── packages/shared/src/routine-variables.ts
├── packages/shared/src/project-mentions.ts
├── packages/adapter-utils/src/log-redaction.ts
└── packages/adapter-utils/src/session-compaction.ts
```

---

## 实施步骤

### Task 1: MoonBit 项目初始化

**Files:**
- Create: `moon.mod.json`
- Create: `moon.pkg.json`
- Create: `src/string_utils.mbt`

- [ ] **Step 1: 创建 moon.mod.json**

```json
{
  "name": "paperclip-wasm",
  "version": "0.1.0",
  "readme": "README.md",
  "repository": "",
  "license": "MIT",
  "source": "src"
}
```

- [ ] **Step 2: 创建 moon.pkg.json**

```json
{
  "is-main": false,
  "targets": {
    "wasm-gc": ["wasm-gc"]
  },
  "deps": {}
}
```

- [ ] **Step 3: 创建共享字符串工具模块**

```moonbit
// src/string_utils.mbt

/// 查找子串位置，返回 Option[Int]
pub fn find_substring(haystack: String, needle: String) -> Option[Int] {
  let h_len = haystack.length()
  let n_len = needle.length()
  if n_len == 0 || n_len > h_len {
    return None
  }
  let mut i = 0
  while i <= h_len - n_len {
    let mut found = true
    let mut j = 0
    while j < n_len {
      if haystack.byte_at(i + j) != needle.byte_at(j) {
        found = false
        break
      }
      j = j + 1
    }
    if found {
      return Some(i)
    }
    i = i + 1
  }
  None
}

/// 提取子串（基于起始位置和长度）
pub fn substring(s: String, start: Int, length: Int) -> String {
  let mut result = ""
  let mut i = 0
  while i < length {
    result = result + String::from_byte(s.byte_at(start + i))
    i = i + 1
  }
  result
}

/// 检查前缀
pub fn starts_with(s: String, prefix: String) -> Bool {
  let s_len = s.length()
  let p_len = prefix.length()
  if p_len > s_len {
    return false
  }
  let mut i = 0
  while i < p_len {
    if s.byte_at(i) != prefix.byte_at(i) {
      return false
    }
    i = i + 1
  }
  true
}
```

- [ ] **Step 4: 验证项目结构**

Run: `moon check`
Expected: 编译通过，无错误

- [ ] **Step 5: 提交**

```bash
git add moon.mod.json moon.pkg.json src/string_utils.mbt
git commit -m "init: MoonBit wasm project structure"
```

---

### Task 2: 迁移 routine-variables.ts

**Files:**
- Create: `src/routine_variables.mbt`
- Create: `src_test/routine_variables_test.mbt`
- Create: `src_test/routine_variables_bench.mbt`
- Reference: `paperclip/packages/shared/src/routine-variables.ts`

- [ ] **Step 1: 阅读原始实现**

Read: `paperclip/packages/shared/src/routine-variables.ts`
目的：理解 extractVariables 和 interpolate 的完整逻辑

- [ ] **Step 2: 实现 MoonBit 版本**

```moonbit
// src/routine_variables.mbt

/// 提取模板变量
/// 输入: "{{agent.name}} 处理 {{issue.title}}"
/// 输出: ["agent.name", "issue.title"]
pub fn extract_variables(template: String) -> Array[String] {
  let mut variables = []
  let len = template.length()
  let mut i = 0
  
  while i < len - 1 {
    if template.byte_at(i) == '{'.codepoint_at(0) && 
       template.byte_at(i + 1) == '{'.codepoint_at(0) {
      let start = i + 2
      let mut j = start
      while j < len - 1 {
        if template.byte_at(j) == '}'.codepoint_at(0) && 
           template.byte_at(j + 1) == '}'.codepoint_at(0) {
          let var_name = template.substring(start, j - start)
          variables = variables.push(var_name)
          i = j + 2
          break
        }
        j = j + 1
      }
      if j >= len - 1 {
        i = len
      }
    } else {
      i = i + 1
    }
  }
  
  variables
}

/// 执行模板插值
/// 输入: template="{{agent.name}}", vars={"agent.name": "Claude"}
/// 输出: "Claude"
pub fn interpolate(template: String, vars: Map[String, String]) -> String {
  let mut result = ""
  let len = template.length()
  let mut i = 0
  
  while i < len {
    if i < len - 1 && 
       template.byte_at(i) == '{'.codepoint_at(0) && 
       template.byte_at(i + 1) == '{'.codepoint_at(0) {
      let start = i + 2
      let mut j = start
      while j < len - 1 {
        if template.byte_at(j) == '}'.codepoint_at(0) && 
           template.byte_at(j + 1) == '}'.codepoint_at(0) {
          let var_name = template.substring(start, j - start)
          match vars.get(var_name) {
            Some(value) => result = result + value
            None => result = result + "{{" + var_name + "}}"
          }
          i = j + 2
          break
        }
        j = j + 1
      }
      if j >= len - 1 {
        result = result + String::from_byte(template.byte_at(i))
        i = i + 1
      }
    } else {
      result = result + String::from_byte(template.byte_at(i))
      i = i + 1
    }
  }
  
  result
}
```

- [ ] **Step 3: 编写对比测试**

```moonbit
// src_test/routine_variables_test.mbt

test "extract_variables single variable" {
  let result = extract_variables("{{agent.name}}")
  inspect(result, content=["agent.name"])
}

test "extract_variables multiple variables" {
  let result = extract_variables("{{agent.name}} 处理 {{issue.title}}")
  inspect(result, content=["agent.name", "issue.title"])
}

test "extract_variables no variables" {
  let result = extract_variables("plain text")
  inspect(result, content=[])
}

test "interpolate single variable" {
  let vars = Map::from_iter([("agent.name", "Claude")])
  let result = interpolate("{{agent.name}}", vars)
  inspect(result, content="Claude")
}

test "interpolate multiple variables" {
  let vars = Map::from_iter([
    ("agent.name", "Claude"),
    ("issue.title", "Bug Fix")
  ])
  let result = interpolate("{{agent.name}} 处理 {{issue.title}}", vars)
  inspect(result, content="Claude 处理 Bug Fix")
}

test "interpolate missing variable" {
  let vars = Map::from_iter([])
  let result = interpolate("{{missing}}", vars)
  inspect(result, content="{{missing}}")
}
```

- [ ] **Step 4: 运行测试**

Run: `moon test`
Expected: 所有测试通过

- [ ] **Step 5: 编写性能基准测试**

```moonbit
// src_test/routine_variables_bench.mbt

test "extract_variables benchmark" {
  let template = "{{a}} {{b}} {{c}} {{d}} {{e}} {{f}} {{g}} {{h}} {{i}} {{j}}"
  let iterations = 10000
  let start = Performance.now()
  let mut i = 0
  while i < iterations {
    extract_variables(template)
    i = i + 1
  }
  let elapsed = Performance.now() - start
  println("wasm extract_variables: \(elapsed)ms for \(iterations) iterations")
}
```

- [ ] **Step 6: 运行基准测试**

Run: `moon test src_test/routine_variables_bench.mbt`
Expected: 输出性能数据

- [ ] **Step 7: 格式化代码**

Run: `moon fmt`

- [ ] **Step 8: 提交**

```bash
git add src/routine_variables.mbt src_test/routine_variables_test.mbt src_test/routine_variables_bench.mbt
git commit -m "feat: migrate routine-variables to MoonBit wasm"
```

---

### Task 3: 迁移 project-mentions.ts

**Files:**
- Create: `src/project_mentions.mbt`
- Create: `src_test/project_mentions_test.mbt`
- Create: `src_test/project_mentions_bench.mbt`
- Reference: `paperclip/packages/shared/src/project-mentions.ts`

- [ ] **Step 1: 阅读原始实现**

Read: `paperclip/packages/shared/src/project-mentions.ts`
目的：理解 mention 解析、颜色规范化、协议构建的完整逻辑

- [ ] **Step 2: 实现 MoonBit 版本**

```moonbit
// src/project_mentions.mbt

pub enum MentionType {
  Project
  Agent
  Skill
} derive(Show, Eq)

pub struct Mention {
  type: MentionType
  id: String
  display: String
  raw: String
} derive(Show)

/// 解析 mention 协议
pub fn parse_mentions(text: String) -> Array[Mention] {
  let mut mentions = []
  let len = text.length()
  let mut i = 0
  
  while i < len {
    let protocols = ["project://", "agent://", "skill://"]
    let mut found = false
    
    for protocol in protocols {
      let proto_len = protocol.length()
      if i + proto_len <= len && text.substring(i, proto_len) == protocol {
        let start = i + proto_len
        let mut j = start
        while j < len {
          let ch = text.byte_at(j)
          if ch == ' '.codepoint_at(0) || ch == '\n'.codepoint_at(0) || ch == ')'.codepoint_at(0) {
            break
          }
          j = j + 1
        }
        
        let id = text.substring(start, j - start)
        let raw = text.substring(i, j - i)
        let type = match protocol {
          "project://" => MentionType::Project
          "agent://" => MentionType::Agent
          "skill://" => MentionType::Skill
          _ => MentionType::Project
        }
        
        mentions = mentions.push(Mention::{
          type: type,
          id: id,
          display: id,
          raw: raw
        })
        
        i = j
        found = true
        break
      }
    }
    
    if not found {
      i = i + 1
    }
  }
  
  mentions
}

/// 规范化十六进制颜色值
pub fn normalize_color(color: String) -> String {
  let len = color.length()
  if len == 4 && color.byte_at(0) == '#'.codepoint_at(0) {
    let r = color.substring(1, 1)
    let g = color.substring(2, 1)
    let b = color.substring(3, 1)
    "#" + r + r + g + g + b + b
  } else {
    color
  }
}

/// 构建 mention 协议字符串
pub fn build_mention(type: MentionType, id: String) -> String {
  let prefix = match type {
    MentionType::Project => "project://"
    MentionType::Agent => "agent://"
    MentionType::Skill => "skill://"
  }
  prefix + id
}
```

- [ ] **Step 3: 编写对比测试**

```moonbit
// src_test/project_mentions_test.mbt

test "parse_mentions project mention" {
  let result = parse_mentions("查看 project://proj-123")
  inspect(result.length(), content=1)
  inspect(result[0].type, content=MentionType::Project)
  inspect(result[0].id, content="proj-123")
}

test "parse_mentions multiple mentions" {
  let result = parse_mentions("project://p1 agent://a1")
  inspect(result.length(), content=2)
}

test "normalize_color short hex" {
  let result = normalize_color("#abc")
  inspect(result, content="#aabbcc")
}

test "normalize_color full hex" {
  let result = normalize_color("#aabbcc")
  inspect(result, content="#aabbcc")
}

test "build_mention project" {
  let result = build_mention(MentionType::Project, "proj-123")
  inspect(result, content="project://proj-123")
}
```

- [ ] **Step 4: 运行测试**

Run: `moon test`
Expected: 所有测试通过

- [ ] **Step 5: 编写性能基准测试**

```moonbit
// src_test/project_mentions_bench.mbt

test "parse_mentions benchmark" {
  let text = "project://p1 agent://a1 skill://s1 project://p2 agent://a2" * 100
  let iterations = 1000
  let start = Performance.now()
  let mut i = 0
  while i < iterations {
    parse_mentions(text)
    i = i + 1
  }
  let elapsed = Performance.now() - start
  println("wasm parse_mentions: \(elapsed)ms for \(iterations) iterations")
}
```

- [ ] **Step 6: 运行基准测试**

Run: `moon test src_test/project_mentions_bench.mbt`

- [ ] **Step 7: 格式化并提交**

Run: `moon fmt`

```bash
git add src/project_mentions.mbt src_test/project_mentions_test.mbt src_test/project_mentions_bench.mbt
git commit -m "feat: migrate project-mentions to MoonBit wasm"
```

---

### Task 4: 迁移 log-redaction.ts

**Files:**
- Create: `src/log_redaction.mbt`
- Create: `src_test/log_redaction_test.mbt`
- Create: `src_test/log_redaction_bench.mbt`
- Reference: `paperclip/packages/adapter-utils/src/log-redaction.ts`

- [ ] **Step 1: 阅读原始实现**

Read: `paperclip/packages/adapter-utils/src/log-redaction.ts`
目的：理解路径脱敏、对象脱敏逻辑

- [ ] **Step 2: 实现 MoonBit 版本**

```moonbit
// src/log_redaction.mbt

const REDACTED = "[REDACTED]"

/// 脱敏文本中的路径
pub fn redact_paths(text: String, patterns: Array[String]) -> String {
  let mut result = text
  for pattern in patterns {
    result = replace_all(result, pattern, REDACTED)
  }
  result
}

/// 替换所有匹配
fn replace_all(text: String, pattern: String, replacement: String) -> String {
  let mut result = ""
  let text_len = text.length()
  let pattern_len = pattern.length()
  let mut i = 0
  
  while i < text_len {
    if i + pattern_len <= text_len && text.substring(i, pattern_len) == pattern {
      result = result + replacement
      i = i + pattern_len
    } else {
      result = result + String::from_byte(text.byte_at(i))
      i = i + 1
    }
  }
  
  result
}

/// 递归脱敏 JSON 对象
pub fn redact_json_object(json: @json.Json, patterns: Array[String]) -> @json.Json {
  match json {
    @json.Json::String(s) => @json.Json::String(redact_paths(s, patterns))
    @json.Json::Object(obj) => {
      let mut new_obj = Map::new()
      for (key, value) in obj {
        new_obj[key] = redact_json_object(value, patterns)
      }
      @json.Json::Object(new_obj)
    }
    @json.Json::Array(arr) => {
      let mut new_arr = []
      for item in arr {
        new_arr = new_arr.push(redact_json_object(item, patterns))
      }
      @json.Json::Array(new_arr)
    }
    _ => json
  }
}
```

- [ ] **Step 3: 编写对比测试**

```moonbit
// src_test/log_redaction_test.mbt

test "redact_paths single pattern" {
  let result = redact_paths("Error at /Users/john/secret/key.pem", ["/Users/john/"])
  inspect(result, content="Error at [REDACTED]key.pem")
}

test "redact_paths multiple patterns" {
  let result = redact_paths("Path: /home/user, Token: abc123", ["/home/user", "abc123"])
  inspect(result, content="Path: [REDACTED], Token: [REDACTED]")
}

test "redact_json_object" {
  let json = @json.Json::Object(Map::from_iter([
    ("message", @json.Json::String("Error at /secret/path")),
    ("code", @json.Json::Number(500))
  ]))
  let result = redact_json_object(json, ["/secret/"])
  let expected = @json.Json::Object(Map::from_iter([
    ("message", @json.Json::String("Error at [REDACTED]path")),
    ("code", @json.Json::Number(500))
  ]))
  inspect(result, content=expected)
}
```

- [ ] **Step 4: 运行测试**

Run: `moon test`
Expected: 所有测试通过

- [ ] **Step 5: 编写性能基准测试**

```moonbit
// src_test/log_redaction_bench.mbt

test "redact_paths benchmark" {
  let text = "Error at /Users/john/secret/key.pem in /home/user/config" * 100
  let patterns = ["/Users/john/", "/home/user/"]
  let iterations = 1000
  let start = Performance.now()
  let mut i = 0
  while i < iterations {
    redact_paths(text, patterns)
    i = i + 1
  }
  let elapsed = Performance.now() - start
  println("wasm redact_paths: \(elapsed)ms for \(iterations) iterations")
}
```

- [ ] **Step 6: 运行基准测试**

Run: `moon test src_test/log_redaction_bench.mbt`

- [ ] **Step 7: 格式化并提交**

Run: `moon fmt`

```bash
git add src/log_redaction.mbt src_test/log_redaction_test.mbt src_test/log_redaction_bench.mbt
git commit -m "feat: migrate log-redaction to MoonBit wasm"
```

---

### Task 5: 迁移 session-compaction.ts

**Files:**
- Create: `src/session_compaction.mbt`
- Create: `src_test/session_compaction_test.mbt`
- Create: `src_test/session_compaction_bench.mbt`
- Reference: `paperclip/packages/adapter-utils/src/session-compaction.ts`

- [ ] **Step 1: 阅读原始实现**

Read: `paperclip/packages/adapter-utils/src/session-compaction.ts`
目的：理解策略计算、合并逻辑

- [ ] **Step 2: 实现 MoonBit 版本**

```moonbit
// src/session_compaction.mbt

pub enum StrategySource {
  Default
  AdapterConfig
  Override
} derive(Show, Eq)

pub struct AdapterConfig {
  name: String
  max_tokens: Int
  enabled: Bool
} derive(Show)

pub struct CompactionStrategy {
  enabled: Bool
  max_tokens: Int
  strategy_source: StrategySource
} derive(Show, Eq)

/// 解析压缩策略
pub fn resolve_strategy(
  session_count: Int,
  total_tokens: Int,
  adapter_configs: Array[AdapterConfig]
) -> CompactionStrategy {
  if adapter_configs.length() > 0 {
    let config = adapter_configs[0]
    CompactionStrategy::{
      enabled: config.enabled,
      max_tokens: config.max_tokens,
      strategy_source: StrategySource::AdapterConfig
    }
  } else {
    CompactionStrategy::{
      enabled: total_tokens > 1000,
      max_tokens: total_tokens / 2,
      strategy_source: StrategySource::Default
    }
  }
}

/// 合并多个策略
pub fn merge_strategies(
  base: CompactionStrategy,
  overrides: Array[CompactionStrategy]
) -> CompactionStrategy {
  let mut result = base
  for override in overrides {
    result = CompactionStrategy::{
      enabled: override.enabled,
      max_tokens: override.max_tokens,
      strategy_source: StrategySource::Override
    }
  }
  result
}
```

- [ ] **Step 3: 编写对比测试**

```moonbit
// src_test/session_compaction_test.mbt

test "resolve_strategy default" {
  let result = resolve_strategy(5, 2000, [])
  inspect(result.enabled, content=true)
  inspect(result.max_tokens, content=1000)
  inspect(result.strategy_source, content=StrategySource::Default)
}

test "resolve_strategy from adapter" {
  let configs = [AdapterConfig::{
    name: "test",
    max_tokens: 500,
    enabled: true
  }]
  let result = resolve_strategy(5, 2000, configs)
  inspect(result.max_tokens, content=500)
  inspect(result.strategy_source, content=StrategySource::AdapterConfig)
}

test "merge_strategies single override" {
  let base = CompactionStrategy::{
    enabled: true,
    max_tokens: 1000,
    strategy_source: StrategySource::Default
  }
  let overrides = [CompactionStrategy::{
    enabled: false,
    max_tokens: 2000,
    strategy_source: StrategySource::Override
  }]
  let result = merge_strategies(base, overrides)
  inspect(result.enabled, content=false)
  inspect(result.max_tokens, content=2000)
}
```

- [ ] **Step 4: 运行测试**

Run: `moon test`
Expected: 所有测试通过

- [ ] **Step 5: 编写性能基准测试**

```moonbit
// src_test/session_compaction_bench.mbt

test "resolve_strategy benchmark" {
  let iterations = 10000
  let start = Performance.now()
  let mut i = 0
  while i < iterations {
    resolve_strategy(i % 10, i * 100, [])
    i = i + 1
  }
  let elapsed = Performance.now() - start
  println("wasm resolve_strategy: \(elapsed)ms for \(iterations) iterations")
}
```

- [ ] **Step 6: 运行基准测试**

Run: `moon test src_test/session_compaction_bench.mbt`

- [ ] **Step 7: 格式化并提交**

Run: `moon fmt`

```bash
git add src/session_compaction.mbt src_test/session_compaction_test.mbt src_test/session_compaction_bench.mbt
git commit -m "feat: migrate session-compaction to MoonBit wasm"
```

---

### Task 6: 创建 Wasm Loader 和 Bridge 层

**Files:**
- Create: `wasm_loader/loader.ts`
- Create: `wasm_loader/bindings.ts`
- Create: `wasm_loader/memory.ts`
- Create: `wasm_loader/init.ts`
- Create: `bridge/wasm_bridge.ts`
- Create: `benchmark/routine_variables.bench.ts`
- Create: `benchmark/project_mentions.bench.ts`
- Create: `benchmark/log_redaction.bench.ts`
- Create: `benchmark/session_compaction.bench.ts`

- [ ] **Step 1: 创建 wasm loader**

```typescript
// wasm_loader/loader.ts
import { readFileSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

let wasmInstance: WebAssembly.Exports | null = null;
let wasmMemory: WebAssembly.Memory | null = null;

export async function initWasm(): Promise<boolean> {
  try {
    const wasmPath = join(__dirname, '../target/wasm-gc/release/bundle/paperclip-wasm.wasm');
    const wasmBuffer = readFileSync(wasmPath);
    
    const importObject = {
      env: {}
    };
    
    const { instance } = await WebAssembly.instantiate(wasmBuffer, importObject);
    wasmInstance = instance.exports;
    wasmMemory = instance.exports.memory as WebAssembly.Memory;
    
    console.log('[wasm] Paperclip modules loaded');
    return true;
  } catch (err) {
    console.warn('[wasm] Failed to load, falling back to TS:', err.message);
    return false;
  }
}

export function getWasm(): WebAssembly.Exports | null {
  return wasmInstance;
}

export function getMemory(): WebAssembly.Memory | null {
  return wasmMemory;
}

// 内存读写工具
export function writeString(str: string): number {
  if (!wasmMemory) throw new Error('Wasm memory not initialized');
  const encoder = new TextEncoder();
  const data = encoder.encode(str);
  const ptr = wasmInstance!.malloc(data.length);
  const memory = new Uint8Array(wasmMemory.buffer);
  memory.set(data, ptr);
  return ptr;
}

export function readString(ptr: number, length: number): string {
  if (!wasmMemory) throw new Error('Wasm memory not initialized');
  const memory = new Uint8Array(wasmMemory.buffer);
  const data = memory.slice(ptr, ptr + length);
  return new TextDecoder().decode(data);
}
```

```typescript
// wasm_loader/bindings.ts
export type WasmBindings = {
  malloc: (size: number) => number;
  free: (ptr: number) => void;
  
  extract_variables: (ptr: number, len: number) => number;
  interpolate: (tplPtr: number, tplLen: number, varsPtr: number) => number;
  
  parse_mentions: (ptr: number, len: number) => number;
  normalize_color: (ptr: number, len: number) => number;
  build_mention: (type: number, idPtr: number, idLen: number) => number;
  
  redact_paths: (textPtr: number, textLen: number, patternsPtr: number) => number;
  
  resolve_strategy: (sessionCount: number, totalTokens: number, configsPtr: number) => number;
  merge_strategies: (basePtr: number, overridesPtr: number) => number;
};
```

```typescript
// wasm_loader/init.ts
import { initWasm } from './loader.js';

export async function initializeWasmModules(): Promise<void> {
  const success = await initWasm();
  if (!success) {
    console.warn('[wasm] Modules not loaded, using TypeScript fallback');
  }
}
```

- [ ] **Step 2: 创建 bridge 层**

```typescript
// bridge/wasm_bridge.ts
import { getWasm, writeString, readString } from '../wasm_loader/loader.js';

// 导入原始 TS 实现
import * as tsRoutineVars from '../paperclip/packages/shared/src/routine-variables.js';
import * as tsProjectMentions from '../paperclip/packages/shared/src/project-mentions.js';
import * as tsLogRedaction from '../paperclip/packages/adapter-utils/src/log-redaction.js';
import * as tsSessionCompaction from '../paperclip/packages/adapter-utils/src/session-compaction.js';

export const routineVariables = {
  extractVariables(template: string): string[] {
    const wasm = getWasm();
    if (wasm?.extract_variables) {
      const ptr = writeString(template);
      const resultPtr = wasm.extract_variables(ptr, template.length);
      return JSON.parse(readString(resultPtr, 1024)) as string[];
    }
    return tsRoutineVars.extractVariables(template);
  },
  
  interpolate(template: string, vars: Record<string, string>): string {
    const wasm = getWasm();
    if (wasm?.interpolate) {
      const tplPtr = writeString(template);
      const varsPtr = writeString(JSON.stringify(vars));
      const resultPtr = wasm.interpolate(tplPtr, template.length, varsPtr);
      return readString(resultPtr, 4096);
    }
    return tsRoutineVars.interpolate(template, vars);
  }
};

export const projectMentions = {
  parseMentions(text: string) {
    const wasm = getWasm();
    if (wasm?.parse_mentions) {
      const ptr = writeString(text);
      const resultPtr = wasm.parse_mentions(ptr, text.length);
      return JSON.parse(readString(resultPtr, 4096));
    }
    return tsProjectMentions.parseMentions(text);
  },
  
  normalizeColor(color: string): string {
    const wasm = getWasm();
    if (wasm?.normalize_color) {
      const ptr = writeString(color);
      const resultPtr = wasm.normalize_color(ptr, color.length);
      return readString(resultPtr, 64);
    }
    return tsProjectMentions.normalizeColor(color);
  },
  
  buildMention(type: 'project'|'agent'|'skill', id: string): string {
    const wasm = getWasm();
    if (wasm?.build_mention) {
      const typeMap = { project: 0, agent: 1, skill: 2 };
      const idPtr = writeString(id);
      const resultPtr = wasm.build_mention(typeMap[type], idPtr, id.length);
      return readString(resultPtr, 256);
    }
    return tsProjectMentions.buildMention(type, id);
  }
};

export const logRedaction = {
  redactPaths(text: string, patterns: string[]): string {
    const wasm = getWasm();
    if (wasm?.redact_paths) {
      const textPtr = writeString(text);
      const patternsPtr = writeString(JSON.stringify(patterns));
      const resultPtr = wasm.redact_paths(textPtr, text.length, patternsPtr);
      return readString(resultPtr, 4096);
    }
    return tsLogRedaction.redactPaths(text, patterns);
  }
};

export const sessionCompaction = {
  resolveStrategy(sessionCount: number, totalTokens: number, adapterConfigs: any[]) {
    const wasm = getWasm();
    if (wasm?.resolve_strategy) {
      const configsPtr = writeString(JSON.stringify(adapterConfigs));
      const resultPtr = wasm.resolve_strategy(sessionCount, totalTokens, configsPtr);
      return JSON.parse(readString(resultPtr, 1024));
    }
    return tsSessionCompaction.resolveCompactionStrategy(
      Array.from({ length: sessionCount }),
      adapterConfigs
    );
  }
};
```

- [ ] **Step 3: 创建性能基准测试**

```typescript
// benchmark/routine_variables.bench.ts
import { extractVariables as tsExtract } from '../paperclip/packages/shared/src/routine-variables.js';
import { routineVariables } from '../bridge/wasm_bridge.js';

const template = '{{a}} {{b}} {{c}} {{d}} {{e}} {{f}} {{g}} {{h}} {{i}} {{j}}';
const iterations = 10000;

console.log('=== routine-variables 性能对比 ===');
console.log(`模板长度: ${template.length} 字符`);
console.log(`迭代次数: ${iterations}\n`);

// TS 版本
console.time('TypeScript extractVariables');
for (let i = 0; i < iterations; i++) {
  tsExtract(template);
}
console.timeEnd('TypeScript extractVariables');

// wasm 版本
console.time('wasm extract_variables');
for (let i = 0; i < iterations; i++) {
  routineVariables.extractVariables(template);
}
console.timeEnd('wasm extract_variables');
```

创建类似的 benchmark 文件用于其他三个模块。

- [ ] **Step 4: 运行性能基准测试**

Run: `npx tsx benchmark/routine_variables.bench.ts`
Expected: 输出 TS vs wasm 的对比数据

- [ ] **Step 5: 编写性能报告**

```markdown
# docs/migration/performance-report.md

# MoonBit wasm-gc 迁移性能报告

## 测试环境
- Node.js 版本: v20.x
- CPU: [具体型号]
- 内存: [具体大小]

## routine-variables

| 实现 | 执行时间 (ms) | 相对性能 |
|------|---------------|----------|
| TypeScript | XXX | 1.0x |
| MoonBit wasm-gc | XXX | X.Xx |

## project-mentions
...

## log-redaction
...

## session-compaction
...

## 结论
[总结各模块性能收益]
```

- [ ] **Step 6: 提交**

```bash
git add wasm_loader/ bridge/ benchmark/ docs/migration/
git commit -m "feat: add wasm loader, bridge layer, and benchmark infrastructure"
```

---

### Task 7: 集成测试和文档

**Files:**
- Create: `docs/migration/overview.md`
- Create: `README.mbt.md`
- Modify: `package.json` (添加 wasm 相关 scripts)

- [ ] **Step 1: 编写迁移概述文档**

```markdown
# docs/migration/overview.md

# Paperclip → MoonBit wasm-gc 迁移概述

## 迁移目标
将 4 个高频计算模块迁移到 MoonBit wasm-gc，通过 WebAssembly 提升性能。

## 迁移模块
1. routine-variables - 模板变量提取和插值
2. project-mentions - mention 协议解析
3. log-redaction - 日志脱敏
4. session-compaction - 会话压缩策略

## 架构设计
- MoonBit 代码位于仓库根目录 `src/`
- 编译目标：wasm-gc
- Node.js 通过 wasm loader 加载
- bridge 层实现透明替换
- TypeScript 实现保留在 `./paperclip/` 作为降级方案

## 构建命令
```bash
moon check          # 类型检查
moon test           # 运行测试
moon build --target wasm-gc  # 编译 wasm
```

## 性能测试
```bash
npx tsx benchmark/routine_variables.bench.ts
```
```

- [ ] **Step 2: 更新 README.md（MoonBit 文档）**

```markdown
# README.mbt.md

# Paperclip wasm Modules

MoonBit wasm-gc 模块，用于 paperclip 项目的高性能计算核心。

## 模块
- `routine_variables.mbt` - 模板变量引擎
- `project_mentions.mbt` - mention 协议解析
- `log_redaction.mbt` - 日志脱敏
- `session_compaction.mbt` - 会话压缩策略

## 构建
```bash
moon check
moon test
moon build --target wasm-gc
```

## 文档测试
```mbt check
test "extract_variables works" {
  let result = extract_variables("{{name}}")
  inspect(result, content=["name"])
}
```
```

- [ ] **Step 3: 更新 package.json scripts**

```json
{
  "scripts": {
    "wasm:build": "moon build --target wasm-gc",
    "wasm:check": "moon check",
    "wasm:test": "moon test",
    "wasm:benchmark": "npx tsx benchmark/routine_variables.bench.ts"
  }
}
```

- [ ] **Step 4: 运行完整测试**

Run: `moon check && moon test && moon build --target wasm-gc`
Expected: 全部通过

- [ ] **Step 5: 最终提交**

```bash
git add docs/migration/ README.mbt.md package.json
git commit -m "docs: add migration documentation and package scripts"
```

---

## 风险和缓解

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|----------|
| wasm-gc 性能不如预期 | 中 | 中 | 保留 TS fallback，性能数据透明记录 |
| MoonBit 字符串 API 限制 | 高 | 低 | 手动实现基础字符串操作（已在 string_utils.mbt 中） |
| wasm 内存管理复杂 | 中 | 中 | 使用简单的 malloc/free 模式，避免复杂对象 |
| 序列化开销抵消计算收益 | 中 | 中 | 基准测试包含序列化时间，确保端到端收益 |
| MoonBit 编译器 bug | 高 | 低 | 使用稳定版本，记录编译器版本，遇到 bug 及时反馈 |

---

## 验证步骤

### 功能验证
```bash
moon check && moon test
```

### 性能验证
```bash
moon build --target wasm-gc
npx tsx benchmark/routine_variables.bench.ts
npx tsx benchmark/project_mentions.bench.ts
npx tsx benchmark/log_redaction.bench.ts
npx tsx benchmark/session_compaction.bench.ts
```

### 集成验证
```bash
# 确保 bridge 层正常工作
npx tsx -e "
import { initializeWasmModules } from './wasm_loader/init.js';
import { routineVariables } from './bridge/wasm_bridge.js';

await initializeWasmModules();
const result = routineVariables.extractVariables('{{name}}');
console.log('Result:', result);
"
```

---

## 后续改进建议

1. **WASI 支持**：未来可考虑迁移到 WASI 目标，获得更好的系统调用支持
2. **批量处理**：对于大量数据处理，可考虑批量 API 减少跨 wasm 调用次数
3. **零拷贝优化**：探索 SharedArrayBuffer 实现零拷贝数据传递
4. **渐进式回退**：根据性能数据，可选择性地回退某些模块到 TS 实现
5. **类型生成**：考虑使用工具从 MoonBit 类型自动生成 TypeScript 类型声明

---

## 变更文件清单

### 新建文件（23 个）

| 文件 | 用途 |
|------|------|
| `moon.mod.json` | MoonBit 模块定义 |
| `moon.pkg.json` | 根包配置 |
| `src/string_utils.mbt` | 共享字符串工具 |
| `src/routine_variables.mbt` | routine-variables 实现 |
| `src/project_mentions.mbt` | project-mentions 实现 |
| `src/log_redaction.mbt` | log-redaction 实现 |
| `src/session_compaction.mbt` | session-compaction 实现 |
| `src_test/routine_variables_test.mbt` | routine-variables 测试 |
| `src_test/routine_variables_bench.mbt` | routine-variables 基准 |
| `src_test/project_mentions_test.mbt` | project-mentions 测试 |
| `src_test/project_mentions_bench.mbt` | project-mentions 基准 |
| `src_test/log_redaction_test.mbt` | log-redaction 测试 |
| `src_test/log_redaction_bench.mbt` | log-redaction 基准 |
| `src_test/session_compaction_test.mbt` | session-compaction 测试 |
| `src_test/session_compaction_bench.mbt` | session-compaction 基准 |
| `wasm_loader/loader.ts` | wasm 加载器 |
| `wasm_loader/bindings.ts` | 类型声明 |
| `wasm_loader/memory.ts` | 内存管理 |
| `wasm_loader/init.ts` | 初始化入口 |
| `bridge/wasm_bridge.ts` | 统一桥接层 |
| `benchmark/*.bench.ts` (4 个) | 性能基准对比 |
| `docs/migration/overview.md` | 迁移概述 |
| `docs/migration/performance-report.md` | 性能报告 |

### 修改文件（1 个）

| 文件 | 变更 |
|------|------|
| `package.json` | 添加 wasm 相关 scripts |

### 参考文件（不变）

| 文件 | 用途 |
|------|------|
| `paperclip/packages/shared/src/routine-variables.ts` | 原始实现参考 |
| `paperclip/packages/shared/src/project-mentions.ts` | 原始实现参考 |
| `paperclip/packages/adapter-utils/src/log-redaction.ts` | 原始实现参考 |
| `paperclip/packages/adapter-utils/src/session-compaction.ts` | 原始实现参考 |
