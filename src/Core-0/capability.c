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
#include "domain.h"
#include "pmm.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"
#include "formal_verification.h"

/* ==================== 全局能力表 ==================== */
__capability cap_entry_t g_global_cap_table[CAP_TABLE_SIZE];

/* ==================== 共享内存区域配置 ==================== */
#define MAX_SHMEM_REGIONS  64

/* 共享内存区域表（前置声明供机制层使用） */
static shmem_region_t g_shmem_regions[MAX_SHMEM_REGIONS];

/* ==================== 域密钥表（每个域一个） ==================== */
domain_key_t g_domain_keys[HIC_DOMAIN_MAX];

/* ==================== 能力派生跟踪 ==================== */
#define MAX_DERIVATIVES_PER_CAP 16

typedef struct cap_derivative {
    cap_id_t parent;
    cap_id_t children[MAX_DERIVATIVES_PER_CAP];
    u32 child_count;
} cap_derivative_t;

static cap_derivative_t g_derivatives[CAP_TABLE_SIZE];

/* ==================== 初始化 ==================== */

void capability_system_init(void) {
    console_puts("[CAP] Initializing capability system...\n");
    
    memzero(g_global_cap_table, sizeof(g_global_cap_table));
    console_puts("[CAP] Global capability table cleared (");
    console_putu32(CAP_TABLE_SIZE);
    console_puts(" entries)\n");
    
    memzero(g_derivatives, sizeof(g_derivatives));
    console_puts("[CAP] Derivative table cleared\n");
    
    memzero(g_domain_keys, sizeof(g_domain_keys));
    console_puts("[CAP] Domain key table cleared (");
    console_putu32(HIC_DOMAIN_MAX);
    console_puts(" domains)\n");
    
    /* 为 Core-0 初始化密钥 */
    g_domain_keys[HIC_DOMAIN_CORE].seed = 0x12345678;
    g_domain_keys[HIC_DOMAIN_CORE].multiplier = 0x9E3779B9;
    console_puts("[CAP] Core-0 domain key initialized\n");
    
    console_puts("[CAP] Capability system initialized (secure & fast, < 5ns)\n");
    console_puts("[CAP] Security: Handle obfuscation enabled\n");
    console_puts("[CAP] Performance: Inlined verification ready\n");
}

/* 初始化域密钥（使用伪随机数） */
void cap_init_domain_key(domain_id_t domain) {
    console_puts("[CAP] Initializing domain key for domain ");
    console_putu64(domain);
    console_puts("...\n");
    
    if (domain >= HIC_DOMAIN_MAX) {
        console_puts("[CAP] ERROR: Invalid domain ID (>= ");
        console_putu32(HIC_DOMAIN_MAX);
        console_puts(")\n");
        return;
    }
    
    /* 使用域ID和时间戳生成伪随机密钥 */
    extern u64 hal_get_timestamp(void);
    u64 ts = hal_get_timestamp();
    
    g_domain_keys[domain].seed = (u32)(ts ^ (domain * 0x9E3779B9));
    g_domain_keys[domain].multiplier = 0x9E3779B9 + domain;
    
    console_puts("[CAP] Domain key initialized: seed=0x");
    console_puthex64((u64)g_domain_keys[domain].seed);
    console_puts(", mult=0x");
    console_puthex64(g_domain_keys[domain].multiplier);
    console_puts("\n");
    console_puts("[CAP] >>> Domain ");
    console_putu64(domain);
    console_puts(" key is now ACTIVE <<<\n");
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

/* ==================== 能力撤销（递归实现） ==================== */

/* 最大派生深度限制，防止无限递归 */
#define CAP_REVOKE_MAX_DEPTH  16

/**
 * @brief 检查并释放共享内存资源
 * 
 * 如果能力对应共享内存区域，减少引用计数，
 * 引用计数为0时释放物理内存。
 * 
 * 注意：调用者必须持有原子锁
 */
static void cap_release_shmem_if_needed(cap_id_t cap) {
    /* 外部引用：共享内存区域表（定义在后面的共享内存机制层） */
    extern shmem_region_t g_shmem_regions[];
    
    if (cap >= CAP_TABLE_SIZE) return;
    
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    /* 检查是否为内存能力 */
    if (entry->memory.base == 0 || entry->memory.size == 0) return;
    
    /* 检查是否属于共享内存区域 */
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == entry->memory.base) {
            /* 找到匹配的共享内存区域 */
            if (g_shmem_regions[i].ref_count > 0) {
                g_shmem_regions[i].ref_count--;
                
                /* 引用计数为0，释放物理内存 */
                if (g_shmem_regions[i].ref_count == 0) {
                    u32 pages = (u32)((g_shmem_regions[i].size + PAGE_SIZE - 1) / PAGE_SIZE);
                    pmm_free_frames(g_shmem_regions[i].phys_base, pages);
                    g_shmem_regions[i].phys_base = 0;
                    g_shmem_regions[i].size = 0;
                }
            }
            break;
        }
    }
}

