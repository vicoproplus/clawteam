# MoonBit wasm-gc 模块 - 独立项目

## 概述

这是一个独立的 MoonBit wasm-gc 项目，包含 4 个高性能计算模块。

## 模块

| 模块 | MoonBit 路径 | TypeScript 参考 |
|------|--------------|-----------------|
| routine-variables | `src/routine_variables.mbt` | `lib/shared/routine-variables.ts` |
| project-mentions | `src/project_mentions.mbt` | `lib/shared/project-mentions.ts` |
| log-redaction | `src/log_redaction.mbt` | `lib/adapter-utils/log-redaction.ts` |
| session-compaction | `src/session_compaction.mbt` | `lib/adapter-utils/session-compaction.ts` |

**总计：46 个测试全部通过 ✅**

## 架构设计

```
clawteam/
├── lib/                           # TypeScript 参考代码（本地副本）
│   ├── shared/
│   │   ├── routine-variables.ts
│   │   ├── project-mentions.ts
│   │   └── types/
│   └── adapter-utils/
│       ├── log-redaction.ts
│       ├── session-compaction.ts
│       └── types.ts
│
├── src/                           # MoonBit 源码
│   ├── routine_variables.mbt
│   ├── project_mentions.mbt
│   ├── log_redaction.mbt
│   ├── session_compaction.mbt
│   ├── string_utils.mbt           # 共享工具
│   └── *_test.mbt                 # 测试文件
│
├── wasm_loader/                   # Node.js wasm 加载层
│   ├── loader.ts                  # wasm 加载器
│   ├── memory.ts                  # 内存管理
│   ├── bindings.ts                # 类型声明
│   └── init.ts                    # 初始化入口
│
├── bridge/                        # 桥接层
│   └── wasm_bridge.ts             # 统一 API，wasm/TS 自动切换
│
└── target/wasm-gc/release/        # 编译输出
    └── bundle/moonbit-wasm.wasm
```

## 核心设计原则

1. **渐进式迁移**：从最简单模块开始，逐个验证，保留 TS fallback
2. **零 FFI 依赖**：所有字符串处理在 wasm 内部完成，避免跨调用开销
3. **测试驱动**：每个模块有对比测试，验证与 TS 输出一致
4. **透明替换**：调用方无需关心底层实现，bridge 层自动切换
5. **本地副本**：`./lib/` 目录包含 TypeScript 参考代码的本地副本

## 构建命令

```bash
# MoonBit 构建
moon check              # 类型检查（快速）
moon test               # 运行所有测试
moon build --target wasm-gc  # 编译 wasm 模块
moon fmt                # 格式化代码

# TypeScript 构建（如果配置了）
npm run wasm:build      # 编译 wasm
npm run wasm:check      # 类型检查
npm run wasm:test       # 运行测试
npm run wasm:benchmark  # 性能基准测试
```

## 性能对比

| 模块 | TypeScript | MoonBit wasm-gc | 性能提升 |
|------|------------|-----------------|----------|
| routine-variables | 基准 | 待测量 | 待测量 |
| project-mentions | 基准 | 待测量 | 待测量 |
| log-redaction | 基准 | 待测量 | 待测量 |
| session-compaction | 基准 | 待测量 | 待测量 |

*注：性能数据需要在实际环境中运行基准测试获取*

## 已知限制

1. **无 RegExp 支持**：MoonBit wasm-gc 不支持原生 RegExp，所有正则逻辑用手动字符串扫描替代
2. **无 URL API**：手动实现 URL 解析逻辑
3. **无 toLowerCase**：手动实现 ASCII 小写转换
4. **const struct 不支持**：用函数替代常量

## 后续改进建议

1. **WASI 支持**：未来可考虑迁移到 WASI 目标，获得更好的系统调用支持
2. **批量处理**：对于大量数据处理，可考虑批量 API 减少跨 wasm 调用次数
3. **零拷贝优化**：探索 SharedArrayBuffer 实现零拷贝数据传递
4. **类型生成**：考虑使用工具从 MoonBit 类型自动生成 TypeScript 类型声明
5. **性能基准测试**：创建完整的基准测试套件，对比 TS vs wasm 性能
