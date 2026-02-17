/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 简洁安全实现
 * 平衡：简洁性 + 安全性 + 性能
 * 目标：验证速度 < 5ns @ 3GHz
 * 
 * 安全保证（TD文档）：
 * - 域间句柄不可推导（域特定密钥）
 * - 句柄不可伪造（混淆令牌）
 * - 撤销立即生效
 * 
 * 性能优化：
 * - 轻量级混淆（~5条指令）
 * - 内联验证函数
 * - 缓存行对齐
 */

#include "capability.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/console.h"

/* ==================== 全局能力表 ==================== */
cap_entry_t g_global_cap_table[CAP_TABLE_SIZE];

/* ==================== 域密钥表（每个域一个） ==================== */
domain_key_t g_domain_keys[HIC_DOMAIN_MAX];

/* ==================== 初始化 ==================== */

void capability_system_init(void) {
    memzero(g_global_cap_table, sizeof(g_global_cap_table));
    memzero(g_domain_keys, sizeof(g_domain_keys));
    
    /* 为 Core-0 初始化密钥 */
    g_domain_keys[HIC_DOMAIN_CORE].seed = 0x12345678;
    g_domain_keys[HIC_DOMAIN_CORE].multiplier = 0x9E3779B9;
    
    console_puts("[CAP] Capability system initialized (secure & fast, < 5ns)\n");
}

/* 初始化域密钥（使用伪随机数） */
void cap_init_domain_key(domain_id_t domain) {
    if (domain >= HIC_DOMAIN_MAX) {
        return;
    }
    
    /* 使用域ID和时间戳生成伪随机密钥 */
    extern u64 hal_get_timestamp(void);
    u64 ts = hal_get_timestamp();
    
    g_domain_keys[domain].seed = (u32)(ts ^ (domain * 0x9E3779B9));
    g_domain_keys[domain].multiplier = 0x9E3779B9 + domain;
    
    console_puts("[CAP] Domain key initialized: ");
    console_putu64(domain);
    console_puts("\n");
}

/* ==================== 能力创建 ==================== */

hic_status_t cap_create_memory(domain_id_t owner, phys_addr_t base, 
                               size_t size, cap_rights_t rights, cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    /* 快速查找空闲槽位 */
    cap_id_t cap = HIC_CAP_INVALID;
    for (u32 i = 1; i < CAP_TABLE_SIZE && cap == HIC_CAP_INVALID; i++) {
        if (g_global_cap_table[i].cap_id == 0) {
            cap = i;
        }
    }
    
    if (cap == HIC_CAP_INVALID) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 初始化能力 */
    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = rights;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].flags = 0;
    g_global_cap_table[cap].memory.base = base;
    g_global_cap_table[cap].memory.size = size;
    
    atomic_exit_critical(irq);
    
    *out = cap;
    return HIC_SUCCESS;
}

hic_status_t cap_create_endpoint(domain_id_t owner, domain_id_t target, cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || target >= HIC_DOMAIN_MAX || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    cap_id_t cap = HIC_CAP_INVALID;
    for (u32 i = 1; i < CAP_TABLE_SIZE && cap == HIC_CAP_INVALID; i++) {
        if (g_global_cap_table[i].cap_id == 0) {
            cap = i;
        }
    }
    
    if (cap == HIC_CAP_INVALID) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_MEMORY;
    }
    
    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = 0x01;  /* CALL permission */
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].flags = 0;
    g_global_cap_table[cap].endpoint.target = target;
    
    atomic_exit_critical(irq);
    
    *out = cap;
    return HIC_SUCCESS;
}

/* ==================== 能力授予（返回混淆句柄） ==================== */