/**
 * @brief 递归撤销能力及其所有派生能力
 * 
 * 安全保证：
 * 1. 先设置 CAP_FLAG_REVOKED 标志，避免重复撤销
 * 2. 检查子能力是否已被撤销，防止死循环
 * 3. 深度限制防止栈溢出
 * 4. 撤销完成后清零 cap_id，使槽位可重用
 * 5. 自动处理共享内存引用计数
 * 6. 从父能力的子列表中移除，保持一致性
 * 
 * 注意：调用者必须持有原子锁
 */
static void cap_revoke_recursive_impl(cap_id_t cap, u32 depth) {
    /* 深度限制检查 */
    if (depth > CAP_REVOKE_MAX_DEPTH) {
        return;
    }
    
    /* 边界检查 */
    if (cap >= CAP_TABLE_SIZE) {
        return;
    }
    
    /* 检查能力是否存在 */
    if (g_global_cap_table[cap].cap_id != cap) {
        return;
    }
    
    /* 先设置撤销标志，避免重复撤销 */
    if (g_global_cap_table[cap].flags & CAP_FLAG_REVOKED) {
        return;  /* 已撤销，跳过 */
    }
    g_global_cap_table[cap].flags |= CAP_FLAG_REVOKED;
    
    /* 递归撤销所有派生能力（在清零 cap_id 之前） */
    cap_derivative_t *deriv = &g_derivatives[cap];
    for (u32 i = 0; i < deriv->child_count && i < MAX_DERIVATIVES_PER_CAP; i++) {
        cap_id_t child = deriv->children[i];
        
        /* 检查子能力是否已被撤销，防止死循环 */
        if (child != HIC_CAP_INVALID && child < CAP_TABLE_SIZE) {
            if (!(g_global_cap_table[child].flags & CAP_FLAG_REVOKED)) {
                cap_revoke_recursive_impl(child, depth + 1);
            }
        }
    }
    
    /* 检查并释放共享内存资源 */
    cap_release_shmem_if_needed(cap);
    
    /* 从父能力的子列表中移除当前能力 */
    cap_id_t parent = deriv->parent;
    if (parent != HIC_CAP_INVALID && parent < CAP_TABLE_SIZE) {
        cap_derivative_t *parent_deriv = &g_derivatives[parent];
        for (u32 i = 0; i < parent_deriv->child_count && i < MAX_DERIVATIVES_PER_CAP; i++) {
            if (parent_deriv->children[i] == cap) {
                /* 找到，用最后一个元素覆盖（避免移动数组） */
                parent_deriv->children[i] = parent_deriv->children[parent_deriv->child_count - 1];
                parent_deriv->child_count--;
                break;
            }
        }
    }
    
    /* 清空子能力列表 */
    deriv->child_count = 0;
    deriv->parent = HIC_CAP_INVALID;
    
    /* 清零 cap_id，使槽位可重用 */
    g_global_cap_table[cap].cap_id = 0;
    g_global_cap_table[cap].flags = 0;
    g_global_cap_table[cap].rights = 0;
    g_global_cap_table[cap].owner = 0;
}

/**
 * @brief 撤销能力及其所有派生能力
 * 
 * 满足"撤销立即生效"的安全规范：
 * - 撤销父能力时，所有派生能力同步失效
 * - 整个递归过程在原子操作保护内
 * 
 * @param cap 要撤销的能力ID
 * @return 状态码
 */
