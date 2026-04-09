#!/bin/bash
# MoonBit 后端迁移验证脚本
# 用法: ./scripts/validate-moonbit-migration.sh

set -e

echo "========================================="
echo "MoonBit 后端迁移验证"
echo "========================================="
echo ""

# 1. 验证 MoonBit 构建
echo "1/5 验证 MoonBit 构建..."
cd moonbit-core
if moon build; then
  echo "   ✅ MoonBit 构建成功"
else
  echo "   ❌ MoonBit 构建失败"
  exit 1
fi
echo ""

# 2. 运行测试
echo "2/5 运行 MoonBit 测试..."
if moon test; then
  echo "   ✅ 所有测试通过"
else
  echo "   ❌ 测试失败"
  exit 1
fi
echo ""

# 3. 类型检查
echo "3/5 类型检查..."
if moon check; then
  echo "   ✅ 类型检查通过"
else
  echo "   ❌ 类型检查失败"
  exit 1
fi
echo ""

# 4. 格式化检查
echo "4/5 检查代码格式..."
if moon fmt; then
  echo "   ✅ 代码格式化完成"
else
  echo "   ❌ 格式化失败"
  exit 1
fi
echo ""

# 5. 生成 API 接口文件
echo "5/5 生成 API 接口文件..."
if moon info; then
  echo "   ✅ API 接口文件生成成功"
else
  echo "   ❌ API 接口文件生成失败"
  exit 1
fi
echo ""

cd ..

echo "========================================="
echo "✅ 所有验证通过!"
echo "========================================="
echo ""
echo "文件统计:"
echo "  MoonBit 文件: $(find moonbit-core/src -name '*.mbt' | wc -l)"
echo "  测试文件: $(find moonbit-core/src -name '*_test.mbt' | wc -l)"
echo "  TypeScript 集成文件: $(find server/src/services -name 'moonbit-*.ts' | wc -l)"
echo ""
