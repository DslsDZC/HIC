# 域 (Domain) 抽象规范

## 1. 域标识与类型

### 1.1 域标识符

```
domain_id :: word32

-- 域标识符约束
domain_id_valid :: domain_id → bool
domain_id_valid(did) ⟷ 0 ≤ did < MAX_DOMAINS

-- 域标识符唯一性
unique_domain_id :: SystemState → domain_id → bool
unique_domain_id(σ, did) ⟷ 
  ∃! d. d ∈ σ.domains ∧ d.domain_id = did
```

### 1.2 域类型

```
domain_type :: 
  | CORE         -- Core-0 仲裁层，唯一，domain_id = 0
  | PRIVILEGED   -- Privileged-1 特权服务层
  | APPLICATION  -- Application-3 应用层

-- 类型约束
core_domain_unique :: SystemState → bool
core_domain_unique(σ) ⟷
  |{d ∈ σ.domains | d.type = CORE}| = 1 ∧
  (the d ∈ σ.domains | d.type = CORE).domain_id = 0

-- 类型层级关系
type_level :: domain_type → nat
type_level(CORE)        = 0
type_level(PRIVILEGED)  = 1
type_level(APPLICATION) = 3

type_hierarchy :: domain_type → domain_type → bool
type_hierarchy(t1, t2) ⟷ type_level(t1) < type_level(t2)
```

---

## 2. 域状态

### 2.1 生命周期状态

```
domain_state ::
  | INIT        -- 初始化中
  | READY       -- 就绪，等待调度
  | RUNNING     -- 正在运行
  | SUSPENDED   -- 暂停
  | TERMINATED  -- 已终止

-- 状态转换图
state_transition :: domain_state → domain_state → bool
state_transition = {
  INIT → READY,
  INIT → TERMINATED,      -- 初始化失败
  READY → RUNNING,
  READY → SUSPENDED,
  READY → TERMINATED,
  RUNNING → READY,        -- 时间片用尽
  RUNNING → SUSPENDED,    -- 显式暂停
  RUNNING → TERMINATED,   -- 显式终止
  SUSPENDED → READY,      -- 恢复
  SUSPENDED → TERMINATED  -- 销毁暂停域
}

-- 有效状态转换
valid_transition :: domain_state → domain_state → bool
valid_transition(s1, s2) ⟷ (s1, s2) ∈ state_transition
```

### 2.2 状态不变量

```
-- 只有 READY 或 RUNNING 状态的域可以被调度
schedulable :: domain_state → bool
schedulable(READY)    = true
schedulable(RUNNING)  = true
schedulable(_)        = false

-- RUNNING 状态意味着至少有一个线程在执行
running_implies_active_thread :: SystemState → Domain → bool
running_implies_active_thread(σ, d) ⟷
  d.state = RUNNING →
  ∃ t ∈ σ.threads. t.domain_id = d.domain_id ∧ t.state = RUNNING
```

---

## 3. 域结构

### 3.1 域抽象定义

```
Domain :: {
  -- 标识
  domain_id     : domain_id,
  type          : domain_type,
  state         : domain_state,
  
  -- 父子关系
  parent_domain : option domain_id,
  child_domains : domain_id set,
  
  -- 物理内存布局
  phys_base     : physical_addr,
  phys_size     : size,
  page_table    : option page_table_id,
  
  -- 能力空间
  cap_space     : capability_handle set,
  cap_capacity  : nat,           -- 能力槽位上限
  
  -- 线程集合
  threads       : thread_id set,
  thread_limit  : nat,           -- 线程数上限
  
  -- 资源配额
  quota         : DomainQuota,
  
  -- 使用统计
  usage         : DomainUsage,
  
  -- 标志
  flags         : domain_flags
}
```

### 3.2 域配额

```
DomainQuota :: {
  memory_quota    : nat,  -- 内存配额（字节）
  cpu_quota       : nat,  -- CPU 时间配额（时间片数）
  io_quota        : nat,  -- I/O 带宽配额
  capability_quota: nat,  -- 能力数量配额
  thread_quota    : nat   -- 线程数量配额
}

-- 配额有效性
quota_valid :: DomainQuota → bool
quota_valid(q) ⟷
  q.memory_quota > 0 ∧
  q.cpu_quota ≥ 0 ∧
  q.io_quota ≥ 0 ∧
  q.capability_quota > 0 ∧
  q.thread_quota > 0
```