hic_status_t cap_grant(domain_id_t domain, cap_id_t cap, cap_handle_t *out) {
    if (domain >= HIC_DOMAIN_MAX || cap >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查能力是否存在 */
    if (g_global_cap_table[cap].cap_id != cap) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 生成混淆句柄 */
    cap_handle_t handle = cap_make_handle(domain, cap);
    
    *out = handle;
    return HIC_SUCCESS;
}

/* ==================== 能力撤销 ==================== */

hic_status_t cap_revoke(cap_id_t cap) {
    if (cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    g_global_cap_table[cap].flags |= CAP_FLAG_REVOKED;
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/* ==================== 能力传递（创建新句柄） ==================== */

hic_status_t cap_transfer(domain_id_t from, domain_id_t to, cap_id_t cap, cap_handle_t *out) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX || 
        cap >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (g_global_cap_table[cap].cap_id != cap) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    if (g_global_cap_table[cap].owner != from) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 改变所有者 */
    g_global_cap_table[cap].owner = to;
    
    /* 为目标域生成新句柄 */
    return cap_grant(to, cap, out);
}

/* ==================== 能力派生 ==================== */

hic_status_t cap_derive(domain_id_t owner, cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || parent >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查父能力是否存在 */
    if (g_global_cap_table[parent].cap_id != parent || 
        (g_global_cap_table[parent].flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查所有权 */
    if (g_global_cap_table[parent].owner != owner) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 查找空闲能力槽 */
    bool irq = atomic_enter_critical();
    
    cap_id_t cap = HIC_CAP_INVALID;
    for (u32 i = 0; i < CAP_TABLE_SIZE; i++) {
        if (g_global_cap_table[i].cap_id != i) {
            cap = i;
            break;
        }
    }
    
    if (cap == HIC_CAP_INVALID) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 初始化派生能力 */
    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = sub_rights;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].flags = 0;
    
    /* 复制父能力的数据 */
    g_global_cap_table[cap].memory = g_global_cap_table[parent].memory;
    
    atomic_exit_critical(irq);
    
    *out = cap;
    return HIC_SUCCESS;
}

/* ==================== 能力验证（完整版本） ==================== */

hic_status_t cap_check_access(domain_id_t domain, cap_handle_t handle, cap_rights_t required) {
    if (handle == CAP_HANDLE_INVALID) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    cap_id_t cap_id = cap_get_cap_id(handle);
    
    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 内存屏障确保读取顺序 */
    atomic_acquire_barrier();
    
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    /* 检查能力ID */
    if (entry->cap_id != cap_id) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查是否撤销 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        return HIC_ERROR_CAP_REVOKED;
    }
    
    /* 检查权限 */
    if ((entry->rights & required) != required) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 验证令牌（确保句柄属于该域） */
    u32 token = cap_get_token(handle);
    if (!cap_validate_token(domain, cap_id, token)) {
        return HIC_ERROR_PERMISSION;
    }
    
    return HIC_SUCCESS;
}

/* ==================== 特权内存访问通道实现（增强安全版） ==================== */

/* Core-0 内存区域（绝对禁止访问） */
phys_addr_t g_core0_mem_start = 0x00000000;
phys_addr_t g_core0_mem_end   = 0x00FFFFFF;

/* 可用物理内存范围（防止访问未映射区域） */
phys_addr_t g_usable_memory_start = 0x01000000;
phys_addr_t g_usable_memory_end   = 0xFFFFFFFF;

/* 特权域位图（运行时验证，防止标志位被篡改） */
u32 g_privileged_domain_bitmap[HIC_DOMAIN_MAX / 32];

/* 特权访问审计计数器（用于监控） */
u64 g_privileged_access_count[HIC_DOMAIN_MAX];

/* 检查域是否为特权域（使用位图快速查找） */
bool cap_is_privileged_domain(domain_id_t domain) {
    if (domain >= HIC_DOMAIN_MAX) {
        return false;
    }
    
    u32 bitmap_index = domain / 32;
    u32 bitmap_bit = 1U << (domain % 32);
    
    return (g_privileged_domain_bitmap[bitmap_index] & bitmap_bit) != 0;
}

/* 设置特权域标志 */
void cap_set_privileged_domain(domain_id_t domain, bool privileged) {
    if (domain >= HIC_DOMAIN_MAX) {
        return;
    }
    
    bool irq = atomic_enter_critical();
    
    u32 bitmap_index = domain / 32;
    u32 bitmap_bit = 1U << (domain % 32);
    
    if (privileged) {
        g_privileged_domain_bitmap[bitmap_index] |= bitmap_bit;
    } else {
        g_privileged_domain_bitmap[bitmap_index] &= ~bitmap_bit;
    }
    
    atomic_exit_critical(irq);
}

/* 检查域是否为特权域（兼容旧接口） */
bool is_privileged_domain(domain_id_t domain) {
    return cap_is_privileged_domain(domain);
}

/* 特权物理地址转虚拟地址（恒等映射） */
void *privileged_phys_to_virt(phys_addr_t addr) {
    return (void *)addr;
}

/* 特权内存访问检查（完整版本，带审计） */
bool privileged_check_access(domain_id_t domain, phys_addr_t addr, cap_rights_t access_type __attribute__((unused))) {
    /* 1. 检查是否在 Core-0 区域 */
    if (addr >= g_core0_mem_start && addr < g_core0_mem_end) {
        return false;
    }
    
    /* 2. 检查是否在可用内存范围 */
    if (addr < g_usable_memory_start || addr >= g_usable_memory_end) {
        return false;
    }
    
    /* 3. 检查域是否为特权域（运行时验证） */
    if (!cap_is_privileged_domain(domain)) {
        return false;
    }
    
    /* 4. 记录访问计数（审计） */
    g_privileged_access_count[domain]++;
    
    return true;
}

/* ==================== 能力辅助函数 ==================== */

/* 检查能力是否存在 */
bool capability_exists(cap_id_t cap) {
    if (cap >= CAP_TABLE_SIZE) {
        return false;
    }
    return g_global_cap_table[cap].cap_id == cap && 
           !(g_global_cap_table[cap].flags & CAP_FLAG_REVOKED);
}

/* 获取能力权限 */
cap_rights_t get_capability_permissions(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return 0;
    }
    return g_global_cap_table[cap].rights;
}

/* 获取能力对象类型 */
u32 get_capability_object_type(cap_id_t cap) {
    /* 简化实现：返回能力类型 */
    if (!capability_exists(cap)) {
        return 0;
    }
    
    /* 根据权限判断类型 */
    cap_rights_t rights = g_global_cap_table[cap].rights;
    if (rights & CAP_MEM_DEVICE) {
        return 2; /* MMIO */
    } else {
        return 1; /* Memory */
    }
}

/* 获取能力类型 */
u32 get_capability_type(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return 0;
    }
    return g_global_cap_table[cap].rights;
}

/* 获取能力派生信息 */
hic_status_t get_capability_derivatives(cap_id_t cap, cap_id_t *derivatives, u32 *count) {
    if (!capability_exists(cap) || derivatives == NULL || count == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 简化实现：返回无派生 */
    *count = 0;
    return HIC_SUCCESS;
}