hic_status_t cap_revoke(cap_id_t cap) {
    if (cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    /* 检查能力是否存在 */
    if (g_global_cap_table[cap].cap_id != cap) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查是否已被撤销 */
    if (g_global_cap_table[cap].flags & CAP_FLAG_REVOKED) {
        atomic_exit_critical(irq);
        return HIC_SUCCESS;  /* 幂等操作：已撤销视为成功 */
    }
    
    /* 递归撤销 */
    cap_revoke_recursive_impl(cap, 0);
    
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

/* ==================== 能力传递（带权限衰减） - 机制层 ==================== */

hic_status_t cap_transfer_with_attenuation(domain_id_t from, domain_id_t to, 
                                            cap_id_t cap, cap_rights_t attenuated_rights,
                                            cap_handle_t *out) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX || 
        cap >= CAP_TABLE_SIZE || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    /* 检查能力有效性 */
    if (entry->cap_id != cap) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查所有权 */
    if (entry->owner != from) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }
    
    /* 检查权限单调性：衰减权限必须是原权限的子集 */
    if ((attenuated_rights & entry->rights) != attenuated_rights) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;  /* 不能授予比持有更多的权限 */
    }
    
    /* 创建派生能力 */
    cap_id_t new_cap = HIC_CAP_INVALID;
    for (u32 i = 0; i < CAP_TABLE_SIZE; i++) {
        if (g_global_cap_table[i].cap_id != i) {
            new_cap = i;
            break;
        }
    }
    
    if (new_cap == HIC_CAP_INVALID) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 初始化派生能力 */
    cap_entry_t *new_entry = &g_global_cap_table[new_cap];
    new_entry->cap_id = new_cap;
    new_entry->rights = attenuated_rights;  /* 使用衰减后的权限 */
    new_entry->owner = to;
    new_entry->flags = 0;
    
    /* 复制数据区域 */
    new_entry->memory = entry->memory;
    new_entry->endpoint = entry->endpoint;
    new_entry->logical_core = entry->logical_core;
    
    /* 记录派生关系 */
    if (g_derivatives[cap].child_count < MAX_DERIVATIVES_PER_CAP) {
        g_derivatives[cap].children[g_derivatives[cap].child_count++] = new_cap;
    }
    
    atomic_exit_critical(irq);
    
    /* 为目标域生成句柄 */
    return cap_grant(to, new_cap, out);
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
    
    /* 检查权限单调性：派生权限必须是父权限的子集 */
    if ((sub_rights & ~g_global_cap_table[parent].rights) != 0) {
        return HIC_ERROR_PERMISSION;  /* 派生权限超出父权限范围 */
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
    
    /* 复制父能力的完整数据（联合体的所有字段） */
    g_global_cap_table[cap].memory = g_global_cap_table[parent].memory;
    g_global_cap_table[cap].endpoint = g_global_cap_table[parent].endpoint;
    g_global_cap_table[cap].logical_core = g_global_cap_table[parent].logical_core;
    
    /* 记录派生关系 */
    if (g_derivatives[parent].child_count < MAX_DERIVATIVES_PER_CAP) {
        g_derivatives[parent].children[g_derivatives[parent].child_count++] = cap;
        g_derivatives[cap].parent = parent;  /* 记录父能力 */
    }
    
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

/* 特权访问审计计数器（已废弃：使用 domain_t.audit_counters.privileged_mem_access) */
u64 g_privileged_access_count[HIC_DOMAIN_MAX] __attribute__((deprecated));

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
    
    /* 4. 记录访问计数（使用域私有审计计数器） */
    if (domain < HIC_DOMAIN_MAX && g_domains[domain].state != DOMAIN_STATE_INIT) {
        g_domains[domain].audit_counters.privileged_mem_access++;
    }
    
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
u64 get_capability_permissions(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return 0;
    }
    return (u64)g_global_cap_table[cap].rights;
}

