# 开发证据链追踪规范

> 本文档定义 HIC 内核开发证据链追踪机制，满足 EAL7 对开发过程可追溯性的要求。

---

## 1. 证据链概述

### 1.1 EAL7 证据要求

Common Criteria EAL7 要求所有开发活动都有可追溯的形式化证据：

```
需求分析 → 形式化规范 → 形式化设计 → 实现 → 测试 → 运行
    ↓          ↓            ↓          ↓       ↓       ↓
  REQ-001   SPEC-001     DESIGN-001  CODE-001 TEST-001 AUDIT-001
```

### 1.2 证据类型

| 证据类型 | 描述 | 存储位置 |
|---------|------|---------|
| **需求证据** | 需求文档、用例图、安全目标 | `docs/requirements/` |
| **规范证据** | 形式化规范、抽象模型 | `formal/*.thy` |
| **设计证据** | 详细设计文档、架构图 | `docs/design/` |
| **实现证据** | 源代码、代码审查记录 | `src/`, `.code_review/` |
| **测试证据** | 测试用例、覆盖率报告 | `tests/`, `.coverage/` |
| **配置证据** | 版本控制、构建记录 | `.git/`, `build.log` |
| **审计证据** | 审计日志、安全事件记录 | `audit/` |

---

## 2. 需求追溯

### 2.1 需求标识符格式

```
REQ-[模块]-[类型]-[编号]

模块: CAP (能力), DOM (域), SCH (调度), MEM (内存), AUD (审计), SEC (安全)
类型: F (功能), S (安全), P (性能), I (接口)

示例:
REQ-CAP-F-001: 能力授予功能
REQ-CAP-S-001: 能力不可伪造性
REQ-DOM-S-001: 域内存隔离性
```

### 2.2 需求追踪矩阵

| 需求ID | 需求描述 | 形式化规范 | 设计文档 | 代码实现 | 测试用例 |
|--------|---------|-----------|---------|---------|---------|
| REQ-CAP-F-001 | 能力授予 | `cap_grant` in `HIC_Capability_System.thy` | `docs/design/capability.md` | `capability.c:cap_grant` | `tests/cap_grant_test.c` |
| REQ-CAP-S-001 | 能力不可伪造 | `no_privilege_escalation` theorem | `docs/design/security.md` | `capability.c:cap_fast_check` | `tests/security_test.c` |
| REQ-DOM-S-001 | 域内存隔离 | `memory_isolated` invariant | `docs/design/domain.md` | `domain.c:domain_create` | `tests/isolation_test.c` |
| REQ-SCH-F-001 | 调度公平性 | `fair_no_starvation` theorem | `docs/design/scheduler.md` | `scheduler.c:schedule` | `tests/scheduler_test.c` |

### 2.3 需求追溯文件格式

```yaml
# requirements_trace.yaml
requirements:
  - id: REQ-CAP-F-001
    description: "能力授予功能"
    type: functional
    priority: high
    security_relevant: true
    traces:
      formal_spec:
        - file: "formal/HIC_Capability_System.thy"
          definition: "cap_grant"
          theorem: "invariant_preserved_after_grant"
      design:
        - file: "docs/design/capability.md"
          section: "4.2 能力授予"
      implementation:
        - file: "src/Core-0/capability.c"
          function: "cap_grant"
          lines: [355-420]
      tests:
        - file: "tests/cap_grant_test.c"
          case: "test_cap_grant_basic"
        - file: "tests/cap_grant_test.c"
          case: "test_cap_grant_permission_denied"
      code_reviews:
        - id: CR-2026-001
          date: 2026-03-27
          reviewers: ["alice", "bob"]
          result: approved
```

---

## 3. 代码审查证据

### 3.1 审查记录格式

