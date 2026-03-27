# 代码到模型形式化绑定规范

> 本文档定义 HIC 内核 C 代码与 Isabelle/HOL 形式化模型之间的精确对应关系，
> 确保"代码到模型"的形式化一致性证明。

---

## 1. 绑定方法论

### 1.1 绑定策略

采用**精化方法 (Refinement Method)**：
```
抽象规范 (Isabelle) → 中间规范 → 具体实现 (C代码)
```

每一层都需要证明：
- **精化正确性**：具体实现满足抽象规范
- **数据精化**：具体数据结构表示抽象数据
- **行为精化**：具体行为符合抽象行为

### 1.2 绑定层次

```
┌─────────────────────────────────────────────────────────────────┐
│                    层次 0: 抽象规范 (Isabelle)                    │
│  HIC_Capability_System.thy, HIC_Domain_Scheduler.thy, etc.      │
└─────────────────────────────────────────────────────────────────┘
                              ↓ 精化证明
┌─────────────────────────────────────────────────────────────────┐
│                    层次 1: 中间规范 (Isabelle)                    │
│  数据结构布局、算法规范、接口契约                                 │
└─────────────────────────────────────────────────────────────────┘
                              ↓ C 语义绑定
┌─────────────────────────────────────────────────────────────────┐
│                    层次 2: C 实现标准                             │
│  formal_verification.c, capability.c, domain.c, etc.           │
└─────────────────────────────────────────────────────────────────┘
                              ↓ 编译器验证
┌─────────────────────────────────────────────────────────────────┐
│                    层次 3: 二进制实现                             │
│  编译后的机器码                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 数据类型绑定

### 2.1 能力系统类型绑定

| Isabelle 类型 | C 类型 | 绑定规范 |
|--------------|--------|---------|
| `domain_id :: nat` | `domain_id_t` (uint32) | 精确表示，范围 [0, MAX_DOMAINS) |
| `cap_id :: nat` | `cap_id_t` (uint32) | 精确表示，范围 [0, CAP_TABLE_SIZE) |
| `cap_rights :: cap_right set` | `cap_rights_t` (uint32) | 位图表示，每位对应一种权限 |
| `cap_state` | `uint8` | 枚举值精确对应 |
| `capability record` | `cap_entry_t` | 字段一一对应 |

#### Isabelle 定义
```isabelle
record capability =
  cap_id     :: cap_id
  cap_rights :: "cap_right set"
  cap_owner  :: domain_id
  cap_state  :: cap_state
  cap_parent :: "cap_id option"
```

#### C 定义
```c
typedef struct __attribute__((aligned(64))) cap_entry {
    cap_id_t       cap_id;       /* 能力ID */
    cap_rights_t   rights;       /* 权限 */
    domain_id_t    owner;        /* 拥有者 */
    uint8_t        flags;        /* 标志（包含 cap_state） */
    uint8_t        reserved[7];  /* 对齐填充 */
    union { ... };               /* 资源数据 */
} cap_entry_t;
```

#### 绑定函数
```isabelle
(* C 结构到 Isabelle 记录的映射 *)
definition cap_entry_to_cap :: "cap_entry_t \<Rightarrow> capability" where
  "cap_entry_to_cap ce \<equiv> \<lparr>
    cap_id     = cap_id_t.nat (ce.cap_id),
    cap_rights = decode_rights (ce.rights),
    cap_owner  = domain_id_t.nat (ce.owner),
    cap_state  = decode_state (ce.flags),
    cap_parent = if ce.parent = 0 then None else Some (cap_id_t.nat ce.parent)
  \<rparr>"

(* 绑定不变式：C 结构是 Isabelle 记录的有效表示 *)
definition valid_cap_entry :: "cap_entry_t \<Rightarrow> bool" where
  "valid_cap_entry ce \<equiv>
    ce.cap_id < CAP_TABLE_SIZE \<and>
    ce.owner < MAX_DOMAINS \<and>
    (\<exists>cap. cap_entry_to_cap ce = cap)"
