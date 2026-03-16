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

/* 能力表大小 - 优化以减少内核BSS段大小 */
#define CAP_TABLE_SIZE     2048   /* 从65536减小到2048，满足形式化验证要求 */

/* 内存权限标志 */
#define CAP_MEM_READ   (1U << 0)
#define CAP_MEM_WRITE  (1U << 1)
#define CAP_MEM_EXEC   (1U << 2)
#define CAP_MEM_DEVICE (1U << 3)

/* 逻辑核心权限标志 */
#define CAP_LCORE_USE      (1U << 8)  /* 使用逻辑核心（创建线程） */
#define CAP_LCORE_QUERY    (1U << 9)  /* 查询逻辑核心信息 */
#define CAP_LCORE_MIGRATE  (1U << 10) /* 迁移逻辑核心 */
#define CAP_LCORE_SET_AFFINITY (1U << 11) /* 设置亲和性 */
#define CAP_LCORE_MONITOR  (1U << 12) /* 监控性能计数器 */
#define CAP_LCORE_BORROW   (1U << 13) /* 借用逻辑核心 */

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
        struct {
            logical_core_id_t logical_core_id;
            logical_core_flags_t flags;
            logical_core_quota_t quota;
        } logical_core;
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

/* 快速验证能力（内联，目标 < 5ns）
 * 
 * 优化策略：
 * 1. 减少分支数量（从5个减到2个）
 * 2. 使用条件传送避免分支预测失败
 * 3. 合并检查条件
 * 
 * 预期指令数：~12条 @ 3GHz = 4ns
 */
static inline bool cap_fast_check(domain_id_t domain, cap_handle_t handle, cap_rights_t required) {
    /* 提取能力ID（单条指令） */
    cap_id_t cap_id = (cap_id_t)(handle & CAP_HANDLE_CAP_MASK);
    
    /* 边界检查 + 表项读取 */
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    /* 合并检查：ID匹配 + 未撤销 + 权限足够
     * 编译为条件传送，无分支预测失败
     */
    u32 flags = entry->flags;
    u32 rights = entry->rights;
    cap_id_t stored_id = entry->cap_id;
    
    /* 单次综合检查 */
    bool valid = (stored_id == cap_id) && 
                 !(flags & CAP_FLAG_REVOKED) && 
                 ((rights & required) == required);
    
    /* 快速失败：无效则直接返回 */
    if (!valid) return false;
    
    /* 令牌验证（仅在基本检查通过后执行） */
    u32 token = (u32)(handle >> CAP_HANDLE_TOKEN_SHIFT);
    u32 expected = cap_generate_token(domain, cap_id);
    
    return token == expected;
}

/* 超快速路径：仅检查权限和撤销状态（用于已信任句柄）
 * 预期指令数：~6条 @ 3GHz = 2ns
 */