### 3.3 域使用统计

```
DomainUsage :: {
  memory_used      : nat,  -- 已用内存
  cpu_time_used    : nat,  -- 已用 CPU 时间
  io_used          : nat,  -- 已用 I/O
  capability_used  : nat,  -- 已用能力数
  thread_used      : nat,  -- 已用线程数
  
  -- 异常检测指标
  memory_alloc_rate : nat, -- 内存分配速率
  cpu_usage_peak    : nat  -- CPU 使用峰值
}
```

### 3.4 域标志

```
domain_flags :: word32

-- 标志位定义
DOMAIN_FLAG_TRUSTED    = 0x01  -- 可信域
DOMAIN_FLAG_CRITICAL   = 0x02  -- 关键域，不可终止
DOMAIN_FLAG_PRIVILEGED = 0x04  -- 特权域，可访问物理内存
DOMAIN_FLAG_PRIMARY    = 0x08  -- 主域，负责启动其他域
DOMAIN_FLAG_ISOLATED   = 0x10  -- 隔离域，禁止 IPC
```

---

## 4. 域操作

### 4.1 域创建

```
domain_create :: 
  SystemState → 
  domain_type → 
  option domain_id →      -- 父域（可选）
  DomainQuota → 
  domain_flags →
  (SystemState × domain_id) option

-- 前置条件
domain_create_pre(σ, dtype, parent, quota, flags) ⟷
  -- 存在可用域槽位
  |σ.domains| < MAX_DOMAINS ∧
  -- 配额有效
  quota_valid(quota) ∧
  -- 父域存在且有效
  (parent = None ∨ 
   (∃ p ∈ σ.domains. p.domain_id = parent ∧ p.state ≠ TERMINATED)) ∧
  -- CORE 类型约束
  (dtype = CORE → |{d ∈ σ.domains | d.type = CORE}| = 0) ∧
  -- 配额不超过父域
  (parent ≠ None → 
   let p = the d ∈ σ.domains | d.domain_id = parent in
   quota ≤ p.quota - p.usage)

-- 后置条件
domain_create_post(σ, σ', did) ⟷
  -- 新域被创建
  ∃ d_new ∈ σ'.domains. 
    d_new.domain_id = did ∧
    d_new.state = INIT ∧
    d_new.cap_space = ∅ ∧
    d_new.threads = ∅ ∧
    d_new.usage = zero_usage ∧
  -- 父子关系建立
  (let parent = d_new.parent_domain in
   parent ≠ None →
   let p = the d ∈ σ'.domains | d.domain_id = parent in
   did ∈ p.child_domains) ∧
  -- 其他域不变
  ∀ d ∈ σ.domains. d.domain_id ≠ did → d ∈ σ'.domains
```

### 4.2 域销毁

```
domain_destroy ::
  SystemState →
  domain_id →
  SystemState option

-- 前置条件
domain_destroy_pre(σ, did) ⟷
  -- 域存在
  ∃ d ∈ σ.domains. d.domain_id = did ∧
  -- 非 CORE 域
  d.type ≠ CORE ∧
  -- 非关键域
  ¬(d.flags & DOMAIN_FLAG_CRITICAL) ∧
  -- 无子域
  d.child_domains = ∅ ∧
  -- 可终止状态
  d.state ∈ {READY, SUSPENDED, TERMINATED}

-- 后置条件
domain_destroy_post(σ, σ', did) ⟷
  -- 域被移除
  ¬(∃ d ∈ σ'.domains. d.domain_id = did) ∧
  -- 所有线程被终止
  ∀ t ∈ σ.threads. t.domain_id = did → t.state = TERMINATED ∧
  -- 所有能力被撤销
  ∀ cap ∈ σ.cap_table. cap.owner = did → cap.revoked = true ∧
  -- 从父域移除
  (let d = the d ∈ σ.domains | d.domain_id = did in
   let p = the d ∈ σ.domains | d.domain_id = d.parent_domain in
   did ∉ p.child_domains in σ')
```

### 4.3 域暂停

```
domain_suspend ::
  SystemState →
  domain_id →
  SystemState option

-- 前置条件
domain_suspend_pre(σ, did) ⟷
  ∃ d ∈ σ.domains. d.domain_id = did ∧
  d.state ∈ {READY, RUNNING}

-- 后置条件
domain_suspend_post(σ, σ', did) ⟷
  let d' = the d ∈ σ'.domains | d.domain_id = did in
  d'.state = SUSPENDED ∧
  -- 所有运行线程变为就绪
  ∀ t ∈ σ.threads. 
    t.domain_id = did ∧ t.state = RUNNING →
    (the t' ∈ σ'.threads | t'.thread_id = t.thread_id).state = READY
```

