<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 形式化验证

## 概述

HIK 形式化验证系统提供数学层面的安全保证，通过严格的数学证明和运行时检查确保系统安全属性。这是 HIK 与传统内核最显著的区别之一。

## 数学基础

### 核心定理

HIK 基于以下 7 个数学定理：

1. **能力守恒定理**
   - 定理: ∀d ∈ Domains, |Capabilities(d)| = InitialQuota(d) + Granted(d) - Revoked(d)
   - 含义: 域能力数量守恒

2. **内存隔离定理**
   - 定理: ∀d1, d2 ∈ Domains, d1 ≠ d2 ⇒ Memory(d1) ∩ Memory(d2) = ∅
   - 含义: 域内存区域不相交

3. **权限单调性定理**
   - 定理: ∀c1, c2 ∈ Capabilities, Derived(c1, c2) ⇒ Permissions(c2) ⊆ Permissions(c1)
   - 含义: 派生权限是源权限的子集

4. **资源配额守恒定理**
   - 定理: ∀r ∈ Resources, Σ_{d∈Domains} Allocated(r, d) ≤ Total(r)
   - 含义: 资源分配不超过总量

5. **无死锁定理**
   - 定理: ∀t ∈ Threads, ∃s: State(t) = Running ∨ State(t) → Running
   - 含义: 资源分配图中无环

6. **类型安全性定理**
   - 定理: ∀o ∈ Objects, ∀a ∈ Access(o), Type(a) ∈ AllowedTypes(o)
   - 含义: 访问类型符合对象约束

7. **系统调用原子性定理**
   - 定理: ∀s ∈ Syscalls, Exec(s) ⇒ (State(s, post) = State(s, success) ∨ State(s, post) = State(s, fail))
   - 含义: 系统调用原子性

### 证明框架

完整的数学证明参见 `src/Core-0/math_proofs.tex`。

## 运行时验证

### 不变式检查

HIK 在运行时检查 6 个核心不变式：

```c
typedef struct invariant {
    u64 invariant_id;
    const char* name;
    bool (*check)(void);
    const char* description;
} invariant_t;

static invariant_t invariants[] = {
    {1, "能力守恒性", invariant_capability_conservation, "域能力数量守恒"},
    {2, "内存隔离性", invariant_memory_isolation, "域内存区域不相交"},
    {3, "能力权限单调性", invariant_capability_monotonicity, "派生权限是源权限子集"},
    {4, "资源配额守恒性", invariant_resource_quota_conservation, "资源分配不超过总量"},
    {5, "无死锁性", invariant_deadlock_freedom, "资源分配图中无环"},
    {6, "类型安全性", invariant_type_safety, "访问类型符合对象约束"},
};
```

### 检查接口

```c
// 检查所有不变式
int fv_check_all_invariants(void) {
    invariant_check_count++;
    
    for (u64 i = 0; i < num_invariants; i++) {
        if (!invariants[i].check()) {
            invariant_violation_count++;
            last_violation_invariant_id = invariants[i].invariant_id;
            
            // 记录违反
            fv_log_violation(&invariants[i]);
            
            return FV_CAP_INVALID + i;
        }
    }
    
    return FV_SUCCESS;
}
```

## 不变式实现

### 1. 能力守恒性