```yaml
# .code_reviews/CR-2026-001.yaml
review:
  id: CR-2026-001
  date: 2026-03-27
  status: approved
  
change:
  commit: "19473ed02ecdf33abadb7f6dc5b9205d86e91e38"
  files:
    - "src/Core-0/capability.c"
    - "src/Core-0/capability.h"
  description: "能力系统安全增强"
  
reviewers:
  - name: alice
    role: security_expert
    focus: [security, formality]
    decision: approved
    comments:
      - file: "capability.c"
        line: 103
        text: "验证逻辑与 Isabelle 模型一致 ✓"
        
  - name: bob
    role: kernel_developer
    focus: [correctness, performance]
    decision: approved
    comments:
      - file: "capability.c"
        line: 128
        text: "性能符合要求 < 5ns ✓"

checklist:
  - item: "代码与形式化规范一致"
    result: pass
    evidence: "cap_fast_check 对应 HIC_Capability_System.thy L103-128"
    
  - item: "不变式检查完整"
    result: pass
    evidence: "fv_check_all_invariants 覆盖所有不变式"
    
  - item: "安全属性验证"
    result: pass
    evidence: "no_privilege_escalation 定理已证明"
    
  - item: "测试覆盖率充足"
    result: pass
    evidence: "覆盖率 95%，关键路径 100%"
```

### 3.2 审查追踪脚本

```python
#!/usr/bin/env python3
# scripts/track_code_review.py

import yaml
import git
import os
from datetime import datetime

def create_review_record(commit_hash, reviewers, checklist):
    """创建代码审查记录"""
    repo = git.Repo('.')
    commit = repo.commit(commit_hash)
    
    review_id = f"CR-{datetime.now().year}-{len(os.listdir('.code_reviews'))+1:03d}"
    
    record = {
        'review': {
            'id': review_id,
            'date': datetime.now().strftime('%Y-%m-%d'),
            'status': 'pending'
        },
        'change': {
            'commit': commit_hash,
            'files': [f.a_path for f in commit.stats.files.keys()],
            'description': commit.message
        },
        'reviewers': reviewers,
        'checklist': checklist,
        'formal_traces': extract_formal_traces(commit)
    }
    
    with open(f'.code_reviews/{review_id}.yaml', 'w') as f:
        yaml.dump(record, f)
    
    return review_id

def extract_formal_traces(commit):
    """提取提交中的形式化追溯信息"""
    traces = []
    for file in commit.stats.files:
        if file.endswith('.c') or file.endswith('.h'):
            # 解析代码注释中的形式化追溯
            # 例如: /* FORMAL_TRACE: REQ-CAP-F-001, cap_grant */
            pass
    return traces
```

---

## 4. 测试证据追踪

### 4.1 测试用例格式

```c
/**
 * 测试用例: TEST-CAP-GRANT-001
 * 
 * 追溯需求: REQ-CAP-F-001 (能力授予功能)
 * 追溯规范: HIC_Capability_System.thy:cap_grant
 * 追溯实现: capability.c:cap_grant
 * 
 * 测试目的: 验证基本能力授予功能
 * 
 * 前置条件:
 *   - 源域拥有有效能力
 *   - 目标域已创建
 * 
 * 测试步骤:
 *   1. 创建测试域
 *   2. 授予能力
 *   3. 验证授予成功
 * 
 * 预期结果:
 *   - 返回 HIC_SUCCESS
 *   - 目标域获得有效能力句柄
 *   - 不变式仍然成立
 */
void test_cap_grant_basic(void) {
    /* 测试实现 */
    domain_id_t src = create_test_domain();
    domain_id_t dst = create_test_domain();
    
    cap_id_t cap;
    TEST_ASSERT(HIC_SUCCESS == cap_create_memory(src, 0x1000000, 4096, 
                                                   CAP_READ | CAP_WRITE, &cap));
    
    cap_handle_t handle;
    TEST_ASSERT(HIC_SUCCESS == cap_grant(dst, cap, &handle));
    
    /* 验证不变式 */
    TEST_ASSERT(FV_SUCCESS == fv_check_all_invariants());
    
    /* 验证能力可用 */
    TEST_ASSERT(cap_fast_check(dst, handle, CAP_READ));
    
    /* 清理 */
    destroy_test_domain(src);
    destroy_test_domain(dst);
}
```

### 4.2 测试覆盖率追踪

