# MoonBit wasm-gc 模块

独立的高性能计算模块，可单独使用或与 paperclip 项目集成。

## 模块

| 模块 | 描述 | 测试数 |
|------|------|--------|
| `routine_variables.mbt` | 模板变量引擎，提取和插值 `{{variable}}` 模式 | 13 ✅ |
| `project_mentions.mbt` | mention 协议解析，支持 project/agent/skill 类型 | 18 ✅ |
| `log_redaction.mbt` | 日志脱敏，处理 macOS/Linux/Windows 路径 | 7 ✅ |
| `session_compaction.mbt` | 会话压缩策略计算和合并 | 8 ✅ |
| `string_utils.mbt` | 共享字符串工具（查找、前缀检查、trim） | - |

## 构建

```bash
moon check              # 类型检查（快速）
moon test               # 运行所有测试
moon build --target wasm-gc  # 编译 wasm 模块
moon fmt                # 格式化代码
moon info               # 生成 API 接口文件
```

## 项目结构

```
src/
├── routine_variables.mbt      # 模板变量处理
├── routine_variables_test.mbt # 测试
├── project_mentions.mbt       # mention 协议
├── project_mentions_test.mbt  # 测试
├── log_redaction.mbt          # 日志脱敏
├── log_redaction_test.mbt     # 测试
├── session_compaction.mbt     # 会话压缩
├── session_compaction_test.mbt # 测试
└── string_utils.mbt           # 共享工具
```

## 设计决策

### 无 FFI 依赖

所有字符串处理逻辑在 wasm 内部完成，不依赖 JavaScript 运行时 API：

- ❌ 无 `RegExp` → ✅ 手动字符串扫描
- ❌ 无 `URL` → ✅ 手动协议解析
- ❌ 无 `toLowerCase()` → ✅ 手动 ASCII 转换

这确保了 wasm 模块的独立性和可移植性。

### 测试策略

每个模块有完整的单元测试：

```moonbit
test "extract_variables single variable" {
  let result = extract_variables("{{name}}")
  let expected: Array[String] = ["name"]
  @debug.assert_eq(result, expected)
}
```

运行测试：
```bash
moon test
```

## 与 TypeScript 集成

### wasm 加载

```typescript
import { initializeWasmModules } from './wasm_loader/init.js';

// 应用启动时
await initializeWasmModules();
```

### 通过 Bridge 层调用

```typescript
import { routineVariables } from './bridge/wasm_bridge.js';

// 自动选择 wasm 或 TS 实现
const vars = await routineVariables.extractVariables("{{name}} and {{action}}");
console.log(vars);  // ["name", "action"]
```

## 文档测试

代码示例可以在文档中测试：

```mbt check
test "extract_variables works" {
  let result = extract_variables("{{name}}")
  assert_eq(result.length(), 1)
}
```

## TypeScript 参考代码

原始 TypeScript 实现的本地副本位于 `lib/` 目录中，作为：
- 参考实现
- 降级方案
- 性能对比基准

## License

MIT