### 4.4 域恢复

```
domain_resume ::
  SystemState →
  domain_id →
  SystemState option

-- 前置条件
domain_resume_pre(σ, did) ⟷
  ∃ d ∈ σ.domains. d.domain_id = did ∧
  d.state = SUSPENDED

-- 后置条件
domain_resume_post(σ, σ', did) ⟷
  let d' = the d ∈ σ'.domains | d.domain_id = did in
  d'.state = READY
```

### 4.5 配额检查

```
domain_quota_check ::
  SystemState →
  domain_id →
  quota_type →
  size →
  quota_check_result

quota_type :: MEMORY | CPU | IO | CAPABILITY | THREAD

quota_check_result :: 
  | QUOTA_OK remaining:size
  | QUOTA_EXCEEDED limit:size used:size requested:size
  | QUOTA_DOMAIN_NOT_FOUND

-- 定义
domain_quota_check(σ, did, qtype, amount) =
  let d = the d ∈ σ.domains | d.domain_id = did in
  case qtype of
    MEMORY → 
      if d.usage.memory_used + amount ≤ d.quota.memory_quota
      then QUOTA_OK(d.quota.memory_quota - d.usage.memory_used - amount)
      else QUOTA_EXCEEDED(d.quota.memory_quota, d.usage.memory_used, amount)
    CPU → ...
    IO → ...
    CAPABILITY → ...
    THREAD → ...
```

### 4.6 配额消耗

```
domain_quota_consume ::
  SystemState →
  domain_id →
  quota_type →
  size →
  SystemState option

-- 前置条件
domain_quota_consume_pre(σ, did, qtype, amount) ⟷
  domain_quota_check(σ, did, qtype, amount) = QUOTA_OK(_)

-- 后置条件
domain_quota_consume_post(σ, σ', did, qtype, amount) ⟷
  let d = the d ∈ σ.domains | d.domain_id = did in
  let d' = the d ∈ σ'.domains | d'.domain_id = did in
  case qtype of
    MEMORY → d'.usage.memory_used = d.usage.memory_used + amount
    CPU → d'.usage.cpu_time_used = d.usage.cpu_time_used + amount
    ...
```

---

## 5. 原子域切换（零停机更新）

### 5.1 并行域创建

```
domain_parallel_create ::
  SystemState →
  domain_id →           -- 模板域
  string →              -- 新域名称
  DomainQuota →
  (SystemState × domain_id) option

-- 基于模板创建新域，复用能力配置和内存布局
-- 新域处于 INIT 状态，等待原子切换
```

### 5.2 原子切换

```
domain_atomic_switch ::
  SystemState →
  domain_id →           -- 源域
  domain_id →           -- 目标域
  cap_id list →         -- 需要重定向的能力列表
  SystemState option

-- 前置条件
domain_atomic_switch_pre(σ, from_id, to_id, cap_list) ⟷
  -- 源域和目标域都存在
  ∃ from_d ∈ σ.domains. from_d.domain_id = from_id ∧
  ∃ to_d ∈ σ.domains. to_d.domain_id = to_id ∧
  -- 目标域处于 READY 状态
  to_d.state = READY ∧
  -- 能力列表有效
  ∀ cap_id ∈ cap_list. 
    ∃ cap ∈ σ.cap_table. 
      cap.cap_id = cap_id ∧
      cap.owner = from_id ∧
      cap.type = ENDPOINT

-- 后置条件
domain_atomic_switch_post(σ, σ', from_id, to_id, cap_list) ⟷
  -- 所有端点能力原子重定向
  ∀ cap_id ∈ cap_list.
    let cap = the c ∈ σ.cap_table | c.cap_id = cap_id in
    let cap' = the c ∈ σ'.cap_table | c.cap_id = cap_id in
    cap'.endpoint.target = to_id ∧
    cap'.owner = to_id ∧
  -- 源域状态变为 TERMINATED
  (the d ∈ σ'.domains | d.domain_id = from_id).state = TERMINATED ∧
  -- 目标域状态变为 RUNNING
  (the d ∈ σ'.domains | d.domain_id = to_id).state = RUNNING
```