```yaml
# .coverage/coverage_report.yaml
coverage:
  date: 2026-03-27
  tool: gcov
  version: 11.2.0
  
summary:
  total_lines: 15420
  covered_lines: 14720
  coverage_percent: 95.5
  
modules:
  - name: capability
    file: "src/Core-0/capability.c"
    coverage: 98.5
    formal_traced: true
    critical_paths:
      - function: "cap_fast_check"
        coverage: 100
        formal_trace: "HIC_Capability_System.thy:cap_fast_check"
      - function: "cap_grant"
        coverage: 100
        formal_trace: "HIC_Capability_System.thy:cap_grant"
        
  - name: domain
    file: "src/Core-0/domain.c"
    coverage: 94.2
    formal_traced: true
    
  - name: scheduler
    file: "src/Core-0/scheduler.c"
    coverage: 92.1
    formal_traced: true

requirements_coverage:
  - req_id: REQ-CAP-F-001
    tests:
      - TEST-CAP-GRANT-001
      - TEST-CAP-GRANT-002
      - TEST-CAP-GRANT-003
    coverage: 100
    
  - req_id: REQ-CAP-S-001
    tests:
      - TEST-CAP-SECURITY-001
      - TEST-CAP-SECURITY-002
    coverage: 100
```

---

## 5. 配置管理证据

### 5.1 版本控制记录

```yaml
# .git/formal_config.yaml
version_control:
  system: git
  repository: git@github.com:DslsDZC/hic.git
  
branch_policy:
  main:
    protection: true
    required_reviews: 2
    required_checks:
      - "Isabelle proofs complete"
      - "Test coverage > 90%"
      - "No compiler warnings"
  
  develop:
    protection: true
    required_reviews: 1

formal_artifacts:
  - path: "formal/*.thy"
    validation: "isabelle build"
    integrity_check: "sha256sum"
    
  - path: "formal/*.md"
    validation: "markdown lint"
    
commit_policy:
  - type: "feat"
    requires_formal_spec: true
    requires_tests: true
    
  - type: "fix"
    requires_formal_verification: true
    requires_regression_test: true
    
  - type: "security"
    requires_formal_proof: true
    requires_security_review: true
```

### 5.2 构建记录

```yaml
# build_records/build-2026-03-27-001.yaml
build:
  id: build-2026-03-27-001
  date: 2026-03-27
  time: "08:35:12"
  
environment:
  os: "Linux 6.19.9-zen1-1-zen"
  compiler: "gcc 13.2.0"
  isabelle: "Isabelle2023"
  
inputs:
  commit: "19473ed02ecdf33abadb7f6dc5b9205d86e91e38"
  config: "build_config.yaml"
  
outputs:
  - file: "build/bin/hic-kernel.elf"
    size: 28672
    sha256: "abc123..."
  - file: "build/bin/hic-kernel.hic"
    size: 19005600
    sha256: "def456..."
    
verification:
  isabelle:
    status: pass
    theorems: 156
    sorry_count: 0
    
  tests:
    status: pass
    total: 245
    passed: 245
    failed: 0
    coverage: 95.5
    
  formality_checks:
    - check: "invariant_capability_conservation"
      result: pass
    - check: "invariant_memory_isolation"
      result: pass
    - check: "invariant_capability_monotonicity"
      result: pass
      
signatures:
  builder: "build-bot"
  verifier: "alice"
  timestamp: "2026-03-27T08:45:00Z"
```

---

## 6. 安全审计证据

### 6.1 安全事件记录

```yaml
# audit/security_events.yaml
events:
  - id: SEC-EVT-001
    date: 2026-03-27
    type: "capability_check_failure"
    severity: "warning"
    
    context:
      domain: 5
      capability: 1024
      requested_rights: CAP_WRITE
      actual_rights: CAP_READ
      
    analysis:
      root_cause: "权限不足"
      is_security_issue: false
      follows_spec: true
      
    trace:
      requirement: REQ-CAP-S-001
      formal_spec: "no_unauthorized_access theorem"
      code_path: "capability.c:cap_fast_check"
      
  - id: SEC-EVT-002
    date: 2026-03-27
    type: "memory_isolation_violation_attempt"
    severity: "critical"
    
    context:
      source_domain: 5
      target_domain: 3
      attempted_address: 0x800000
      
    analysis:
      root_cause: "域5尝试访问域3的内存"
      is_security_issue: true
      follows_spec: false
      blocked_by: "MMU"
      
    trace:
      requirement: REQ-DOM-S-001
      formal_spec: "memory_isolated invariant"
      mitigation: "硬件MMU强制隔离"
```

### 6.2 审计追踪查询