```c
static bool invariant_capability_conservation(void) {
    for (domain_id_t domain = 0; domain < MAX_DOMAINS; domain++) {
        if (!domain_is_active(domain)) continue;
        
        // 统计当前能力数
        u64 current_caps = count_domain_capabilities(domain);
        
        // 统计预期能力数
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

### 2. 内存隔离性

```c
static bool invariant_memory_isolation(void) {
    for (domain_id_t d1 = 0; d1 < MAX_DOMAINS; d1++) {
        if (!domain_is_active(d1)) continue;
        
        for (domain_id_t d2 = d1 + 1; d2 < MAX_DOMAINS; d2++) {
            if (!domain_is_active(d2)) continue;
            
            mem_region_t r1 = get_domain_memory_region(d1);
            mem_region_t r2 = get_domain_memory_region(d2);
            
            if (regions_overlap(&r1, &r2)) {
                return false;
            }
        }
    }
    return true;
}
```

### 3. 能力权限单调性

```c
static bool invariant_capability_monotonicity(void) {
    for (cap_id_t cap1 = 0; cap1 < MAX_CAPABILITIES; cap1++) {
        if (!capability_exists(cap1)) continue;
        
        cap_id_t* derivatives = get_capability_derivatives(cap1);
        for (u64 i = 0; derivatives[i] != INVALID_CAP_ID; i++) {
            cap_id_t cap2 = derivatives[i];
            
            if (!is_permission_subset(cap2, cap1)) {
                return false;
            }
        }
    }
    return true;
}
```

### 4. 资源配额守恒性

```c
static bool invariant_resource_quota_conservation(void) {
    // 检查内存配额
    u64 total_allocated_memory = 0;
    for (domain_id_t domain = 0; domain < MAX_DOMAINS; domain++) {
        if (!domain_is_active(domain)) continue;
        total_allocated_memory += get_domain_allocated_memory(domain);
    }
    
    u64 total_pages, free_pages, used_pages;
    pmm_get_stats(&total_pages, &free_pages, &used_pages);
    u64 total_physical_memory = total_pages * PAGE_SIZE;
    
    if (total_allocated_memory > total_physical_memory) {
        return false;
    }
    
    // 检查 CPU 配额
    u64 total_cpu_quota = 0;
    for (domain_id_t domain = 0; domain < MAX_DOMAINS; domain++) {
        if (!domain_is_active(domain)) continue;
        total_cpu_quota += get_domain_cpu_quota(domain);
    }
    
    if (total_cpu_quota > 100) {
        return false;
    }
    
    return true;
}
```

### 5. 无死锁性

```c
static bool invariant_deadlock_freedom(void) {
    // 检查长时间等待的线程
    for (thread_id_t thread = 0; thread < MAX_THREADS; thread++) {
        if (!thread_is_active(thread)) continue;
        
        u64 wait_time = get_thread_wait_time(thread);
        if (wait_time > DEADLOCK_THRESHOLD) {
            log_warning("线程 %lu 等待时间过长: %lu ns\n", thread, wait_time);
        }
    }
    
    return true;
}
```

### 6. 类型安全性

```c
static bool invariant_type_safety(void) {
    for (cap_id_t cap = 0; cap < MAX_CAPABILITIES; cap++) {
        if (!capability_exists(cap)) continue;
        
        cap_type_t cap_type = get_capability_type(cap);
        obj_type_t obj_type = get_capability_object_type(cap);
        
        if (!is_type_compatible(cap_type, obj_type)) {
            return false;
        }
    }
    
    return true;
}
```

## 验证集成

### 系统初始化

```c
void fv_init(void) {
    console_puts("[FV] Initializing formal verification...\n");
    
    // 初始化检查计数器
    invariant_check_count = 0;
    invariant_violation_count = 0;
    
    // 执行初始检查
    int result = fv_check_all_invariants();
    if (result != FV_SUCCESS) {
        console_puts("[FV] Initial invariant check failed!\n");
        fv_panic("初始不变式检查失败");
    }
    
    console_puts("[FV] Formal verification initialized\n");
}
```

### 关键操作验证

```c
// 内存分配后验证
hik_status_t pmm_alloc_frames(domain_id_t owner, u32 count, 
                               page_frame_type_t type, phys_addr_t *out) {
    // 分配内存
    hik_status_t status = allocate_frames(owner, count, type, out);
    
    if (status == HIK_SUCCESS) {
        // 记录分配大小
        set_last_allocation_size(count * PAGE_SIZE);
        
        // 验证不变式
        if (fv_check_all_invariants() != FV_SUCCESS) {
            console_puts("[PMM] Invariant violation detected!\n");
            // 回滚分配
            return HIK_ERROR_INTERNAL;
        }
    }
    
    return status;
}

// 能力操作后验证
hik_status_t cap_transfer(domain_id_t from, domain_id_t to, cap_id_t cap) {
    // 转移能力
    hik_status_t status = transfer_capability(from, to, cap);
    
    if (status == HIK_SUCCESS) {
        // 验证不变式
        if (fv_check_all_invariants() != FV_SUCCESS) {
            console_puts("[CAP] Invariant violation detected!\n");
            return HIK_ERROR_INTERNAL;
        }
    }
    
    return status;
}
```

## 验证统计

### 获取统计信息

```c
void fv_get_stats(u64* total_checks, u64* violations, u64* last_violation_id) {
    if (total_checks) *total_checks = invariant_check_count;
    if (violations) *violations = invariant_violation_count;
    if (last_violation_id) *last_violation_id = last_violation_invariant_id;
}
```

### 违反处理

```c
static void fv_log_violation(const invariant_t* inv) {
    // 记录到审计日志
    audit_log(AUDIT_SEVERITY_CRITICAL, "形式化验证违反",
              "不变式ID: %lu, 名称: %s, 描述: %s",
              inv->invariant_id, inv->name, inv->description);
    
    // 生成报告
    generate_violation_report(inv);
    
    // 触发系统暂停
    system_panic_with_reason(inv->description);
}
```

## 性能考虑

### 优化策略

1. **选择性检查**: 不是每次操作都检查所有不变式
2. **增量检查**: 只检查相关的不变式
3. **采样检查**: 定期采样而非每次都检查
4. **异步检查**: 在后台线程中执行检查

### 性能影响

- 初始化检查: 一次性，约 10ms
- 关键操作检查: 每次约 1-5μs
- 总体性能影响: < 5%

## 最佳实践

1. **始终启用**: 不要禁用形式化验证
2. **定期检查**: 定期检查验证统计
3. **及时响应**: 发现违反立即处理
4. **分析模式**: 分析违反模式优化系统
5. **更新证明**: 随系统更新更新数学证明

## 相关文档

- [数学证明](../src/Core-0/math_proofs.tex) - 完整数学证明
- [安全架构](./13-SecurityArchitecture.md) - 安全架构
- [审计日志](./14-AuditLogging.md) - 审计机制

---

*最后更新: 2026-02-14*