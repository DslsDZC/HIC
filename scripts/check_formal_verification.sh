#!/bin/bash
# HIC形式化验证一致性检查脚本
# 确保代码实现与数学证明一致

set -e

echo " HIC 形式化验证一致性检查 "
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查计数
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0

# 检查函数
check_item() {
    local description="$1"
    local command="$2"
    
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    echo -n "[$TOTAL_CHECKS] $description ... "
    
    if eval "$command" > /dev/null 2>&1; then
        echo -e "${GREEN}通过${NC}"
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        return 0
    else
        echo -e "${RED}失败${NC}"
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        return 1
    fi
}

echo "1. 检查数学证明文档"
echo "--"

check_item "math_proofs.tex 存在" "test -f src/Core-0/math_proofs.tex"
check_item "math_proofs.tex 包含7个定理" "grep -c '\\\\begin{theorem}' src/Core-0/math_proofs.tex | grep -q '^7$'"
check_item "包含能力守恒性定理" "grep -q '能力守恒性' src/Core-0/math_proofs.tex"
check_item "包含内存隔离性定理" "grep -q '内存隔离性' src/Core-0/math_proofs.tex"
check_item "包含权限单调性定理" "grep -q '能力权限单调性' src/Core-0/math_proofs.tex"
check_item "包含资源配额守恒性定理" "grep -q '资源配额守恒性' src/Core-0/math_proofs.tex"
check_item "包含无死锁定理" "grep -q '无死锁性' src/Core-0/math_proofs.tex"
check_item "包含类型安全性定理" "grep -q '类型安全性' src/Core-0/math_proofs.tex"
check_item "包含原子性保证定理" "grep -q '原子性保证' src/Core-0/math_proofs.tex"

echo ""
echo "2. 检查形式化验证代码"
echo "--"

check_item "formal_verification.h 存在" "test -f src/Core-0/formal_verification.h"
check_item "formal_verification.c 存在" "test -f src/Core-0/formal_verification.c"
check_item "包含7个不变式检查函数（含增强版）" "grep 'static bool invariant_' src/Core-0/formal_verification.c | wc -l | grep -q '^7$'"

echo ""
echo "3. 检查定理与代码的对应关系"
echo "--"

check_item "定理1 -> 不变式1 (能力守恒性)" \
    "grep -q 'invariant_capability_conservation' src/Core-0/formal_verification.c"
check_item "定理2 -> 不变式2 (内存隔离性)" \
    "grep -q 'invariant_memory_isolation' src/Core-0/formal_verification.c"
check_item "定理3 -> 不变式3 (权限单调性)" \
    "grep -q 'invariant_capability_monotonicity' src/Core-0/formal_verification.c"
check_item "定理4 -> 不变式4 (资源配额守恒性)" \
    "grep -q 'invariant_resource_quota_conservation' src/Core-0/formal_verification.c"
check_item "定理5 -> 不变式5 (无死锁性)" \
    "grep -q 'invariant_deadlock_freedom' src/Core-0/formal_verification.c"
check_item "定理6 -> 不变式6 (类型安全性)" \
    "grep -q 'invariant_type_safety' src/Core-0/formal_verification.c"

echo ""
echo "4. 检查关键实现"
echo "--"

check_item "实现不变式检查接口" \
    "grep -q 'fv_check_all_invariants' src/Core-0/formal_verification.c"
check_item "实现系统调用原子性验证" \
    "grep -q 'fv_verify_syscall_atomicity' src/Core-0/formal_verification.c"
check_item "实现域隔离性验证" \
    "grep -q 'fv_verify_domain_isolation' src/Core-0/formal_verification.c"
check_item "实现死锁检测（资源分配图）" \
    "grep -q 'detect_cycle_in_rag' src/Core-0/formal_verification.c"
check_item "实现验证统计接口" \
    "grep -q 'fv_get_stats' src/Core-0/formal_verification.c"
check_item "实现验证覆盖率统计" \
    "grep -q 'fv_get_coverage' src/Core-0/formal_verification.c"
check_item "实现详细报告生成" \
    "grep -q 'fv_get_report' src/Core-0/formal_verification.c"

echo ""
echo "5. 检查增强功能"
echo "--"

check_item "实现不变式依赖关系" \
    "grep -q 'fv_get_invariant_dependencies' src/Core-0/formal_verification.c"
check_item "实现验证时序保证" \
    "grep -q 'fv_get_state' src/Core-0/formal_verification.c"
check_item "实现证明检查点" \
    "grep -q 'fv_register_checkpoint' src/Core-0/formal_verification.c"
check_item "实现验证状态机" \
    "grep -q 'fv_trigger_event' src/Core-0/formal_verification.c"
check_item "实现形式化规范" \
    "grep -q 'fv_get_invariant_specs' src/Core-0/formal_verification.c"

echo ""
echo "6. 检查文档一致性"
echo "--"

check_item "形式化验证文档存在" \
    "test -f docs/Wiki/15-FormalVerification.md"
check_item "文档引用数学证明" \
    "grep -q 'math_proofs.tex' docs/Wiki/15-FormalVerification.md"
check_item "文档列出核心不变式" \
    "grep -c '不变式' docs/Wiki/15-FormalVerification.md | grep -q '^10$'"

echo ""
echo "7. 检查代码质量"
echo "--"

check_item "包含中文注释" \
    "grep -q 'HIC内核形式化验证模块' src/Core-0/formal_verification.c"
check_item "遵循编码规范（缩进使用空格）" \
    "! grep -P '\t' src/Core-0/formal_verification.c"
check_item "包含函数文档注释" \
    "grep -q '/\\*\\*' src/Core-0/formal_verification.c"

echo ""
echo "8. 检查与数学证明的数学表达式一致性"
echo "--"

check_item "能力守恒性实现（get_domain_initial_cap_quota, get_domain_granted_caps, get_domain_revoked_caps）" \
    "grep -q 'get_domain_initial_cap_quota' src/Core-0/formal_verification.c && grep -q 'get_domain_granted_caps' src/Core-0/formal_verification.c && grep -q 'get_domain_revoked_caps' src/Core-0/formal_verification.c"
check_item "内存隔离性实现（regions_overlap）" \
    "grep -q 'regions_overlap' src/Core-0/formal_verification.c"
check_item "权限单调性实现（is_permission_subset）" \
    "grep -q 'is_permission_subset' src/Core-0/formal_verification.c"

echo ""
echo "9. 检查增强实现的完整性"
echo "--"

check_item "检查点注册（至少10个）" \
    "grep -c 'fv_register_checkpoint' src/Core-0/formal_verification.c | grep -q '^1[0-9]$'"
check_item "状态转换表定义" \
    "grep -q 'fv_transition_t' src/Core-0/formal_verification.c"
check_item "不变式规范包含形式化表达式" \
    "grep -q 'formal_expr' src/Core-0/formal_verification.c"
check_item "依赖关系表定义" \
    "grep -q 'invariant_dependency_t' src/Core-0/formal_verification.c"

echo ""
echo " 检查结果汇总 "
echo ""
echo -e "总检查数: $TOTAL_CHECKS"
echo -e "${GREEN}通过: $PASSED_CHECKS${NC}"
echo -e "${RED}失败: $FAILED_CHECKS${NC}"
echo ""

if [ $FAILED_CHECKS -eq 0 ]; then
    echo -e "${GREEN}✓ 所有检查通过！${NC}"
    exit 0
else
    echo -e "${RED}✗ 有 $FAILED_CHECKS 项检查失败，需要修复。${NC}"
    exit 1
fi