```python
#!/usr/bin/env python3
# scripts/audit_trace.py

import yaml
import sys

def trace_event(event_id):
    """追踪安全事件的形式化证据链"""
    with open('audit/security_events.yaml') as f:
        events = yaml.safe_load(f)
    
    event = next((e for e in events['events'] if e['id'] == event_id), None)
    if not event:
        return None
    
    # 构建证据链
    evidence_chain = {
        'event': event,
        'formal_spec': load_formal_spec(event['trace']['formal_spec']),
        'requirement': load_requirement(event['trace']['requirement']),
        'code_path': analyze_code_path(event['trace']['code_path'])
    }
    
    return evidence_chain
```

---

## 7. 证据链完整性验证

### 7.1 完整性检查脚本

```python
#!/usr/bin/env python3
# scripts/verify_evidence_chain.py

import yaml
import os
import hashlib

def verify_chain():
    """验证证据链完整性"""
    errors = []
    
    # 1. 检查需求追溯完整性
    with open('requirements_trace.yaml') as f:
        traces = yaml.safe_load(f)
    
    for req in traces['requirements']:
        # 检查形式化规范存在
        for spec in req['traces'].get('formal_spec', []):
            if not os.path.exists(spec['file']):
                errors.append(f"Missing formal spec: {spec['file']}")
            else:
                # 验证定理存在
                if not check_theorem_exists(spec['file'], spec.get('theorem')):
                    errors.append(f"Missing theorem: {spec['theorem']}")
        
        # 检查实现存在
        for impl in req['traces'].get('implementation', []):
            if not os.path.exists(impl['file']):
                errors.append(f"Missing implementation: {impl['file']}")
        
        # 检查测试存在
        for test in req['traces'].get('tests', []):
            if not os.path.exists(test['file']):
                errors.append(f"Missing test: {test['file']}")
    
    # 2. 检查代码审查记录
    for review_file in os.listdir('.code_reviews'):
        with open(f'.code_reviews/{review_file}') as f:
            review = yaml.safe_load(f)
        # 验证审查完整性
        if review['review']['status'] != 'approved':
            errors.append(f"Unapproved review: {review_file}")
    
    # 3. 检查构建记录
    if not check_latest_build_success():
        errors.append("Latest build verification failed")
    
    return errors

def check_theorem_exists(file, theorem_name):
    """检查 Isabelle 定理是否存在且已证明"""
    with open(file) as f:
        content = f.read()
    if theorem_name and theorem_name not in content:
        return False
    if 'sorry' in content:
        return False
    return True
```

### 7.2 证据链报告

```yaml
# evidence_chain_report.yaml
report:
  date: 2026-03-27
  version: 1.0
  
completeness:
  requirements_traced: 45/45
  formal_specs_complete: 45/45
  implementations_traced: 45/45
  tests_traced: 45/45
  
formal_verification:
  theorems_total: 156
  theorems_proven: 156
  sorry_count: 0
  
coverage:
  code_coverage: 95.5%
  requirement_coverage: 100%
  formal_spec_coverage: 100%
  
integrity:
  all_files_hashed: true
  signatures_valid: true
  chain_complete: true
  
issues:
  - none
```

---

## 8. 自动化集成

### 8.1 Makefile 集成

```makefile
# Makefile 证据链目标

.PHONY: evidence-chain verify-chain evidence-report

evidence-chain:
	@echo "构建证据链..."
	@python scripts/collect_evidence.py
	@python scripts/verify_evidence_chain.py

verify-chain:
	@echo "验证证据链完整性..."
	@python scripts/verify_evidence_chain.py || (echo "证据链不完整!" && exit 1)

evidence-report:
	@echo "生成证据链报告..."
	@python scripts/generate_evidence_report.py > evidence_chain_report.yaml

# 提交前检查
pre-commit: verify-chain
	@echo "证据链验证通过"
```

### 8.2 Git 钩子集成

```bash
#!/bin/bash
# .git/hooks/pre-push

# 检查证据链完整性
make verify-chain || exit 1

# 检查所有证明完成
if grep -r "sorry" formal/*.thy; then
    echo "ERROR: 存在未完成的证明"
    exit 1
fi

# 检查测试覆盖率
if [ $(python scripts/get_coverage.py) -lt 90 ]; then
    echo "ERROR: 测试覆盖率低于 90%"
    exit 1
fi

echo "证据链验证通过"
```

---

*文档版本：1.0*
*最后更新：2026-03-27*
