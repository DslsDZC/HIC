# 安全架构

## 概述

HIK 采用深度防御（Defense-in-Depth）安全架构，通过多层安全机制提供全面的安全保障。该架构基于最小权限原则、形式化验证和数学证明，确保系统的安全性和可信性。

## 安全设计原则

### 1. 最小权限原则
- 每个组件只获得其功能所需的最小权限
- 能力系统实现细粒度访问控制
- 权限可动态授予和撤销

### 2. 纵向隔离
- 三层特权架构（Core-0, Privileged-1, Application-3）
- 每层只能访问下层提供的受控接口
- 域间强制隔离

### 3. 横向隔离
- 同层内不同服务/应用完全隔离
- 独立的内存空间和资源配额
- 能力系统控制跨域访问

### 4. 可验证性
- 数学形式化验证
- 运行时不变式检查
- 完整的审计日志

## 安全层次

### 第一层：硬件隔离

```
┌─────────────────────────────────────┐
│   CPU 特权级                        │
│   - Ring 0: Core-0                  │
│   - Ring 0 (沙箱): Privileged-1     │
│   - Ring 3: Application-3           │
└─────────────────────────────────────┘
```

**机制**:
- CPU 特权级强制执行访问控制
- 内存保护单元（MPU/MMU）
- I/O 端口访问控制

### 第二层：能力系统

```
┌─────────────────────────────────────┐
│   能力系统                          │
│   - 细粒度访问控制                  │
│   - 动态授予/撤销                   │
│   - 权限继承和派生                  │
└─────────────────────────────────────┘
```

**机制**:
- 不伪造、不可复制的令牌
- 显式授权
- 权限单调性保证

### 第三层：域隔离

```
┌─────────────────────────────────────┐
│   域隔离                            │
│   - 独立内存空间                    │
│   - 资源配额                        │
│   - 能力空间隔离                    │
└─────────────────────────────────────┘
```

**机制**:
- 物理内存隔离
- 能力隔离
- 调度隔离

### 第四层：形式化验证

```
┌─────────────────────────────────────┐
│   形式化验证                        │
│   - 数学证明                        │
│   - 运行时检查                      │
│   - 不变式监控                      │
└─────────────────────────────────────┘
```

**机制**:
- 7个核心数学定理
- 6个运行时不变式
- 自动违规检测

### 第五层：审计日志

```
┌─────────────────────────────────────┐
│   审计日志                          │
│   - 30种事件类型                    │
│   - 不可篡改记录                    │
│   - 实时监控                        │
└─────────────────────────────────────┘
```

**机制**:
- 30种审计事件
- 完整的系统操作记录
- 安全事件实时告警

## 安全机制

### 1. 能力系统安全

#### 能力不可伪造
```c
// 能力包含混淆令牌
typedef struct cap_handle {
    cap_id_t  cap_id;        // 全局能力ID
    u32       token;         // 混淆令牌
} cap_handle_t;

// 验证能力
bool validate_capability(cap_handle_t *handle) {
    cap_entry_t *entry = &g_cap_table[handle->cap_id];
    return entry->token == handle->token;
}
```

#### 权限单调性
```c
// 派生能力权限是父能力的子集
hik_status_t cap_derive(domain_id_t owner, cap_id_t parent, 
                        cap_rights_t sub_rights, cap_id_t *out) {
    cap_entry_t *parent_entry = &g_cap_table[parent];
    
    // 检查子权限是否为父权限的子集
    if ((sub_rights & parent_entry->rights) != sub_rights) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    // 创建派生能力
    return create_derived_capability(parent, sub_rights, out);
}
```

#### 能力撤销
```c
// 撤销能力及其所有派生
hik_status_t cap_revoke(cap_id_t cap) {
    cap_entry_t *entry = &g_cap_table[cap];
    
    // 标记为已撤销
    entry->flags |= CAP_FLAG_REVOKED;
    
    // 撤销所有派生能力
    revoke_all_derivatives(cap);
    
    return HIK_SUCCESS;
}
```

### 2. 内存隔离安全

#### 物理内存隔离
```c
// 每个域有独立的物理内存区域
typedef struct domain {
    phys_addr_t phys_base;  // 物理基地址
    size_t      phys_size;  // 物理大小
    // ...
} domain_t;

// 检查内存访问
bool check_memory_access(domain_id_t domain, phys_addr_t addr, size_t size) {
    domain_t *d = get_domain(domain);
    
    // 检查地址是否在域的内存范围内
    if (addr < d->phys_base || 
        addr + size > d->phys_base + d->phys_size) {
        return false;
    }
    
    return true;
}
```

#### 运行时验证
```c
// 内存隔离不变式
static bool invariant_memory_isolation(void) {
    for (domain_id_t d1 = 0; d1 < MAX_DOMAINS; d1++) {
        if (!domain_is_active(d1)) continue;
        
        for (domain_id_t d2 = d1 + 1; d2 < MAX_DOMAINS; d2++) {
            if (!domain_is_active(d2)) continue;
            
            mem_region_t r1 = get_domain_memory_region(d1);
            mem_region_t r2 = get_domain_memory_region(d2);
            
            // 检查内存区域是否重叠
            if (regions_overlap(&r1, &r2)) {
                return false;
            }
        }
    }
    return true;
}
```

### 3. 系统调用安全

