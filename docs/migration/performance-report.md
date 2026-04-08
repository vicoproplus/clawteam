# MoonBit wasm-gc 迁移性能报告

## 测试环境

- Node.js 版本: v20.x
- CPU: [待补充]
- 内存: [待补充]
- MoonBit 版本: [待补充]
- 测试日期: 2026-04-08

## 测试方法

每个模块使用以下基准测试：

```typescript
const iterations = 10000;

// TypeScript 版本
console.time('TypeScript');
for (let i = 0; i < iterations; i++) {
  tsFunction(input);
}
console.timeEnd('TypeScript');

// wasm 版本
console.time('wasm');
for (let i = 0; i < iterations; i++) {
  wasmFunction(input);
}
console.timeEnd('wasm');
```

## routine-variables

**测试输入**: `"{{a}} {{b}} {{c}} {{d}} {{e}} {{f}} {{g}} {{h}} {{i}} {{j}}"`

| 实现 | 执行时间 (ms) | 相对性能 |
|------|---------------|----------|
| TypeScript | [待测量] | 1.0x |
| MoonBit wasm-gc | [待测量] | [待测量] |

**分析**: 模板变量提取涉及字符串扫描和数组操作，wasm 的线性内存访问可能带来性能优势。

## project-mentions

**测试输入**: `"project://proj-123 agent://agent-456 skill://skill-789"`（重复 100 次）

| 实现 | 执行时间 (ms) | 相对性能 |
|------|---------------|----------|
| TypeScript | [待测量] | 1.0x |
| MoonBit wasm-gc | [待测量] | [待测量] |

**分析**: mention 解析涉及大量字符串匹配和颜色规范化，wasm 中手动实现避免了 RegExp 开销。

## log-redaction

**测试输入**: 包含多个 `/Users/xxx/` 和 `/home/xxx/` 路径的长文本（10KB）

| 实现 | 执行时间 (ms) | 相对性能 |
|------|---------------|----------|
| TypeScript | [待测量] | 1.0x |
| MoonBit wasm-gc | [待测量] | [待测量] |

**分析**: 路径脱敏涉及多次字符串替换，wasm 中手动扫描替代 RegExp 可能有性能优势。

## session-compaction

**测试输入**: 策略解析 10000 次

| 实现 | 执行时间 (ms) | 相对性能 |
|------|---------------|----------|
| TypeScript | [待测量] | 1.0x |
| MoonBit wasm-gc | [待测量] | [待测量] |

**分析**: 纯数值计算和策略决策，wasm 直接计算无序列化开销，预期性能提升显著。

## 总结

### 总体性能对比

| 模块 | 性能提升 | 推荐迁移 |
|------|----------|----------|
| routine-variables | [待确定] | ✅ |
| project-mentions | [待确定] | ✅ |
| log-redaction | [待确定] | ✅ |
| session-compaction | [待确定] | ✅ |

### 关键发现

1. **字符串处理**：MoonBit 手动实现相比 TS RegExp 在简单模式下可能更快
2. **数值计算**：wasm 直接计算，无类型转换开销
3. **序列化开销**：数据进出 wasm 需要序列化，可能抵消计算收益
4. **冷启动时间**：wasm 加载和实例化有固定开销

### 建议

- 对于高频调用路径，wasm 迁移值得
- 对于低频调用，TS 实现可能更简单高效
- 建议在实际生产环境中测量后再做决策