---

## 6. 域不变量

### 6.1 结构不变量

```
-- 域 ID 唯一性
invariant_domain_id_unique :: SystemState → bool
invariant_domain_id_unique(σ) ⟷
  ∀ d1 d2 ∈ σ.domains.
    d1.domain_id = d2.domain_id → d1 = d2

-- 域 ID 有效范围
invariant_domain_id_valid :: SystemState → bool
invariant_domain_id_valid(σ) ⟷
  ∀ d ∈ σ.domains. 0 ≤ d.domain_id < MAX_DOMAINS

-- CORE 域唯一性
invariant_core_domain_unique :: SystemState → bool
invariant_core_domain_unique(σ) ⟷
  |{d ∈ σ.domains | d.type = CORE}| ≤ 1 ∧
  (∃ d ∈ σ.domains. d.type = CORE → d.domain_id = 0)

-- 父子关系一致性
invariant_parent_child_consistency :: SystemState → bool
invariant_parent_child_consistency(σ) ⟷
  ∀ d ∈ σ.domains.
    -- 父域存在且未终止
    (d.parent_domain ≠ None →
     ∃ p ∈ σ.domains. 
       p.domain_id = d.parent_domain ∧
       p.state ≠ TERMINATED) ∧
    -- 子域列表与实际一致
    d.child_domains = 
      {c.domain_id | c ∈ σ.domains ∧ c.parent_domain = Some(d.domain_id)}
```

### 6.2 状态不变量

```
-- 状态转换合法性
invariant_valid_state_transition :: SystemState → bool
invariant_valid_state_transition(σ) ⟷
  -- 此不变量在状态转换前后验证
  true  -- 由操作语义保证

-- RUNNING 状态有线程执行
invariant_running_domain_has_thread :: SystemState → bool
invariant_running_domain_has_thread(σ) ⟷
  ∀ d ∈ σ.domains.
    d.state = RUNNING →
    ∃ t ∈ σ.threads. t.domain_id = d.domain_id ∧ t.state = RUNNING
```

### 6.3 资源不变量

```
-- 内存配额不超限
invariant_memory_quota :: SystemState → bool
invariant_memory_quota(σ) ⟷
  ∀ d ∈ σ.domains.
    d.usage.memory_used ≤ d.quota.memory_quota

-- 线程数不超限
invariant_thread_quota :: SystemState → bool
invariant_thread_quota(σ) ⟷
  ∀ d ∈ σ.domains.
    d.usage.thread_used ≤ d.quota.thread_quota ∧
    |d.threads| = d.usage.thread_used

-- 能力数不超限
invariant_capability_quota :: SystemState → bool
invariant_capability_quota(σ) ⟷
  ∀ d ∈ σ.domains.
    d.usage.capability_used ≤ d.quota.capability_quota ∧
    |d.cap_space| = d.usage.capability_used
```

### 6.4 隔离不变量

```
-- 内存区域不重叠（除非显式共享）
invariant_memory_isolation :: SystemState → bool
invariant_memory_isolation(σ) ⟷
  ∀ d1 d2 ∈ σ.domains.
    d1.domain_id ≠ d2.domain_id →
    let r1 = (d1.phys_base, d1.phys_base + d1.phys_size) in
    let r2 = (d2.phys_base, d2.phys_base + d2.phys_size) in
    disjoint(r1, r2) ∨
    explicitly_shared(σ, d1.domain_id, d2.domain_id, r1 ∩ r2)

-- 显式共享定义
explicitly_shared :: SystemState → domain_id → domain_id → memory_region → bool
explicitly_shared(σ, did1, did2, region) ⟷
  ∃ cap ∈ σ.cap_table.
    cap.type = SHARED_MEMORY ∧
    cap.memory.base ≤ region.start ∧
    region.end ≤ cap.memory.base + cap.memory.size ∧
    did1 ∈ cap.participants ∧
    did2 ∈ cap.participants
```

### 6.5 类型层级不变量

