<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 安全机制

HIC通过多层安全机制提供强安全隔离和防护。

## 安全架构

### 防御深度

```
┌─────────────────────────────────────────────────┐
│                 应用层攻击面                    │
└─────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────┐
│            能力系统验证 (逻辑层)                 │
│  - 能力验证                                      │
│  - 权限检查                                      │
│  - 域隔离                                        │
└─────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────┐
│            MMU硬件隔离 (物理层)                  │
│  - 页表保护                                      │
│  - 特权级分离                                    │
│  - 内存隔离                                      │
└─────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────┐
│            形式化验证 (数学层)                   │
│  - 不变式证明                                    │
│  - 类型安全                                      │
│  - 安全属性验证                                  │
└─────────────────────────────────────────────────┘
```

### 安全边界

| 边界 | 保护机制 | 验证方式 |
|------|----------|----------|
| Core-0 ↔ Privileged-1 | 能力验证、MMU隔离 | 形式化证明 |
| Privileged-1 ↔ Privileged-1 | 能力验证、MMU隔离 | 运行时检查 |
| Privileged-1 ↔ Application-3 | 能力验证、特权级隔离 | 运行时检查 |
| Application-3 ↔ Application-3 | 能力验证、虚拟地址隔离 | 运行时检查 |

## 能力系统安全

### 能力验证流程

```c
/**
 * @brief 能力验证完整流程
 */
status_t secure_cap_verify(domain_id_t domain, cap_id_t cap_id,
                          cap_rights_t required_rights)
{
    /* 1. 格式验证 */
    if (!is_valid_cap_id_format(cap_id)) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 2. 边界检查 */
    cap_id_t index = cap_id_to_index(cap_id);
    if (index >= MAX_CAPABILITIES) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 3. ID匹配验证 */
    cap_entry_t *entry = &g_cap_table[index];
    if (entry->id != cap_id) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 4. 类型验证 */
    if (entry->type == CAP_TYPE_INVALID) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 5. 撤销状态检查 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        return HIC_ERROR_REVOKED;
    }

    /* 6. 所有权验证 */
    if (entry->owner != domain) {
        return HIC_ERROR_PERMISSION;
    }

    /* 7. 权限验证 */
    if ((entry->rights & required_rights) != required_rights) {
        return HIC_ERROR_PERMISSION;
    }

    /* 8. 时间检查（防止重放攻击） */
    if (entry->create_time > get_timestamp()) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 9. 引用计数检查 */
    if (entry->ref_count >= MAX_REF_COUNT) {
        return HIC_ERROR_TOO_MANY_REFS;
    }

    return HIC_SUCCESS;
}
```

### 能力撤销安全

```c
/**
 * @brief 安全撤销能力
 */
status_t secure_cap_revoke(domain_id_t domain, cap_id_t cap_id)
{
    /* 验证撤销权限 */
    status = cap_verify(domain, cap_id, CAP_PERM_REVOKE);
    if (status != HIC_SUCCESS) {
        return status;
    }

    /* 检查不可变标志 */
    cap_entry_t *entry = &g_cap_table[cap_id_to_index(cap_id)];
    if (entry->flags & CAP_FLAG_IMMUTABLE) {
        return HIC_ERROR_IMMUTABLE;
    }

    /* 原子操作：标记撤销 */
    atomic_store(&entry->flags, entry->flags | CAP_FLAG_REVOKED);

    /* 从所有域的能力空间中移除 */
    remove_from_all_cap_spaces(cap_id);

    /* 撤销所有派生能力 */
    revoke_all_derived_caps(cap_id);

    /* 清理关联资源 */
    cleanup_associated_resources(cap_id);

    /* 记录审计日志 */
    AUDIT_LOG_CAP_REVOKE(domain, cap_id);

    return HIC_SUCCESS;
}
```

## 内存隔离

### 页表保护