#### 能力验证
```c
// 系统调用入口
void syscall_handler(u64 syscall_num, u64 arg1, u64 arg2, 
                     u64 arg3, u64 arg4) {
    // 获取当前域
    domain_id_t current_domain = get_current_domain();
    
    // 验证参数
    if (!validate_syscall_args(syscall_num, arg1, arg2, arg3, arg4)) {
        return;
    }
    
    // 检查权限
    if (!check_syscall_permission(current_domain, syscall_num)) {
        return;
    }
    
    // 执行系统调用
    hik_status_t status = execute_syscall(syscall_num, arg1, arg2, arg3, arg4);
    
    // 记录审计日志
    AUDIT_LOG_SYSCALL(current_domain, syscall_num, status == HIK_SUCCESS);
}
```

### 4. 形式化验证安全

#### 不变式检查
```c
// 检查所有不变式
int fv_check_all_invariants(void) {
    invariant_check_count++;
    
    for (u64 i = 0; i < num_invariants; i++) {
        if (!invariants[i].check()) {
            invariant_violation_count++;
            
            // 记录违反
            fv_log_violation(&invariants[i]);
            
            return FV_CAP_INVALID + i;
        }
    }
    
    return FV_SUCCESS;
}
```

#### 数学保证
- 能力守恒定理
- 内存隔离定理
- 权限单调性定理
- 资源配额守恒定理
- 无死锁定理
- 类型安全性定理
- 系统调用原子性定理

### 5. 审计日志安全

#### 事件记录
```c
// 审计事件类型
typedef enum {
    AUDIT_EVENT_CAP_CREATE,
    AUDIT_EVENT_CAP_TRANSFER,
    AUDIT_EVENT_CAP_REVOKE,
    AUDIT_EVENT_CAP_VERIFY,
    AUDIT_EVENT_DOMAIN_CREATE,
    AUDIT_EVENT_DOMAIN_DESTROY,
    AUDIT_EVENT_SYSCALL,
    AUDIT_EVENT_IRQ,
    AUDIT_EVENT_IPC_CALL,
    AUDIT_EVENT_EXCEPTION,
    AUDIT_EVENT_SECURITY_VIOLATION,
    // ... 更多事件类型
} audit_event_type_t;

// 记录审计事件
void audit_log_event(audit_event_type_t type, domain_id_t domain,
                     cap_id_t cap, thread_id_t thread,
                     u64 *data, u32 data_len, bool result) {
    audit_entry_t entry = {
        .timestamp = get_system_time_ns(),
        .type = type,
        .domain = domain,
        .cap = cap,
        .thread = thread,
        .result = result
    };
    
    // 写入审计日志
    write_audit_entry(&entry);
}
```

#### 安全事件监控
```c
// 监控服务
void monitor_service_loop(void) {
    while (1) {
        monitor_event_t event;
        
        // 检查审计日志
        if (check_audit_log(&event)) {
            handle_security_event(&event);
        }
        
        // 检查系统状态
        if (check_system_health()) {
            handle_system_event();
        }
        
        // 延迟
        hal_udelay(1000000);  // 1秒
    }
}
```

## 威胁模型

### 1. 内部威胁
- **恶意应用**: 通过能力系统隔离
- **特权服务滥用**: 通过域隔离限制
- **内核漏洞**: 通过形式化验证最小化

### 2. 外部威胁
- **物理攻击**: 通过 TPM 和安全启动保护
- **侧信道攻击**: 通过隔离和审计检测
- **供应链攻击**: 通过模块签名验证

### 3. 系统威胁
- **拒绝服务**: 通过配额和监控防护
- **信息泄露**: 通过隔离和加密保护
- **权限提升**: 通过能力系统防止

## 安全配置

### 安全策略

```c
// 安全策略配置
typedef struct security_policy {
    bool enforce_capability_monotonicity;  // 强制权限单调性
    bool enforce_memory_isolation;        // 强制内存隔离
    bool enable_formal_verification;     // 启用形式化验证
    bool enable_audit_logging;            // 启用审计日志
    bool enable_secure_boot;             // 启用安全启动
    u64  audit_log_size;                 // 审计日志大小
} security_policy_t;

// 应用安全策略
void apply_security_policy(security_policy_t *policy) {
    if (policy->enable_formal_verification) {
        fv_init();
    }
    
    if (policy->enable_audit_logging) {
        audit_system_init(policy->audit_log_size);
    }
    
    if (policy->enforce_capability_monotonicity) {
        enable_capability_monotonicity_check();
    }
}
```

## 安全最佳实践

1. **最小权限**: 只授予必需的能力
2. **及时撤销**: 不再需要的能力立即撤销
3. **隔离设计**: 服务和应用完全隔离
4. **审计监控**: 启用完整的审计日志
5. **定期验证**: 运行形式化验证检查
6. **安全启动**: 启用 TPM 和安全启动
7. **模块签名**: 只加载签名的模块
8. **持续监控**: 实时监控系统状态

## 相关文档

- [能力系统](./11-CapabilitySystem.md) - 能力系统详解
- [形式化验证](./15-FormalVerification.md) - 形式化验证
- [审计日志](./14-AuditLogging.md) - 审计机制
- [安全启动](./16-SecureBoot.md) - 安全启动

---

*最后更新: 2026-02-14*