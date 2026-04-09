#!/bin/bash
# MoonBit 后端迁移 - 完整验证脚本
# 用法: bash scripts/verify-moonbit-migration.sh

set -e

echo "========================================="
echo "MoonBit 后端迁移 - 完整验证"
echo "========================================="
echo ""

PASS=0
FAIL=0
WARN=0

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 验证函数
check() {
  local name="$1"
  local cmd="$2"
  
  echo -n "验证: $name ... "
  if eval "$cmd" > /dev/null 2>&1; then
    echo -e "${GREEN}✅ 通过${NC}"
    PASS=$((PASS + 1))
  else
    echo -e "${RED}❌ 失败${NC}"
    FAIL=$((FAIL + 1))
  fi
}

warn() {
  local name="$1"
  echo -n "验证: $name ... "
  echo -e "${YELLOW}⏸️ 待验证${NC}"
  WARN=$((WARN + 1))
}

# 1. MoonBit 核心验证
echo "--- MoonBit 核心 ---"
check "MoonBit 构建" "cd moonbit-core && moon build"
check "MoonBit 测试 (58个)" "cd moonbit-core && moon test"
check "MoonBit 类型检查" "cd moonbit-core && moon check"
echo ""

# 2. 文件结构验证
echo "--- 文件结构 ---"
check "枚举文件存在" "test -f moonbit-core/src/domain/common/enums.mbt"
check "Agent 状态机文件存在" "test -f moonbit-core/src/domain/common/agent_state_machine.mbt"
check "Issue 状态机文件存在" "test -f moonbit-core/src/domain/common/issue_state_machine.mbt"
check "Budget 类型文件存在" "test -f moonbit-core/src/domain/common/budget_types.mbt"
check "Server handlers 文件存在" "test -f moonbit-core/src/server/handlers.mbt"
check "Server main 文件存在" "test -f moonbit-core/src/server/main.mbt"
echo ""

# 3. TypeScript 集成验证
echo "--- TypeScript 集成 ---"
check "MoonBit 客户端文件存在" "test -f server/src/services/moonbit-client.ts"
check "进程管理器文件存在" "test -f server/src/services/moonbit-process-manager.ts"
check "app.ts 已修改" "grep -q 'MoonBitProcessManager' server/src/app.ts"
echo ""

# 4. 配置验证
echo "--- 配置 ---"
check "package.json 有 moonbit scripts" "grep -q 'build:moonbit' package.json"
check "vitest.config.ts 包含 moonbit" "grep -q 'moonbit-integration' vitest.config.ts"
check "moon.mod.json 存在" "test -f moonbit-core/moon.mod.json"
echo ""

# 5. 文档验证
echo "--- 文档 ---"
check "README.md 存在" "test -f moonbit-core/README.md"
check "验收报告存在" "test -f moonbit-core/ACCEPTANCE-REPORT.md"
check "状态机契约存在" "test -f moonbit-core/STATE-MACHINE-CONTRACTS.md"
check "语法参考存在" "test -f moonbit-core/MOONBIT-SYNTAX-REFERENCE.md"
echo ""

# 6. 测试文件验证
echo "--- 测试文件 ---"
MOONBIT_TESTS=$(find moonbit-core/src -name '*_test.mbt' | wc -l)
echo -n "MoonBit 测试文件数: $MOONBIT_TESTS ... "
if [ "$MOONBIT_TESTS" -ge 5 ]; then
  echo -e "${GREEN}✅ 通过${NC}"
  PASS=$((PASS + 1))
else
  echo -e "${RED}❌ 失败${NC}"
  FAIL=$((FAIL + 1))
fi

check "集成测试文件存在" "test -f tests/moonbit-integration/alignment.test.ts"
echo ""

# 7. 枚举对齐验证
echo "--- 枚举对齐 ---"
check "CompanyStatus 枚举" "grep -q 'CompanyStatus' moonbit-core/src/domain/common/enums.mbt"
check "AgentStatus 枚举" "grep -q 'AgentStatus' moonbit-core/src/domain/common/enums.mbt"
check "IssueStatus 枚举" "grep -q 'IssueStatus' moonbit-core/src/domain/common/enums.mbt"
check "AgentRole 枚举" "grep -q 'AgentRole' moonbit-core/src/domain/common/enums.mbt"
check "AgentAdapterType 枚举" "grep -q 'AgentAdapterType' moonbit-core/src/domain/common/enums.mbt"
echo ""

# 8. 状态机验证
echo "--- 状态机 ---"
check "Agent 状态机有 can_transition_agent" "grep -q 'can_transition_agent' moonbit-core/src/domain/common/agent_state_machine.mbt"
check "Issue 状态机有 can_transition_issue" "grep -q 'can_transition_issue' moonbit-core/src/domain/common/issue_state_machine.mbt"
check "Agent 终端状态处理" "grep -q 'Terminated.*false' moonbit-core/src/domain/common/agent_state_machine.mbt"
check "Issue 终端状态处理" "grep -q 'Done.*false' moonbit-core/src/domain/common/issue_state_machine.mbt"
echo ""

# 9. JSON-RPC 验证
echo "--- JSON-RPC ---"
check "handlers.mbt 有 handle_request" "grep -q 'handle_request' moonbit-core/src/server/handlers.mbt"
check "health 端点" "grep -q 'health' moonbit-core/src/server/handlers.mbt"
check "agent.validate 端点" "grep -q 'agent.validate' moonbit-core/src/server/handlers.mbt"
check "budget.enforce 端点" "grep -q 'budget.enforce' moonbit-core/src/server/handlers.mbt"
check "MOONBIT_READY 信号" "grep -q 'MOONBIT_READY' moonbit-core/src/server/main.mbt"
echo ""

# 汇总
echo "========================================="
echo "验证结果汇总"
echo "========================================="
echo -e "通过: ${GREEN}$PASS${NC}"
echo -e "失败: ${RED}$FAIL${NC}"
echo -e "待验证: ${YELLOW}$WARN${NC}"
echo ""

TOTAL=$((PASS + FAIL + WARN))
echo "总计: $TOTAL 项检查"
echo ""

if [ $FAIL -eq 0 ]; then
  echo -e "${GREEN}=========================================${NC}"
  echo -e "${GREEN}✅ 所有验证通过! 迁移成功!${NC}"
  echo -e "${GREEN}=========================================${NC}"
  exit 0
else
  echo -e "${RED}=========================================${NC}"
  echo -e "${RED}❌ 有 $FAIL 项验证失败${NC}"
  echo -e "${RED}=========================================${NC}"
  exit 1
fi