```
-- 子域类型层级不低于父域
invariant_type_hierarchy :: SystemState → bool
invariant_type_hierarchy(σ) ⟷
  ∀ d ∈ σ.domains.
    d.parent_domain ≠ None →
    let p = the x ∈ σ.domains | x.domain_id = d.parent_domain in
    type_level(d.type) ≥ type_level(p.type)

-- 能力传递遵守类型层级
invariant_capability_type_hierarchy :: SystemState → bool
invariant_capability_type_hierarchy(σ) ⟷
  ∀ cap ∈ σ.cap_table.
    cap.type = DOMAIN →
    let owner_domain = the d ∈ σ.domains | d.domain_id = cap.owner in
    let target_domain = the d ∈ σ.domains | d.domain_id = cap.domain.target in
    type_level(owner_domain.type) ≤ type_level(target_domain.type)
```

---

## 7. 域关系图

```
-- 域的父子关系形成有向无环图 (DAG)
domain_graph :: SystemState → (domain_id × domain_id) set
domain_graph(σ) = {
  (d.domain_id, p) | d ∈ σ.domains, p = d.parent_domain, p ≠ None
}

-- 无环性
invariant_domain_graph_acyclic :: SystemState → bool
invariant_domain_graph_acyclic(σ) ⟷
  acyclic(domain_graph(σ))

-- 根域
root_domains :: SystemState → domain_id set
root_domains(σ) = {
  d.domain_id | d ∈ σ.domains, d.parent_domain = None
}

-- Core-0 必须是根域
invariant_core_is_root :: SystemState → bool
invariant_core_is_root(σ) ⟷
  ∃ d ∈ σ.domains. d.type = CORE → d.parent_domain = None
```

---

## 8. 域能力空间

### 8.1 能力空间定义

```
-- 域的能力空间是其持有的所有能力句柄集合
capability_space :: SystemState → domain_id → capability_handle set
capability_space(σ, did) = 
  {h | ∃ cap ∈ σ.cap_table. 
       cap.owner = did ∧ 
       ¬cap.revoked ∧
       h = encode_handle(did, cap.cap_id)}

-- 能力空间约束
invariant_capability_space_valid :: SystemState → bool
invariant_capability_space_valid(σ) ⟷
  ∀ d ∈ σ.domains.
    -- 能力空间中的句柄都有效
    ∀ h ∈ d.cap_space.
      ∃ cap ∈ σ.cap_table.
        decode_handle(h).domain_id = d.domain_id ∧
        decode_handle(h).cap_id = cap.cap_id ∧
        ¬cap.revoked
```

### 8.2 能力空间操作

```
-- 添加能力到域
add_capability :: Domain → capability_handle → Domain
add_capability(d, h) = d' where
  d'.cap_space = d.cap_space ∪ {h}
  d'.usage.capability_used = d.usage.capability_used + 1

-- 从域移除能力
remove_capability :: Domain → capability_handle → Domain
remove_capability(d, h) = d' where
  d'.cap_space = d.cap_space \ {h}
  d'.usage.capability_used = d.usage.capability_used - 1
```

---

## 9. 特权域特殊语义

### 9.1 特权内存访问

```
-- 特权域可直接访问物理内存（除 Core-0 区域）
privileged_memory_access :: SystemState → domain_id → physical_addr → bool
privileged_memory_access(σ, did, addr) ⟷
  let d = the x ∈ σ.domains | x.domain_id = did in
  (d.flags & DOMAIN_FLAG_PRIVILEGED ≠ 0) ∧
  (addr < σ.core0_mem_start ∨ addr ≥ σ.core0_mem_end)

-- 特权访问不变量
invariant_privileged_memory_boundary :: SystemState → bool
invariant_privileged_memory_boundary(σ) ⟷
  ∀ d ∈ σ.domains.
    (d.flags & DOMAIN_FLAG_PRIVILEGED ≠ 0) →
    -- 特权域不可访问 Core-0 内存
    ∀ addr ∈ σ.core0_mem_region.
      ¬can_access(d, addr)
```

### 9.2 特权系统调用

```
-- 仅特权域可执行的 syscall
privileged_syscalls :: syscall_id set
privileged_syscalls = {
  SYS_DIRECT_PHYSICAL_ACCESS,
  SYS_INTERRUPT_ROUTING,
  SYS_MODULE_LOAD,
  SYS_DOMAIN_CREATE_PRIVELEGED,
  ...
}

-- 特权系统调用检查
check_privileged_syscall :: SystemState → domain_id → syscall_id → bool
check_privileged_syscall(σ, did, sid) ⟷
  sid ∉ privileged_syscalls ∨
  let d = the x ∈ σ.domains | x.domain_id = did in
  (d.flags & DOMAIN_FLAG_PRIVILEGED ≠ 0)
```