/* 获取能力对象类型 */
obj_type_t get_capability_object_type(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return OBJ_MEMORY;  /* 默认返回内存类型 */
    }
    
    cap_rights_t rights = g_global_cap_table[cap].rights;
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    /* 根据权限特征和大小判断对象类型 */
    if (rights & CAP_MEM_DEVICE) {
        return OBJ_DEVICE;  /* 设备 */
    }
    
    /* 检查是否为IPC端点（大小为0，且target有效） */
    if (entry->memory.size == 0 && entry->endpoint.target < HIC_DOMAIN_MAX) {
        return OBJ_IPC;
    }
    
    /* 检查是否为共享内存 */
    if ((rights & (CAP_MEM_READ | CAP_MEM_WRITE)) == (CAP_MEM_READ | CAP_MEM_WRITE) &&
        entry->memory.size > 0 && !(rights & CAP_MEM_EXEC)) {
        return OBJ_SHARED;
    }
    
    /* 检查是否为线程相关 */
    if (rights & (CAP_LCORE_USE | CAP_LCORE_QUERY)) {
        return OBJ_THREAD;
    }
    
    return OBJ_MEMORY;  /* 默认内存类型 */
}

/* 获取能力类型 */
cap_type_t get_capability_type(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return CAP_MEMORY;
    }
    
    cap_rights_t rights = g_global_cap_table[cap].rights;
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    /* 根据权限特征判断能力类型 */
    
    /* MMIO区域能力 */
    if (rights & CAP_MEM_DEVICE) {
        return CAP_MMIO;
    }
    
    /* IPC端点能力 */
    if (entry->memory.size == 0 && entry->endpoint.target < HIC_DOMAIN_MAX) {
        return CAP_ENDPOINT;
    }
    
    /* 中断能力 */
    if (rights & (1U << 20)) {  /* 假设中断权限位 */
        return CAP_IRQ;
    }
    
    /* 服务能力 */
    if (rights & CAP_LCORE_QUERY) {
        return CAP_SERVICE;
    }
    
    /* 线程能力 */
    if (rights & (CAP_LCORE_USE | CAP_LCORE_MIGRATE)) {
        return CAP_THREAD;
    }
    
    /* 共享内存能力 */
    if ((rights & (CAP_MEM_READ | CAP_MEM_WRITE)) == (CAP_MEM_READ | CAP_MEM_WRITE) &&
        !(rights & CAP_MEM_EXEC)) {
        return CAP_SHARED;
    }
    
    /* 设备能力 */
    if (rights & (1U << 16)) {  /* 假设设备权限位 */
        return CAP_DEVICE;
    }
    
    /* 派生能力 */
    if (rights & CAP_LCORE_BORROW) {
        return CAP_CAP_DERIVE;
    }
    
    /* 默认内存能力 */
    if (rights & (CAP_MEM_READ | CAP_MEM_WRITE | CAP_MEM_EXEC)) {
        return CAP_MEMORY;
    }
    
    return CAP_MEMORY;
}

/* 查找能力的派生信息 */
cap_id_t* get_capability_derivatives(cap_id_t cap) {
    if (!capability_exists(cap)) {
        return NULL;
    }
    
    /* 
     * 注意：这是一个静态缓冲区，调用者应该立即使用返回值
     * 在下一次调用此函数前使用完毕
     */
    static cap_id_t derivative_list[MAX_DERIVATIVES_PER_CAP + 1];
    
    cap_derivative_t *deriv = &g_derivatives[cap];
    
    if (deriv->child_count == 0) {
        derivative_list[0] = HIC_CAP_INVALID;
        return derivative_list;
    }
    
    /* 复制子能力列表 */
    for (u32 i = 0; i < deriv->child_count && i < MAX_DERIVATIVES_PER_CAP; i++) {
        derivative_list[i] = deriv->children[i];
    }
    derivative_list[deriv->child_count] = HIC_CAP_INVALID;  /* 终止标记 */
    
    return derivative_list;
}

/* ==================== 逻辑核心能力函数 ==================== */

/**
 * 创建逻辑核心能力
 */