```c
/**
 * @brief 创建安全页表
 */
status_t create_secure_pagetable(domain_id_t domain_id,
                                pagetable_t *out)
{
    pagetable_t pt = allocate_pagetable();
    if (!pt) {
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 1. 映射Core-0为不可访问 */
    for (u64 addr = CORE_0_BASE; addr < CORE_0_END; addr += PAGE_SIZE) {
        map_page(pt, addr, addr, PAGE_FLAGS_NONE);
    }

    /* 2. 映射自身代码为只读 */
    for (u64 addr = DOMAIN_CODE_BASE(domain_id);
         addr < DOMAIN_CODE_END(domain_id);
         addr += PAGE_SIZE) {
        map_page(pt, addr, addr, PAGE_FLAGS_READ | PAGE_FLAGS_EXEC);
    }

    /* 3. 映射数据为读写 */
    for (u64 addr = DOMAIN_DATA_BASE(domain_id);
         addr < DOMAIN_DATA_END(domain_id);
         addr += PAGE_SIZE) {
        map_page(pt, addr, addr, PAGE_FLAGS_READ | PAGE_FLAGS_WRITE);
    }

    /* 4. 映射共享内存（根据能力） */
    cap_entry_t *mem_caps[MAX_MEM_CAPS];
    u32 cap_count = 0;
    get_domain_memory_caps(domain_id, mem_caps, &cap_count);

    for (u32 i = 0; i < cap_count; i++) {
        cap_entry_t *cap = mem_caps[i];
        cap_rights_t rights = cap->rights;
        u64 flags = 0;

        if (rights & CAP_PERM_READ) flags |= PAGE_FLAGS_READ;
        if (rights & CAP_PERM_WRITE) flags |= PAGE_FLAGS_WRITE;
        if (rights & CAP_PERM_EXECUTE) flags |= PAGE_FLAGS_EXEC;

        for (u64 addr = cap->resource.memory.base;
             addr < cap->resource.memory.base + cap->resource.memory.size;
             addr += PAGE_SIZE) {
            map_page(pt, addr, addr, flags);
        }
    }

    /* 5. 设置CR3 */
    set_cr3(pt->physical_address);

    *out = pt;
    return HIC_SUCCESS;
}
```

### 域内存隔离

```c
/**
 * @brief 验证域内存隔离
 */
bool verify_domain_memory_isolation(void)
{
    /* 检查所有域的内存区域是否不相交 */
    for (domain_id_t d1 = 0; d1 < MAX_DOMAINS; d1++) {
        domain_t *domain1 = &g_domains[d1];

        if (domain1->state == DOMAIN_STATE_INVALID) {
            continue;
        }

        for (domain_id_t d2 = d1 + 1; d2 < MAX_DOMAINS; d2++) {
            domain_t *domain2 = &g_domains[d2];

            if (domain2->state == DOMAIN_STATE_INVALID) {
                continue;
            }

            /* 检查代码段是否重叠 */
            if (ranges_overlap(domain1->code_base, domain1->code_size,
                             domain2->code_base, domain2->code_size)) {
                return false;
            }

            /* 检查数据段是否重叠 */
            if (ranges_overlap(domain1->data_base, domain1->data_size,
                             domain2->data_base, domain2->data_size)) {
                return false;
            }

            /* 检查共享内存是否冲突 */
            if (shared_memory_conflicts(d1, d2)) {
                return false;
            }
        }
    }

    return true;
}
```

## 拒绝服务防护

### 资源配额

```c
/**
 * @brief 资源配额
 */
typedef struct {
    u64 max_memory;        /* 最大内存 */
    u64 max_threads;       /* 最大线程数 */
    u64 max_caps;          /* 最大能力数 */
    u64 max_ipc_calls;     /* 最大IPC调用/秒 */
    u64 max_io_ops;        /* 最大I/O操作/秒 */
} quota_t;

/**
 * @brief 检查资源配额
 */
bool check_resource_quota(domain_id_t domain_id, resource_type_t type, u64 amount)
{
    domain_t *domain = &g_domains[domain_id];
    quota_t *quota = &domain->quota;

    switch (type) {
    case RESOURCE_MEMORY:
        return (domain->usage.memory_used + amount) <= quota->max_memory;
    case RESOURCE_THREAD:
        return (domain->usage.thread_used + amount) <= quota->max_threads;
    case RESOURCE_CAP:
        return (domain->usage.cap_used + amount) <= quota->max_caps;
    default:
        return false;
    }
}
```

### 流量控制