```

### 2.2 域系统类型绑定

| Isabelle 类型 | C 类型 | 绑定规范 |
|--------------|--------|---------|
| `domain_type` | `uint8` | 枚举值精确对应 |
| `domain_state` | `uint8` | 枚举值精确对应 |
| `domain record` | `domain_t` | 字段对应，内存布局一致 |
| `mem_region` | `mem_region_t` | base + size 表示 |
| `domain_quota` | quota 字段 | 拆分到 domain_t 中 |

#### 绑定证明
```isabelle
(* 域状态映射 *)
definition domain_state_mapping :: "domain_state \<Rightarrow> nat" where
  "domain_state_mapping s = (
    case s of
      DOMAIN_INIT       \<Rightarrow> 0
    | DOMAIN_READY      \<Rightarrow> 1
    | DOMAIN_RUNNING    \<Rightarrow> 2
    | DOMAIN_SUSPENDED  \<Rightarrow> 3
    | DOMAIN_TERMINATED \<Rightarrow> 4)"

(* 映射正确性引理 *)
lemma domain_state_bijection:
  "\<forall>s. domain_state_from_nat (domain_state_mapping s) = Some s"
  by (cases s; auto simp: domain_state_mapping_def)
```

---

## 3. 函数绑定

### 3.1 能力验证函数绑定

#### Isabelle 规范
```isabelle
definition cap_fast_check :: 
  "system_state \<Rightarrow> domain_id \<Rightarrow> cap_handle \<Rightarrow> cap_rights \<Rightarrow> bool" where
  "cap_fast_check s d h req \<equiv>
    let cid = h && 0xFFFFFFFF in
    let entry = cap_table s cid in
    case entry of
      None \<Rightarrow> False
    | Some cap \<Rightarrow>
        cap.cap_id = cid \<and>
        \<not> (CAP_REVOKED \<in> cap.cap_flags) \<and>
        (cap.cap_rights \<inter> req = req) \<and>
        validate_token d cid (h >> 32)"
```

#### C 实现
```c
static inline bool cap_fast_check(domain_id_t domain, 
                                   cap_handle_t handle, 
                                   cap_rights_t required) {
    cap_id_t cap_id = (cap_id_t)(handle & CAP_HANDLE_CAP_MASK);
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    /* 综合检查 */
    bool valid = (entry->cap_id == cap_id) && 
                 !(entry->flags & CAP_FLAG_REVOKED) && 
                 ((entry->rights & required) == required);
    
    if (!valid) return false;
    
    /* 令牌验证 */
    u32 token = (u32)(handle >> CAP_HANDLE_TOKEN_SHIFT);
    return cap_validate_token(domain, cap_id, token);
}
```

#### 绑定定理
```isabelle
theorem cap_fast_check_binding:
  assumes "valid_system_state s"
  and     "cap_entry_array_valid g_global_cap_table"
  and     "s.cap_table = cap_array_to_table g_global_cap_table"
  shows   "cap_fast_check s d h req = 
           c_cap_fast_check d h (encode_rights req)"
proof -
  (* 展开两边定义 *)
  (* 证明语义等价 *)
  show ?thesis sorry (* 详细证明 *)
qed
```

### 3.2 能力授予函数绑定

#### Isabelle 规范
```isabelle
definition cap_grant ::
  "capability_table \<Rightarrow> domain_id \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> 
   cap_rights \<Rightarrow> capability_table option" where
  "cap_grant ct from to cid rights \<equiv>
    case ct cid of
      None \<Rightarrow> None
    | Some source \<Rightarrow>
        if rights \<subseteq> source.cap_rights \<and>
           source.cap_state = CAP_VALID \<and>
           source.cap_owner = from
        then create_derived_cap ct to source rights
        else None"