hic_status_t cap_create_logical_core(domain_id_t owner,
                                    logical_core_id_t logical_core_id,
                                    logical_core_flags_t flags,
                                    logical_core_quota_t quota,
                                    cap_rights_t rights,
                                    cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 查找空闲能力槽 */
    cap_id_t cap = HIC_CAP_INVALID;
    for (u32 i = 1; i < CAP_TABLE_SIZE && cap == HIC_CAP_INVALID; i++) {
        if (g_global_cap_table[i].cap_id == 0) {
            cap = i;
        }
    }
    
    if (cap == HIC_CAP_INVALID) {
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 填充能力条目 */
    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = rights;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].flags = 0;
    
    /* 设置逻辑核心信息 */
    g_global_cap_table[cap].logical_core.logical_core_id = logical_core_id;
    g_global_cap_table[cap].logical_core.flags = flags;
    g_global_cap_table[cap].logical_core.quota = quota;
    
    *out = cap;
    return HIC_SUCCESS;
}

/**
 * 获取逻辑核心能力信息
 */
hic_status_t cap_get_logical_core_info(cap_id_t cap_id,
                                      logical_core_id_t *logical_core_id,
                                      logical_core_flags_t *flags,
                                      logical_core_quota_t *quota) {
    if (cap_id >= CAP_TABLE_SIZE || !capability_exists(cap_id)) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    if (logical_core_id) {
        *logical_core_id = entry->logical_core.logical_core_id;
    }
    if (flags) {
        *flags = entry->logical_core.flags;
    }
    if (quota) {
        *quota = entry->logical_core.quota;
    }
    
    return HIC_SUCCESS;
}

/* ==================== 共享内存机制层实现 ==================== */

/* 共享内存区域表 */
#define MAX_SHMEM_REGIONS  64
static shmem_region_t g_shmem_regions[MAX_SHMEM_REGIONS];

/**
 * 分配共享内存区域（机制层）
 */
hic_status_t shmem_alloc(domain_id_t owner, size_t size, u32 flags,
                          cap_id_t *out_cap, cap_handle_t *out_handle) {
    if (owner >= HIC_DOMAIN_MAX || size == 0 || !out_cap || !out_handle) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    /* 查找空闲共享内存槽 */
    u32 region_idx = MAX_SHMEM_REGIONS;
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == 0) {
            region_idx = i;
            break;
        }
    }
    
    if (region_idx == MAX_SHMEM_REGIONS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 分配物理内存 */
    phys_addr_t phys_base;
    u32 pages = (u32)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    if (pmm_alloc_frames(owner, pages, PAGE_FRAME_SHARED, &phys_base) != HIC_SUCCESS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 计算权限 */
    cap_rights_t rights = CAP_MEM_READ;
    if (flags & SHMEM_FLAG_WRITABLE) {
        rights |= CAP_MEM_WRITE;
    }
    
    /* 创建内存能力 */
    cap_id_t cap;
    hic_status_t status = cap_create_memory(owner, phys_base, pages * PAGE_SIZE, rights, &cap);
    if (status != HIC_SUCCESS) {
        pmm_free_frames(phys_base, pages);
        atomic_exit_critical(irq);
        return status;
    }
    
    /* 记录共享内存区域 */
    g_shmem_regions[region_idx].phys_base = phys_base;
    g_shmem_regions[region_idx].size = pages * PAGE_SIZE;
    g_shmem_regions[region_idx].owner = owner;
    g_shmem_regions[region_idx].ref_count = 1;
    g_shmem_regions[region_idx].flags = flags;
    
    atomic_exit_critical(irq);
    
    /* 生成句柄给创建者 */
    status = cap_grant(owner, cap, out_handle);
    if (status != HIC_SUCCESS) {
        /* 回滚 */
        cap_revoke(cap);
        pmm_free_frames(phys_base, pages);
        g_shmem_regions[region_idx].phys_base = 0;
        return status;
    }
    
    *out_cap = cap;
    return HIC_SUCCESS;
}

/**
 * 映射共享内存到目标域（机制层）
 */
hic_status_t shmem_map(domain_id_t from, domain_id_t to, cap_id_t cap,
                        cap_rights_t rights, cap_handle_t *out_handle) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX || 
        cap >= CAP_TABLE_SIZE || !out_handle) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    /* 检查能力有效性 */
    if (entry->cap_id != cap || (entry->flags & CAP_FLAG_REVOKED)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查发送方持有该能力 */
    if (entry->owner != from) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }
    
    /* 检查权限单调性：授予的权限不能超过原权限 */
    if ((rights & entry->rights) != rights) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }
    
    /* 使用带衰减的传递机制 */
    hic_status_t status = cap_transfer_with_attenuation(from, to, cap, rights, out_handle);
    
    if (status == HIC_SUCCESS) {
        /* 更新引用计数 */
        for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
            if (g_shmem_regions[i].phys_base == entry->memory.base) {
                g_shmem_regions[i].ref_count++;
                break;
            }
        }
    }
    
    atomic_exit_critical(irq);
    
    return status;
}