```c
/**
 * @brief IPC流量控制
 */
typedef struct {
    u64 calls_per_second;
    u64 max_message_size;
    u64 max_pending_messages;
    u64 current_calls;
    u64 current_messages;
    u64 last_second;
} ipc_quota_t;

/**
 * @brief 检查IPC配额
 */
status_t check_ipc_quota(domain_id_t domain_id)
{
    ipc_quota_t *quota = &g_domains[domain_id].ipc_quota;
    u64 current_time = get_timestamp();

    /* 每秒重置计数器 */
    if (current_time - quota->last_second >= 1000) {
        quota->current_calls = 0;
        quota->last_second = current_time;
    }

    /* 检查调用频率 */
    if (quota->current_calls >= quota->calls_per_second) {
        return HIC_ERROR_RATE_LIMIT;
    }

    quota->current_calls++;
    return HIC_SUCCESS;
}
```

### 能力验证失败检测

```c
/**
 * @brief 检测能力验证失败攻击
 */
bool detect_cap_verification_attack(domain_id_t domain_id)
{
    domain_t *domain = &g_domains[domain_id];

    /* 检查失败率 */
    u64 total_attempts = domain->stats.cap_verify_success +
                        domain->stats.cap_verify_failure;
    if (total_attempts > 100) {
        u64 failure_rate = (domain->stats.cap_verify_failure * 100) / total_attempts;
        if (failure_rate > 90) {
            /* 失败率过高，可能是攻击 */
            return true;
        }
    }

    /* 检查失败频率 */
    if (domain->stats.cap_verify_failure > 1000) {
        /* 失败次数过多 */
        return true;
    }

    return false;
}
```

## 安全审计

### 审计日志

```c
/**
 * @brief 审计日志条目
 */
typedef struct {
    u64 timestamp;         /* 时间戳 */
    domain_id_t domain_id;  /* 域ID */
    thread_id_t thread_id;  /* 线程ID */
    audit_event_type_t type; /* 事件类型 */
    u32 data_len;          /* 数据长度 */
    u8 data[256];          /* 事件数据 */
} audit_log_entry_t;

/**
 * @brief 记录审计日志
 */
void audit_log(domain_id_t domain_id, thread_id_t thread_id,
               audit_event_type_t type, const void *data, u32 data_len)
{
    audit_log_entry_t *entry = get_next_audit_entry();

    entry->timestamp = get_timestamp();
    entry->domain_id = domain_id;
    entry->thread_id = thread_id;
    entry->type = type;
    entry->data_len = data_len < sizeof(entry->data) ? data_len : sizeof(entry->data);
    memcpy(entry->data, data, entry->data_len);

    /* 原子推进日志指针 */
    atomic_fetch_add(&g_audit_log.write_index, 1);
}
```

### 安全事件响应

```c
/**
 * @brief 处理安全事件
 */
void handle_security_event(audit_log_entry_t *event)
{
    switch (event->type) {
    case AUDIT_EVENT_SECURITY_VIOLATION:
        /* 安全违规：暂停域 */
        suspend_domain(event->domain_id);
        notify_monitor_service(event);
        break;

    case AUDIT_EVENT_CAP_VERIFY_FAILURE:
        /* 能力验证失败：检查是否是攻击 */
        if (detect_cap_verification_attack(event->domain_id)) {
            terminate_domain(event->domain_id);
        }
        break;

    case AUDIT_EVENT_EXCEPTION:
        /* 异常：检查是否是恶意异常 */
        if (is_malicious_exception(event)) {
            terminate_domain(event->domain_id);
        }
        break;

    default:
        break;
    }
}
```

## 形式化验证

### 不变式验证

```c
/**
 * @brief 验证能力守恒不变式
 * ∑capabilities_before = ∑capabilities_after
 */
bool verify_capability_conservation(void)
{
    u64 caps_before = 0;
    u64 caps_after = 0;

    /* 计算当前能力数 */
    for (cap_id_t i = 0; i < MAX_CAPABILITIES; i++) {
        if (g_cap_table[i].type != CAP_TYPE_INVALID &&
            !(g_cap_table[i].flags & CAP_FLAG_REVOKED)) {
            caps_before++;
        }
    }

    /* 模拟操作 */
    /* ... */

    /* 计算操作后能力数 */
    for (cap_id_t i = 0; i < MAX_CAPABILITIES; i++) {
        if (g_cap_table[i].type != CAP_TYPE_INVALID &&
            !(g_cap_table[i].flags & CAP_FLAG_REVOKED)) {
            caps_after++;
        }
    }

    /* 验证不变式 */
    return caps_before == caps_after;
}
```

