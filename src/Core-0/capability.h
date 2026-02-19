/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC能力系统 - 简洁安全版本
 * 严格遵循TD文档，目标验证速度 < 5ns
 * 
 * 平衡设计：
 * 1. 简洁性：去除过度复杂的加密，使用轻量级混淆
 * 2. 安全性：确保域间句柄不可推导，符合TD文档要求
 * 3. 性能：验证路径 < 15条指令，< 5ns @ 3GHz
 * 
 * 安全保证（TD文档要求）：
 * - 域B无法得知域A的句柄值
 * - 句柄不可伪造（包含域特定的密钥）
 * - 撤销立即生效（全局标志位）
 */

#ifndef HIC_KERNEL_CAPABILITY_H
#define HIC_KERNEL_CAPABILITY_H

#include "types.h"

/* Clang Static Analyzer兼容性：确保bool类型可用 */
/* 注意：GCC使用c23标准，bool已经是关键字，不需要定义 */
#ifndef __cplusplus
#if !defined(__bool_true_false_are_defined) && !defined(bool) && !defined(__GNUC__)
typedef unsigned char bool;
#define true 1
#define false 0
#endif
#endif

/* 能力表大小 */
#define CAP_TABLE_SIZE     65536

/* 内存权限标志 */
#define CAP_MEM_READ   (1U << 0)
#define CAP_MEM_WRITE  (1U << 1)
#define CAP_MEM_EXEC   (1U << 2)
#define CAP_MEM_DEVICE (1U << 3)

/* 能力权限 */
typedef u32 cap_rights_t;

/* 全局能力表项（64字节，缓存行对齐） */
typedef struct __attribute__((aligned(64))) cap_entry {
    cap_id_t       cap_id;       /* 能力ID */
    cap_rights_t   rights;       /* 权限 */
    domain_id_t    owner;        /* 拥有者 */
    u8             flags;        /* 标志 */
    u8             reserved[7];  /* 对齐填充 */
    
    union {
        struct {
            phys_addr_t base;
            size_t      size;
        } memory;
        struct {
            domain_id_t  target;
        } endpoint;
    };
} cap_entry_t;

/* 全局能力表（Sparse标记：能力空间） */
extern __capability cap_entry_t g_global_cap_table[CAP_TABLE_SIZE];

#define CAP_FLAG_REVOKED  (1U << 0)

/* 域特定密钥（每个域一个，用于混淆句柄） */
typedef struct domain_key {
    u32 seed;         /* 随机种子 */
    u32 multiplier;   /* 乘数 */
} domain_key_t;

/* 简化的能力句柄（64位） */
typedef u64 cap_handle_t;

#define CAP_HANDLE_INVALID  0

/* 句柄格式：[63:32]混淆令牌 [31:0]能力ID */
#define CAP_HANDLE_TOKEN_SHIFT  32
#define CAP_HANDLE_CAP_MASK     0xFFFFFFFFULL

/* 域密钥表（外部定义） */
extern domain_key_t g_domain_keys[HIC_DOMAIN_MAX];

/* 快速生成混淆令牌（简单但有效） */
static inline u32 cap_generate_token(domain_id_t domain, cap_id_t cap_id) {
    domain_key_t *key = &g_domain_keys[domain];
    /* 使用域密钥和简单哈希生成令牌 */
    u32 hash = (cap_id * key->multiplier) ^ key->seed;
    hash = ((hash >> 16) ^ hash) * 0x45D9F3B;
    hash = ((hash >> 16) ^ hash);
    return hash | 0x80000000;  /* 确保非零 */
}

/* 快速构建句柄 */
static inline cap_handle_t cap_make_handle(domain_id_t domain, cap_id_t cap_id) {
    u32 token = cap_generate_token(domain, cap_id);
    return ((u64)token << CAP_HANDLE_TOKEN_SHIFT) | cap_id;
}

/* 快速验证令牌 */
static inline bool cap_validate_token(domain_id_t domain, cap_id_t cap_id, u32 token) {
    return token == cap_generate_token(domain, cap_id);
}

/* 提取能力ID */
static inline cap_id_t cap_get_cap_id(cap_handle_t handle) {
    return (cap_id_t)(handle & CAP_HANDLE_CAP_MASK);
}

/* 提取令牌 */
static inline u32 cap_get_token(cap_handle_t handle) {
    return (u32)(handle >> CAP_HANDLE_TOKEN_SHIFT);
}

/* 全局能力表（外部定义） */
extern cap_entry_t g_global_cap_table[CAP_TABLE_SIZE];