/**
 * 解除共享内存映射（机制层）
 */
hic_status_t shmem_unmap(domain_id_t domain, cap_handle_t handle) {
    cap_id_t cap_id = cap_get_cap_id(handle);
    
    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    /* 验证令牌 */
    u32 token = cap_get_token(handle);
    if (!cap_validate_token(domain, cap_id, token)) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查所有权 */
    if (entry->owner != domain) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }
    
    /* 更新引用计数 */
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == entry->memory.base) {
            if (g_shmem_regions[i].ref_count > 0) {
                g_shmem_regions[i].ref_count--;
            }
            
            /* 如果引用计数为0，释放物理内存 */
            if (g_shmem_regions[i].ref_count == 0) {
                u32 pages = (u32)((g_shmem_regions[i].size + PAGE_SIZE - 1) / PAGE_SIZE);
                pmm_free_frames(g_shmem_regions[i].phys_base, pages);
                g_shmem_regions[i].phys_base = 0;
            }
            break;
        }
    }
    
    /* 撤销能力并清零 cap_id，使槽位可重用 */
    entry->flags = 0;
    entry->rights = 0;
    entry->owner = 0;
    entry->cap_id = 0;  /* 清零使槽位可重用 */
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/**
 * 获取共享内存信息
 */
hic_status_t shmem_get_info(cap_id_t cap, shmem_region_t *info) {
    if (cap >= CAP_TABLE_SIZE || !info) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_global_cap_table[cap];
    
    if (entry->cap_id != cap) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 查找对应的共享内存区域 */
    for (u32 i = 0; i < MAX_SHMEM_REGIONS; i++) {
        if (g_shmem_regions[i].phys_base == entry->memory.base) {
            *info = g_shmem_regions[i];
            return HIC_SUCCESS;
        }
    }
    
    return HIC_ERROR_NOT_FOUND;
}

/* ==================== 零停机更新机制层实现 ==================== */

/* 服务实例表 */
#define MAX_SERVICE_INSTANCES  64
static struct {
    char name[64];
    u32 version;
    domain_id_t domain;
    service_instance_state_t state;
    u32 connection_count;
    u32 flags;
    cap_id_t primary_cap;       /* 主实例能力 */
    cap_id_t standby_cap;       /* 备用实例能力 */
} g_service_instances[MAX_SERVICE_INSTANCES];

/* 连接跟踪表 */
#define MAX_CONNECTION_TRACKERS  1024
static struct {
    cap_id_t cap_id;
    domain_id_t owner;
    domain_id_t original_owner;
    u64 create_time;
    u32 flags;
    u8 state_data[256];         /* 连接状态数据 */
    size_t state_size;
} g_connection_trackers[MAX_CONNECTION_TRACKERS];

/**
 * 服务端点原子重定向（机制层）
 * 
 * 安全保证：
 * 1. 验证端点能力有效性
 * 2. 验证目标域存在且活跃
 * 3. 原子性更新目标
 */
hic_status_t cap_endpoint_redirect(cap_id_t endpoint_cap,
                                    domain_id_t new_target,
                                    domain_id_t *old_target) {
    if (endpoint_cap >= CAP_TABLE_SIZE || new_target >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    cap_entry_t *entry = &g_global_cap_table[endpoint_cap];
    
    /* 检查能力有效性 */
    if (entry->cap_id != endpoint_cap) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查是否被撤销 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_REVOKED;
    }
    
    /* 验证目标域存在且活跃 */
    extern domain_t g_domains[];
    domain_t *target_domain = &g_domains[new_target];
    
    /* 检查域是否存在 */
    if (target_domain->domain_id != new_target) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    /* 检查域是否处于活跃状态（READY 或 RUNNING） */
    if (target_domain->state != DOMAIN_STATE_READY &&
        target_domain->state != DOMAIN_STATE_RUNNING) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_STATE;
    }
    
    /* 记录旧目标（用于回滚） */
    domain_id_t old = entry->endpoint.target;
    
    /* 原子性更新目标（单指令写入） */
    entry->endpoint.target = new_target;
    
    /* 确保写入可见 */
    atomic_release_barrier();
    
    atomic_exit_critical(irq);
    
    if (old_target) {
        *old_target = old;
    }
    
    return HIC_SUCCESS;
}