### 类型安全

```c
/**
 * @brief 类型安全验证
 */
bool verify_type_safety(void)
{
    /* 验证所有能力类型一致性 */
    for (cap_id_t i = 0; i < MAX_CAPABILITIES; i++) {
        cap_entry_t *entry = &g_cap_table[i];

        if (entry->type == CAP_TYPE_INVALID) {
            continue;
        }

        /* 验证资源类型与能力类型匹配 */
        switch (entry->type) {
        case CAP_TYPE_MEMORY:
            /* 验证内存资源有效 */
            if (!is_valid_memory_range(entry->resource.memory.base,
                                      entry->resource.memory.size)) {
                return false;
            }
            break;

        case CAP_TYPE_IO_PORT:
            /* 验证I/O端口范围有效 */
            if (!is_valid_io_port_range(entry->resource.io_port.base,
                                        entry->resource.io_port.count)) {
                return false;
            }
            break;

        default:
            break;
        }
    }

    return true;
}
```

## 安全启动

### 信任链

```
硬件信任根 (TPM/Secure Boot)
    ↓
Bootloader签名验证
    ↓
内核签名验证
    ↓
模块签名验证
    ↓
服务启动
```

### 签名验证

```c
/**
 * @brief 验证模块签名
 */
status_t verify_module_signature(const u8 *module, u64 module_size,
                                const u8 *signature, u64 sig_size)
{
    /* 1. 计算模块哈希 */
    u8 hash[SHA384_DIGEST_SIZE];
    sha384(module, module_size, hash);

    /* 2. 验证签名 */
    status = rsa_verify_pss(&g_public_key, hash, sizeof(hash),
                           signature, sig_size);
    if (status != HIC_SUCCESS) {
        return HIC_ERROR_SIGNATURE_INVALID;
    }

    /* 3. 检查签名者证书 */
    status = verify_signer_certificate(signature, sig_size);
    if (status != HIC_SUCCESS) {
        return HIC_ERROR_CERTIFICATE_INVALID;
    }

    return HIC_SUCCESS;
}
```

## 安全监控

### 实时监控

```c
/**
 * @brief 安全监控器
 */
void security_monitor(void)
{
    while (true) {
        /* 1. 检查资源使用 */
        monitor_resource_usage();

        /* 2. 检查异常行为 */
        monitor_abnormal_behavior();

        /* 3. 检查安全事件 */
        monitor_security_events();

        /* 4. 生成报告 */
        generate_security_report();

        /* 等待下一个周期 */
        sleep(1000);  /* 1秒 */
    }
}
```

### 异常检测

```c
/**
 * @brief 检测异常行为
 */
bool detect_abnormal_behavior(domain_id_t domain_id)
{
    domain_t *domain = &g_domains[domain_id];

    /* 1. 检查CPU使用率 */
    if (domain->stats.cpu_usage > 90) {
        /* CPU使用率过高 */
        return true;
    }

    /* 2. 检查内存使用率 */
    u64 mem_usage = (domain->usage.memory_used * 100) / domain->quota.max_memory;
    if (mem_usage > 95) {
        /* 内存使用率过高 */
        return true;
    }

    /* 3. 检查系统调用频率 */
    if (domain->stats.syscall_rate > 10000) {
        /* 系统调用频率过高 */
        return true;
    }

    /* 4. 检查IPC调用频率 */
    if (domain->stats.ipc_rate > 1000) {
        /* IPC调用频率过高 */
        return true;
    }

    return false;
}
```

## 参考资料

- [能力系统](./11-CapabilitySystem.md)
- [形式化验证](./15-FormalVerification.md)
- [审计日志](./14-AuditLogging.md)

---

*最后更新: 2026-02-14*