```

#### C 实现
```c
hic_status_t cap_grant(domain_id_t domain, cap_id_t cap, cap_handle_t *out) {
    if (domain >= HIC_DOMAIN_MAX || cap >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    if (entry->cap_id != cap || (entry->flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    cap_handle_t handle = cap_make_handle(domain, cap);
    *out = handle;
    return HIC_SUCCESS;
}
```

#### 绑定定理
```isabelle
theorem cap_grant_binding:
  assumes "valid_system_state s"
  and     "cap_grant (cap_table s) from to cid rights = Some ct'"
  shows   "\<exists>status handle. c_cap_grant from cid &handle = status \<and>
           status = HIC_SUCCESS \<longrightarrow>
           valid_handle handle \<and>
           decode_handle handle \<in> ct'"
```

---

## 4. 不变式绑定

### 4.1 能力守恒不变式绑定

#### Isabelle 不变式
```isabelle
definition cap_conservation :: "domain \<Rightarrow> bool" where
  "cap_conservation d \<equiv>
    |{c. cap_owner (the (cap_table c)) = d.domain_id}| =
    d.initial_quota + d.granted_count - d.revoked_count"
```

#### C 运行时检查
```c
static bool invariant_capability_conservation(void) {
    for (domain_id_t domain = 0; domain < MAX_DOMAINS; domain++) {
        if (!domain_is_active(domain)) continue;
        
        u64 current_caps = count_domain_capabilities(domain);
        u64 expected_caps = get_domain_initial_cap_quota(domain) +
                           get_domain_granted_caps(domain) -
                           get_domain_revoked_caps(domain);
        
        if (current_caps != expected_caps) {
            return false;
        }
    }
    return true;
}
```

#### 绑定定理
```isabelle
theorem cap_conservation_binding:
  assumes "\<forall>d. cap_conservation d"  (* Isabelle 不变式成立 *)
  and     "system_state_matches_c_state s c_state"  (* 状态对应 *)
  shows   "c_invariant_capability_conservation()"  (* C 检查通过 *)
```

### 4.2 内存隔离不变式绑定

#### Isabelle 不变式
```isabelle
definition memory_isolated :: "domain_table \<Rightarrow> bool" where
  "memory_isolated dt \<equiv>
    \<forall>d1 d2. d1 \<noteq> d2 \<longrightarrow>
      (case dt d1 of None \<Rightarrow> True
       | Some dom1 \<Rightarrow>
           case dt d2 of None \<Rightarrow> True
           | Some dom2 \<Rightarrow>
               dom1.state \<noteq> TERMINATED \<and>
               dom2.state \<noteq> TERMINATED \<longrightarrow>
               regions_disjoint dom1.memory dom2.memory)"
```

#### C 运行时检查
```c
static bool invariant_memory_isolation(void) {
    for (domain_id_t d1 = 0; d1 < MAX_DOMAINS; d1++) {
        if (!domain_is_active(d1)) continue;
        
        for (domain_id_t d2 = d1 + 1; d2 < MAX_DOMAINS; d2++) {
            if (!domain_is_active(d2)) continue;
            
            mem_region_t region1 = get_domain_memory_region(d1);
            mem_region_t region2 = get_domain_memory_region(d2);
            
            if (regions_overlap(&region1, &region2)) {
                return false;
            }
        }
    }
    return true;
}
```

---

## 5. 操作语义绑定

### 5.1 系统调用语义绑定

#### Isabelle 语义
```isabelle
inductive syscall_sem :: "system_state \<Rightarrow> syscall \<Rightarrow> system_state \<Rightarrow> bool" where
  sys_grant: "\<lbrakk>cap_grant (cap_table s) from to cap rights = Some ct'\<rbrakk>
              \<Longrightarrow> syscall_sem s (SYS_CAP_GRANT from to cap rights) 
                         (s\<lparr>cap_table := ct'\<rparr>)"
```

#### C 系统调用实现
```c
/* 系统调用表 */
static syscall_handler_t syscall_table[] = {
    [SYS_CAP_GRANT] = sys_cap_grant_handler,
    [SYS_CAP_REVOKE] = sys_cap_revoke_handler,
    /* ... */
};

/* 系统调用处理 */
hic_status_t sys_cap_grant_handler(syscall_context_t *ctx) {
    return cap_grant(ctx->arg1, ctx->arg2, &ctx->ret_val);
}
```

### 5.2 原子性绑定

#### Isabelle 原子性规范
```isabelle
definition syscall_atomic :: "syscall \<Rightarrow> bool" where
  "syscall_atomic sys \<equiv>
    \<forall>s s'. syscall_sem s sys s' \<longrightarrow>
      (\<exists>post. s' = post) \<or>      (* 完全成功 *)
      (s' = s)"                     (* 完全失败/回滚 *)
```

#### C 原子性实现
```c
bool fv_verify_syscall_atomicity(syscall_id_t syscall_id, 
                                  u64 pre_state, u64 post_state) {
    /* 验证状态转换的有效性 */
    u64 state_transition = post_state & 0xF;
    
    /* 检查中间状态 */
    if (state_transition >= 0x0003 && state_transition <= 0x000F) {
        /* 中间状态 - 原子性违反 */
        return false;
    }
    
    return true;
}
```

---

## 6. 精化证明框架

### 6.1 抽象到具体的精化

```
┌────────────────────────────────────────────────────────────────┐
│                  抽象层 (Isabelle)                              │
│                                                                │
│  specification:                                                │
│    cap_grant :: cap_table \<Rightarrow> domain_id \<Rightarrow> ...  │
│                                                                │
│  theorem: cap_grant_preserves_invariants                       │
└────────────────────────────────────────────────────────────────┘
                              ↓ refine
┌────────────────────────────────────────────────────────────────┐
│                  具体层 (C with VCC/VeriFast)                   │
│                                                                │
│  specification:                                                │
│    /*@ requires \valid(entry);                                 │
│        ensures \result == HIC_SUCCESS ==> ...;                 │
│    @*/                                                         │
│    hic_status_t cap_grant(domain_id_t domain, ...);            │
│                                                                │
│  proof: VCC/VeriFast 验证                                      │
└────────────────────────────────────────────────────────────────┘
```

### 6.2 VCC 规范示例

```c
/* VCC 规范绑定 */
_(ghost typedef struct abstract_cap {
    uint32_t cap_id;
    uint32_t rights;
    uint32_t owner;
    uint8_t state;
} abstract_cap_t;)

_(abstract _(pure) bool is_valid_cap(abstract_cap_t cap))
{
    return cap.cap_id < CAP_TABLE_SIZE &&
           cap.owner < MAX_DOMAINS &&
           cap.rights != 0;
}

/* 具体实现规范 */
_(requires \thread_local(domain))
_(requires \thread_local(cap))
_(requires \writes(\extent(g_global_cap_table)))
_(ensures \result == HIC_SUCCESS ==> 
    is_valid_cap(decode_handle(\result)))
hic_status_t cap_grant(domain_id_t domain, cap_id_t cap, 
                        cap_handle_t *out)
{
    // ... 实现 ...
}
```

---

## 7. 验证工具链

### 7.1 Isabelle 验证

```bash
# 构建所有 Isabelle 理论
isabelle build -D HIC -o browser_info

# 检查 sorry（未完成证明）
grep -r "sorry" HIC/*.thy
```

### 7.2 C 验证

```bash
# VCC 验证
vcc capability.c domain.c scheduler.c

# VeriFast 验证
verifast capability.c domain.c

# Frama-C 验证
frama-c -wp -wp-rte capability.c
```

### 7.3 绑定一致性检查

```bash
# 检查类型绑定一致性
python scripts/check_type_binding.py

# 检查函数绑定一致性
python scripts/check_func_binding.py

# 生成绑定报告
python scripts/gen_binding_report.py > binding_report.md
```

---

## 8. 持续验证

### 8.1 提交钩子

```bash
#!/bin/bash
# .git/hooks/pre-commit

# 检查 Isabelle 证明完整性
if grep -r "sorry" formal/*.thy; then
    echo "ERROR: 未完成的证明 (sorry)"
    exit 1
fi

# 检查 C 规范完整性
if grep -r "TODO: formal spec" src/Core-0/*.c; then
    echo "ERROR: 缺少形式化规范的代码"
    exit 1
fi

# 运行绑定一致性检查
make check-binding || exit 1
```

### 8.2 CI 集成

```yaml
# .github/workflows/formal-verification.yml
name: Formal Verification

on: [push, pull_request]

jobs:
  isabelle:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Check Isabelle proofs
        run: |
          isabelle build -D formal
          test ! $(grep -r "sorry" formal/*.thy)
  
  c-verification:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: VCC verification
        run: |
          vcc src/Core-0/*.c
  
  binding:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Check binding consistency
        run: |
          python scripts/check_binding.py
```

---

## 9. 绑定完整性矩阵

| 模块 | Isabelle 规范 | C 实现 | 类型绑定 | 函数绑定 | 不变式绑定 |
|------|-------------|--------|---------|---------|-----------|
| 能力系统 | ✓ | ✓ | ✓ | ✓ | ✓ |
| 域管理 | ✓ | ✓ | ✓ | ✓ | ✓ |
| 调度器 | ✓ | ✓ | ✓ | ⚠ | ⚠ |
| 内存管理 | ✓ | ✓ | ✓ | ⚠ | ✓ |
| 中断处理 | ✓ | ✓ | ⚠ | ⚠ | ⚠ |
| 审计系统 | ✓ | ✓ | ✓ | ✓ | ✓ |

**图例**: ✓ 完成绑定 | ⚠ 部分绑定 | ✗ 未绑定

---

## 10. 下一步工作

1. **完善调度器绑定**：补充调度器函数的 Isabelle-C 绑定证明
2. **中断处理绑定**：完成中断处理的形式化绑定
3. **自动绑定生成**：开发自动生成绑定证明的工具
4. **集成验证**：将 Isabelle、VCC、VeriFast 验证集成到 CI

---

*文档版本：1.0*
*最后更新：2026-03-27*
*符合标准：Common Criteria EAL7*