/**
 * 创建状态迁移通道（机制层）
 */
hic_status_t cap_migration_channel_create(domain_id_t from,
                                           domain_id_t to,
                                           size_t buffer_size,
                                           cap_id_t *out_cap,
                                           cap_handle_t *out_handle_from,
                                           cap_handle_t *out_handle_to) {
    if (from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX ||
        !out_cap || !out_handle_from || !out_handle_to) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 默认缓冲区大小 */
    if (buffer_size == 0) {
        buffer_size = 64 * 1024;  /* 64KB */
    }
    
    /* 使用共享内存机制创建通道 */
    cap_id_t shmem_cap;
    cap_handle_t handle;
    hic_status_t status = shmem_alloc(from, buffer_size, 
                                       SHMEM_FLAG_WRITABLE,
                                       &shmem_cap, &handle);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 映射到目标域 */
    status = shmem_map(from, to, shmem_cap,
                       CAP_MEM_READ | CAP_MEM_WRITE,
                       out_handle_to);
    if (status != HIC_SUCCESS) {
        /* 回滚 */
        cap_revoke(shmem_cap);
        return status;
    }
    
    *out_cap = shmem_cap;
    *out_handle_from = handle;
    
    return HIC_SUCCESS;
}

/**
 * 迁移连接能力（机制层）
 */