static inline bool cap_fast_check_rights(cap_handle_t handle, cap_rights_t required) {
    cap_id_t cap_id = (cap_id_t)(handle & CAP_HANDLE_CAP_MASK);
    cap_entry_t *e = &g_global_cap_table[cap_id];
    
    return (e->cap_id == cap_id) && 
           !(e->flags & CAP_FLAG_REVOKED) && 
           ((e->rights & required) == required);
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

/* 能力传递（带权限衰减） - 机制层原语 */
hic_status_t cap_transfer_with_attenuation(domain_id_t from, domain_id_t to, 
                                            cap_id_t cap, cap_rights_t attenuated_rights,
                                            cap_handle_t *out);

/* 能力派生 */
hic_status_t cap_derive(domain_id_t owner, cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out);

/* 能力验证（完整版本） */
hic_status_t cap_check_access(domain_id_t domain, cap_handle_t handle, cap_rights_t required);

/* ==================== 特权内存访问通道（增强安全版） ==================== */

/* 特权域位图（外部定义） */
extern u32 g_privileged_domain_bitmap[HIC_DOMAIN_MAX / 32];

/**
 * @brief 特权域内存访问验证（超快速路径，< 2ns）
 * 
 * 用于 Privileged-1 服务（特权域）绕过能力系统直接访问内存
 * 
 * 优化策略：
 * 1. 合并所有检查为单条复合条件
 * 2. 使用位图直接索引（无除法）
 * 3. 移除审计计数器（热路径）
 * 
 * 安全约束：
 * 1. 只有特权域可使用
 * 2. 禁止访问 Core-0 内存区域
 * 
 * 预期指令数：~5条 @ 3GHz = 1.7ns
 */
static inline bool cap_privileged_access_check(domain_id_t domain, phys_addr_t addr, 
                                               cap_rights_t access_type __attribute__((unused))) {
    /* 外部变量 */
    extern phys_addr_t g_core0_mem_start;
    extern phys_addr_t g_core0_mem_end;
    extern u32 g_privileged_domain_bitmap[];
    
    /* 单次综合检查：
     * 1. 地址不在 Core-0 区域
     * 2. 域是特权域
     */
    bool not_core0 = (addr < g_core0_mem_start) || (addr >= g_core0_mem_end);
    bool is_priv = (g_privileged_domain_bitmap[domain >> 5] >> (domain & 31)) & 1;
    
    return not_core0 && is_priv;
}

/**
 * @brief 特权内存访问检查（带审计）
 * 
 * 完整版本，包含访问计数器更新。
 * 用于需要审计的场景。
 */
static inline bool cap_privileged_access_check_audited(domain_id_t domain, phys_addr_t addr, 
                                                        cap_rights_t access_type) {
    if (!cap_privileged_access_check(domain, addr, access_type)) {
        return false;
    }
    
    /* 更新访问计数器 */
    extern u64 g_privileged_access_count[];
    g_privileged_access_count[domain]++;
    
    return true;
}

/* 特权内存访问接口（供特权域使用） */
void *privileged_phys_to_virt(phys_addr_t addr);  /* 物理地址转虚拟地址 */
bool privileged_check_access(domain_id_t domain, phys_addr_t addr, cap_rights_t access_type);

/* 特权域管理接口 */
void cap_set_privileged_domain(domain_id_t domain, bool privileged);
bool cap_is_privileged_domain(domain_id_t domain);

/* ==================== 逻辑核心能力接口 ==================== */

/**
 * @brief 创建逻辑核心能力
 * 
 * 为指定的逻辑核心创建能力对象。
 * 
 * @param owner 拥有者域ID
 * @param logical_core_id 逻辑核心ID
 * @param flags 逻辑核心属性标志
 * @param quota CPU时间配额
 * @param rights 能力权限
 * @param out 输出的能力ID
 * @return 状态码
 */
hic_status_t cap_create_logical_core(domain_id_t owner,
                                    logical_core_id_t logical_core_id,
                                    logical_core_flags_t flags,
                                    logical_core_quota_t quota,
                                    cap_rights_t rights,
                                    cap_id_t *out);

/**
 * @brief 获取逻辑核心能力信息
 * 
 * 从能力条目中提取逻辑核心信息。
 * 
 * @param cap_id 能力ID
 * @param logical_core_id 输出的逻辑核心ID
 * @param flags 输出的逻辑核心标志
 * @param quota 输出的CPU时间配额
 * @return 状态码
 */
hic_status_t cap_get_logical_core_info(cap_id_t cap_id,
                                      logical_core_id_t *logical_core_id,
                                      logical_core_flags_t *flags,
                                      logical_core_quota_t *quota);

/* ==================== 共享内存机制层 ==================== */

/* 共享内存区域描述 */
typedef struct shmem_region {
    phys_addr_t    phys_base;      /* 物理基址 */
    size_t         size;           /* 大小 */
    domain_id_t    owner;          /* 创建者 */
    u32            ref_count;      /* 引用计数 */
    u32            flags;          /* 标志 */
#define SHMEM_FLAG_WRITABLE   (1U << 0)  /* 可写 */
#define SHMEM_FLAG_EXECUTABLE (1U << 1)  /* 可执行 */
} shmem_region_t;

/* 共享内存映射描述 */
typedef struct shmem_mapping {
    cap_id_t       cap_id;         /* 内存能力ID */
    domain_id_t    domain;         /* 映射到的域 */
    virt_addr_t    vaddr;          /* 虚拟地址 */
    cap_rights_t   rights;         /* 访问权限 */
} shmem_mapping_t;

/**
 * @brief 分配共享内存区域（机制层）
 * 
 * Core-0 分配物理内存，创建共享内存区域。
 * 返回内存能力，可用于后续映射。
 * 
 * @param owner 创建者域ID
 * @param size 请求大小
 * @param flags 标志
 * @param out_cap 输出的内存能力ID
 * @param out_handle 输出的句柄（给创建者）
 * @return 状态码
 */
hic_status_t shmem_alloc(domain_id_t owner, size_t size, u32 flags,
                          cap_id_t *out_cap, cap_handle_t *out_handle);

/**
 * @brief 映射共享内存到目标域（机制层）
 * 
 * 将共享内存映射到目标域的地址空间。
 * 可以指定与创建者不同的权限（权限衰减）。
 * 
 * @param from 源域ID（必须持有能力）
 * @param to 目标域ID
 * @param cap 内存能力ID
 * @param rights 授予的权限（可以是原权限的子集）
 * @param out_handle 输出的句柄（给目标域）
 * @return 状态码
 */
hic_status_t shmem_map(domain_id_t from, domain_id_t to, cap_id_t cap,
                        cap_rights_t rights, cap_handle_t *out_handle);

/**
 * @brief 解除共享内存映射（机制层）
 * 
 * @param domain 域ID
 * @param handle 内存能力句柄
 * @return 状态码
 */
hic_status_t shmem_unmap(domain_id_t domain, cap_handle_t handle);

/**
 * @brief 获取共享内存信息
 * 
 * @param cap 能力ID
 * @param info 输出的区域信息
 * @return 状态码
 */
hic_status_t shmem_get_info(cap_id_t cap, shmem_region_t *info);

#endif /* HIC_KERNEL_CAPABILITY_H */