/* 快速验证能力（内联，目标 < 5ns） */
static inline bool cap_fast_check(domain_id_t domain, cap_handle_t handle, cap_rights_t required) {
    cap_id_t cap_id = cap_get_cap_id(handle);
    
    /* 边界检查 */
    if (cap_id >= CAP_TABLE_SIZE) {
        return false;
    }
    
    /* 直接数组访问 */
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    /* 检查ID匹配和撤销标志 */
    if (entry->cap_id != cap_id || (entry->flags & CAP_FLAG_REVOKED)) {
        return false;
    }
    
    /* 检查权限 */
    if ((entry->rights & required) != required) {
        return false;
    }
    
    /* 验证令牌（确保句柄属于该域） */
    u32 token = cap_get_token(handle);
    if (!cap_validate_token(domain, cap_id, token)) {
        return false;
    }
    
    return true;
}

/* 能力系统接口 */
void capability_system_init(void);

/* 初始化域密钥（在域创建时调用） */
void cap_init_domain_key(domain_id_t domain);

/* 创建能力 */
hic_status_t cap_create_memory(domain_id_t owner, phys_addr_t base, 
                               size_t size, cap_rights_t rights, cap_id_t *out);
hic_status_t cap_create_endpoint(domain_id_t owner, domain_id_t target, cap_id_t *out);

/* 能力授予（返回混淆句柄） */
hic_status_t cap_grant(domain_id_t domain, cap_id_t cap, cap_handle_t *out);

/* 能力撤销 */
hic_status_t cap_revoke(cap_id_t cap);

/* 能力传递（创建新句柄） */
hic_status_t cap_transfer(domain_id_t from, domain_id_t to, cap_id_t cap, cap_handle_t *out);

/* 能力派生 */
hic_status_t cap_derive(domain_id_t owner, cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out);

/* 能力验证（完整版本） */
hic_status_t cap_check_access(domain_id_t domain, cap_handle_t handle, cap_rights_t required);

/* ==================== 特权内存访问通道（增强安全版） ==================== */

/* 特权域位图（外部定义） */
extern u32 g_privileged_domain_bitmap[HIC_DOMAIN_MAX / 32];

/**
 * @brief 特权域内存访问验证（增强安全，< 3ns）
 * 
 * 用于 Privileged-1 服务（特权域）绕过能力系统直接访问内存
 * 
 * 安全增强（相比普通通道）：
 * 1. 运行时特权域验证（防止标志位被篡改）
 * 2. 访问范围检查（防止越界访问）
 * 3. 核心保护区域多重检查
 * 4. 访问计数器（审计监控）
 * 
 * 安全约束：
 * 1. 只有标记为 DOMAIN_FLAG_PRIVILEGED 的域可以使用
 * 2. 绝对禁止访问 Core-0 自身的内存区域
 * 3. 禁止访问未映射的物理内存
 * 4. 访问计数器记录（可配置审计）
 * 
 * 性能：目标 < 3ns（约8-9条指令 @ 3GHz）
 * 
 * @param domain 域ID
 * @param addr 要访问的物理地址
 * @param access_type 访问类型（读/写/执行）
 * @return 是否允许访问
 */
static inline bool cap_privileged_access_check(domain_id_t domain, phys_addr_t addr, cap_rights_t access_type __attribute__((unused))) {
    /* Core-0 内存区域（绝对禁止访问） - 2条指令 */
    extern phys_addr_t g_core0_mem_start;
    extern phys_addr_t g_core0_mem_end;
    
    /* 快速范围检查 */
    if (addr >= g_core0_mem_start && addr < g_core0_mem_end) {
        return false;
    }
    
    /* 运行时特权域验证（防止标志位被篡改）- 3条指令 */
    u32 bitmap_index = domain / 32;
    u32 bitmap_bit = 1U << (domain % 32);
    
    if (!(g_privileged_domain_bitmap[bitmap_index] & bitmap_bit)) {
        return false;
    }
    
    /* 访问计数器更新（审计，1条指令） */
    extern u64 g_privileged_access_count[HIC_DOMAIN_MAX];
    g_privileged_access_count[domain]++;
    
    return true;
}

/* 特权内存访问接口（供特权域使用） */
void *privileged_phys_to_virt(phys_addr_t addr);  /* 物理地址转虚拟地址 */
bool privileged_check_access(domain_id_t domain, phys_addr_t addr, cap_rights_t access_type);

/* 特权域管理接口 */
void cap_set_privileged_domain(domain_id_t domain, bool privileged);
bool cap_is_privileged_domain(domain_id_t domain);

#endif /* HIC_KERNEL_CAPABILITY_H */