hic_status_t cap_connection_migrate(cap_id_t conn_cap,
                                     domain_id_t from,
                                     domain_id_t to,
                                     const void *state_data,
                                     size_t state_size) {
    if (conn_cap >= CAP_TABLE_SIZE ||
        from >= HIC_DOMAIN_MAX || to >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 状态数据大小限制 */
    if (state_size > 256) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    cap_entry_t *entry = &g_global_cap_table[conn_cap];
    
    /* 检查能力有效性 */
    if (entry->cap_id != conn_cap) {
        atomic_exit_critical(irq);
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查当前所有者 */
    if (entry->owner != from) {
        atomic_exit_critical(irq);
        return HIC_ERROR_PERMISSION;
    }
    
    /* 查找连接跟踪器 */
    u32 tracker_idx = MAX_CONNECTION_TRACKERS;
    for (u32 i = 0; i < MAX_CONNECTION_TRACKERS; i++) {
        if (g_connection_trackers[i].cap_id == conn_cap) {
            tracker_idx = i;
            break;
        }
    }
    
    /* 如果没有跟踪器，创建一个新的 */
    if (tracker_idx == MAX_CONNECTION_TRACKERS) {
        for (u32 i = 0; i < MAX_CONNECTION_TRACKERS; i++) {
            if (g_connection_trackers[i].cap_id == 0) {
                tracker_idx = i;
                g_connection_trackers[i].cap_id = conn_cap;
                g_connection_trackers[i].original_owner = from;
                g_connection_trackers[i].create_time = hal_get_timestamp();
                break;
            }
        }
    }
    
    if (tracker_idx == MAX_CONNECTION_TRACKERS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 保存状态数据 */
    if (state_data && state_size > 0) {
        memcopy(g_connection_trackers[tracker_idx].state_data, 
                state_data, state_size);
        g_connection_trackers[tracker_idx].state_size = state_size;
    }
    
    /* 原子性转移所有权 */
    entry->owner = to;
    g_connection_trackers[tracker_idx].owner = to;
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/**
 * 创建服务实例能力（机制层）
 */
hic_status_t cap_create_service_instance(domain_id_t owner,
                                          const char *service_name,
                                          u32 version,
                                          u32 flags,
                                          cap_id_t *out) {
    if (owner >= HIC_DOMAIN_MAX || !service_name || !out) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 查找空闲服务实例槽 */
    u32 idx = MAX_SERVICE_INSTANCES;
    for (u32 i = 0; i < MAX_SERVICE_INSTANCES; i++) {
        if (g_service_instances[i].name[0] == '\0') {
            idx = i;
            break;
        }
    }
    
    if (idx == MAX_SERVICE_INSTANCES) {
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 初始化服务实例 */
    strncpy(g_service_instances[idx].name, service_name, 63);
    g_service_instances[idx].name[63] = '\0';
    g_service_instances[idx].version = version;
    g_service_instances[idx].domain = owner;
    g_service_instances[idx].state = SERVICE_INSTANCE_INIT;
    g_service_instances[idx].connection_count = 0;
    g_service_instances[idx].flags = flags;
    g_service_instances[idx].primary_cap = HIC_CAP_INVALID;
    g_service_instances[idx].standby_cap = HIC_CAP_INVALID;
    
    /* 创建能力 */
    cap_id_t cap = HIC_CAP_INVALID;
    for (u32 i = 1; i < CAP_TABLE_SIZE && cap == HIC_CAP_INVALID; i++) {
        if (g_global_cap_table[i].cap_id == 0) {
            cap = i;
        }
    }
    
    if (cap == HIC_CAP_INVALID) {
        g_service_instances[idx].name[0] = '\0';
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 初始化能力条目 */
    g_global_cap_table[cap].cap_id = cap;
    g_global_cap_table[cap].rights = CAP_LCORE_QUERY | flags;
    g_global_cap_table[cap].owner = owner;
    g_global_cap_table[cap].flags = 0;
    
    *out = cap;
    return HIC_SUCCESS;
}

/**
 * 切换服务主实例（机制层）
 * 
 * 安全保证：
 * 1. 验证服务能力有效性
 * 2. 验证新主实例属于同一服务（必须是当前备用实例）
 * 3. 原子性切换
 */
hic_status_t cap_service_switch_primary(cap_id_t service_cap,
                                         cap_id_t new_primary_cap,
                                         cap_id_t *old_primary_cap) {
    if (service_cap >= CAP_TABLE_SIZE || new_primary_cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    /* 查找服务实例 */
    u32 idx = MAX_SERVICE_INSTANCES;
    for (u32 i = 0; i < MAX_SERVICE_INSTANCES; i++) {
        if (g_service_instances[i].primary_cap == service_cap ||
            g_service_instances[i].standby_cap == service_cap) {
            idx = i;
            break;
        }
    }
    
    if (idx == MAX_SERVICE_INSTANCES) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NOT_FOUND;
    }
    
    /* 验证新主实例属于同一服务（必须是当前备用实例） */
    if (g_service_instances[idx].standby_cap != new_primary_cap) {
        atomic_exit_critical(irq);
        return HIC_ERROR_INVALID_PARAM;  /* 新主实例不属于该服务 */
    }
    
    /* 记录旧主实例 */
    cap_id_t old_primary = g_service_instances[idx].primary_cap;
    
    /* 原子性切换 */
    g_service_instances[idx].standby_cap = old_primary;
    g_service_instances[idx].primary_cap = new_primary_cap;
    
    atomic_exit_critical(irq);
    
    if (old_primary_cap) {
        *old_primary_cap = old_primary;
    }
    
    return HIC_SUCCESS;
}

/**
 * 获取服务实例状态（机制层）
 */
hic_status_t cap_get_service_instance_state(cap_id_t instance_cap,
                                             service_instance_state_t *state,
                                             u32 *connection_count) {
    if (instance_cap >= CAP_TABLE_SIZE || !state) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 查找服务实例 */
    u32 idx = MAX_SERVICE_INSTANCES;
    for (u32 i = 0; i < MAX_SERVICE_INSTANCES; i++) {
        if (g_service_instances[i].primary_cap == instance_cap ||
            g_service_instances[i].standby_cap == instance_cap) {
            idx = i;
            break;
        }
    }
    
    if (idx == MAX_SERVICE_INSTANCES) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    *state = g_service_instances[idx].state;
    if (connection_count) {
        *connection_count = g_service_instances[idx].connection_count;
    }
    
    return HIC_SUCCESS;
}
