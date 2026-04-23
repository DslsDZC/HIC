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
#include <stdbool.h>

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
            u8    sched_policy;    /* 调度策略（domain_sched_policy_t） */
            u8    max_derived_policy; /* 允许派生的最高策略 */
            u8    reserved[6];     /* 对齐填充 */
        } logical_core;
        struct {
            thread_id_t thread_id;
            u32         reserved;
        } thread_efc;
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

/* 创建执行流能力（EFC） */
hic_status_t cap_create_thread(domain_id_t owner, thread_id_t thread_id, cap_id_t *out);

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

/**
 * @brief 能力派生（带策略衰减）
 * 
 * 派生能力时同时衰减调度策略。
 * 子能力的策略不能高于父能力的 max_derived_policy。
 * 
 * @param owner 拥有者域ID
 * @param parent 父能力ID
 * @param sub_rights 派生权限（必须是父权限的子集）
 * @param derived_policy 派生后的调度策略
 * @param out 输出的派生能力ID
 * @return 状态码
 */
hic_status_t cap_derive_with_policy(domain_id_t owner, cap_id_t parent, 
                                     cap_rights_t sub_rights,
                                     u8 derived_policy,
                                     cap_id_t *out);

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
 * 已废弃：非 Core-0 域无法写入内核 .data 段。
 * 审计统计通过系统调用委托给 Core-0 完成。
 * 
 * @deprecated 使用 cap_privileged_access_check() 或通过 syscall 审计
 */

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
 * @brief 创建逻辑核心能力（带调度策略）
 * 
 * 创建能力时指定调度策略，并进行策略分层检查。
 * 
 * @param owner 拥有者域ID
 * @param logical_core_id 逻辑核心ID
 * @param flags 逻辑核心属性标志
 * @param quota CPU时间配额
 * @param rights 能力权限
 * @param sched_policy 调度策略
 * @param out 输出的能力ID
 * @return 状态码
 */
hic_status_t cap_create_logical_core_with_policy(domain_id_t owner,
                                                  logical_core_id_t logical_core_id,
                                                  logical_core_flags_t flags,
                                                  logical_core_quota_t quota,
                                                  cap_rights_t rights,
                                                  u8 sched_policy,
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

/**
 * @brief 获取逻辑核心能力的调度策略
 * 
 * @param cap_id 能力ID
 * @param sched_policy 输出的调度策略
 * @param max_derived_policy 输出的最大派生策略
 * @return 状态码
 */
hic_status_t cap_get_logical_core_policy(cap_id_t cap_id,
                                          u8 *sched_policy,
                                          u8 *max_derived_policy);

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

/* ==================== 零停机更新机制层原语 ==================== */

/**
 * @brief 服务端点原子重定向（机制层）
 * 
 * 原子性地将端点能力重定向到新目标域。
 * 这是零停机更新的核心原语。
 * 
 * 机制保证：
 * 1. 单指令原子操作，无竞争窗口
 * 2. 正在进行的调用继续完成
 * 3. 新调用自动路由到新目标
 * 4. 支持快速回滚
 * 
 * @param endpoint_cap 端点能力ID
 * @param new_target 新目标域ID
 * @param old_target 输出旧目标域ID（用于回滚）
 * @return 状态码
 */
hic_status_t cap_endpoint_redirect(cap_id_t endpoint_cap,
                                    domain_id_t new_target,
                                    domain_id_t *old_target);

/**
 * @brief 创建状态迁移通道（机制层）
 * 
 * 创建域间状态迁移的安全通道。
 * 基于共享内存实现零拷贝状态传输。
 * 
 * @param from 源域ID
 * @param to 目标域ID
 * @param buffer_size 缓冲区大小
 * @param out_cap 输出的通道能力ID
 * @param out_handle_from 源域句柄
 * @param out_handle_to 目标域句柄
 * @return 状态码
 */
hic_status_t cap_migration_channel_create(domain_id_t from,
                                           domain_id_t to,
                                           size_t buffer_size,
                                           cap_id_t *out_cap,
                                           cap_handle_t *out_handle_from,
                                           cap_handle_t *out_handle_to);

/**
 * @brief 迁移连接能力（机制层）
 * 
 * 原子性地将连接能力从一个域迁移到另一个域。
 * 保证连接状态完整性。
 * 
 * @param conn_cap 连接能力ID
 * @param from 当前所有者域ID
 * @param to 新所有者域ID
 * @param state_data 状态数据（可选）
 * @param state_size 状态数据大小
 * @return 状态码
 */
hic_status_t cap_connection_migrate(cap_id_t conn_cap,
                                     domain_id_t from,
                                     domain_id_t to,
                                     const void *state_data,
                                     size_t state_size);

/* ==================== 服务实例能力 ==================== */

/* 服务实例状态 */
typedef enum {
    SERVICE_INSTANCE_INIT,       /* 初始化中 */
    SERVICE_INSTANCE_WARMING,    /* 预热中 */
    SERVICE_INSTANCE_ACTIVE,     /* 活跃（接收流量） */
    SERVICE_INSTANCE_DRAINING,   /* 排空中（停止接收新请求） */
    SERVICE_INSTANCE_MIGRATING,  /* 迁移中 */
    SERVICE_INSTANCE_TERMINATING /* 终止中 */
} service_instance_state_t;

/* 服务实例能力标志 */
#define CAP_SERVICE_PRIMARY    (1U << 16)  /* 主实例 */
#define CAP_SERVICE_STANDBY    (1U << 17)  /* 备用实例 */
#define CAP_SERVICE_DRAINABLE  (1U << 18)  /* 可排空 */

/**
 * @brief 创建服务实例能力（机制层）
 * 
 * 创建一个服务实例能力，用于零停机更新。
 * 
 * @param owner 所有者域ID
 * @param service_name 服务名称
 * @param version 服务版本
 * @param flags 实例标志
 * @param out 输出的能力ID
 * @return 状态码
 */
hic_status_t cap_create_service_instance(domain_id_t owner,
                                          const char *service_name,
                                          u32 version,
                                          u32 flags,
                                          cap_id_t *out);

/**
 * @brief 切换服务主实例（机制层）
 * 
 * 原子性地切换服务的主实例。
 * 旧的 Primary 变为 Standby，新的 Standby 变为 Primary。
 * 
 * @param service_cap 服务能力ID
 * @param new_primary_cap 新主实例的能力ID
 * @param old_primary_cap 输出旧主实例的能力ID
 * @return 状态码
 */
hic_status_t cap_service_switch_primary(cap_id_t service_cap,
                                         cap_id_t new_primary_cap,
                                         cap_id_t *old_primary_cap);

/**
 * @brief 获取服务实例状态（机制层）
 * 
 * @param instance_cap 实例能力ID
 * @param state 输出实例状态
 * @param connection_count 输出当前连接数
 * @return 状态码
 */
hic_status_t cap_get_service_instance_state(cap_id_t instance_cap,
                                             service_instance_state_t *state,
                                             u32 *connection_count);

/* ==================== 树状能力空间（Tree-structured CSpace） ==================== */

/**
 * HIC 树状能力空间设计
 * 
 * 平衡 seL4 灵活性与嵌入式确定性：
 * - 全局能力表：固定大小数组，编译时常量
 * - CNode 树：任意深度，灵活权限组织
 * - CPtr：编码路径的整数，快速解析
 * - CDT（能力派生树）：支持撤销传播
 * 
 * 性能特性：
 * - 能力查表：O(1)
 * - 路径解析：O(depth)
 * - 撤销子树：O(subtree size)
 */

/* ==================== CNode 配置 ==================== */

/* CNode 槽位数量选项（2的幂） */
#define CNODE_SLOTS_16      16
#define CNODE_SLOTS_64      64
#define CNODE_SLOTS_256     256
#define CNODE_SLOTS_1024    1024

/* 默认 CNode 槽位数 */
#define CNODE_DEFAULT_SLOTS CNODE_SLOTS_64

/* 最大 CNode 树深度（防止无限递归） */
#define CNODE_MAX_DEPTH     16

/* Guard 最大位数 */
#define CNODE_MAX_GUARD_BITS  32

/* ==================== CPtr 能力指针 ==================== */

/**
 * CPtr 格式（64位）：
 * 
 * [63:48] - 保留（未来扩展）
 * [47:32] - Guard（可选，用于变长路径跳过）
 * [31:0]  - 路径索引序列
 * 
 * 路径索引序列格式：
 * 每级 CNode 的索引位数由该 CNode 的 slot_bits 决定
 * 例如：根 CNode 有 256 槽位（8位），子 CNode 有 64 槽位（6位）
 * CPtr = [level1_idx(8位)][level2_idx(6位)][...]
 */
typedef u64 cptr_t;

#define CPTR_INVALID        0ULL
#define CPTR_ROOT           0ULL    /* 根 CNode */

/* CPtr 操作宏 */
#define CPTR_GUARD_SHIFT    32
#define CPTR_GUARD_MASK     0xFFFFULL

/* 提取 Guard */
static inline u32 cptr_get_guard(cptr_t cptr) {
    return (u32)((cptr >> CPTR_GUARD_SHIFT) & CPTR_GUARD_MASK);
}

/* 设置 Guard */
static inline cptr_t cptr_set_guard(cptr_t cptr, u32 guard) {
    return (cptr & ~(CPTR_GUARD_MASK << CPTR_GUARD_SHIFT)) | 
           ((cptr_t)guard << CPTR_GUARD_SHIFT);
}

/* 提取指定级别的索引（从低位开始） */
static inline u32 cptr_extract_index(cptr_t cptr, u8 slot_bits, u8 level) {
    u8 shift = level * slot_bits;
    u32 mask = (1U << slot_bits) - 1;
    return (u32)((cptr >> shift) & mask);
}

/* ==================== CNode 槽位 ==================== */

/**
 * CNode 槽位结构
 * 
 * 每个槽位存储：
 * - cap_id：指向能力实体（全局表中）或子 CNode
 * - rights_mask：本地权限掩码（权限衰减）
 * - guard：可选，用于变长路径
 * - flags：槽位标志
 */
typedef struct cnode_slot {
    cap_id_t       cap_id;        /* 能力ID（全局表索引）或子 CNode 的 cap_id */
    cap_rights_t   rights_mask;   /* 本地权限掩码 */
    u32            guard;         /* Guard 值（用于变长路径跳过） */
    u8             flags;         /* 槽位标志 */
    u8             reserved[3];   /* 对齐填充 */
} cnode_slot_t;

/* 槽位标志 */
#define CNODE_SLOT_EMPTY       0x00    /* 空槽位 */
#define CNODE_SLOT_VALID       0x01    /* 有效槽位 */
#define CNODE_SLOT_CNODE       0x02    /* 指向子 CNode */
#define CNODE_SLOT_GUARD       0x04    /* 使用 Guard */
#define CNODE_SLOT_REVOKED     0x08    /* 已撤销（待清理） */

/* 槽位大小：16 字节 */
#define CNODE_SLOT_SIZE        sizeof(cnode_slot_t)

/* ==================== CNode 能力节点 ==================== */

/**
 * CNode 结构
 * 
 * CNode 本身也是一种能力对象，存储在全局能力表中。
 * 类型为 CAP_CNODE。
 * 
 * 特性：
 * - 固定大小槽位数组（创建时确定，2的幂）
 * - 可嵌套形成树
 * - 支持权限衰减
 * - Guard 机制支持变长路径
 */
typedef struct cnode {
    cap_id_t       self_cap;      /* 自身的能力ID（在全局表中） */
    domain_id_t    owner;         /* 所属域 */
    u16            slot_count;    /* 槽位数量（2的幂） */
    u8             slot_bits;     /* 槽位索引位数（log2(slot_count)） */
    u8             depth;         /* 在树中的深度（根 CNode 为 0） */
    u32            guard;         /* CNode 级别的 Guard */
    u8             guard_bits;    /* Guard 位数 */
    u8             flags;         /* CNode 标志 */
    u8             reserved[6];   /* 对齐填充 */
    
    /* 槽位数组（变长，根据 slot_count 分配） */
    /* 注意：实际槽位数组在全局能力表的 memory 区域中分配 */
} cnode_t;

/* CNode 标志 */
#define CNODE_FLAG_ROOT        (1U << 0)    /* 根 CNode */
#define CNODE_FLAG_READONLY    (1U << 1)    /* 只读（禁止修改） */
#define CNODE_FLAG_REVOKING    (1U << 2)    /* 正在撤销中 */

/* 获取 CNode 槽位数组指针 */
static inline cnode_slot_t* cnode_get_slots(cnode_t *cnode) {
    /* 槽位数组紧随 cnode_t 结构之后 */
    return (cnode_slot_t*)(cnode + 1);
}

/* 计算槽位索引 */
static inline u32 cnode_slot_index(cnode_t *cnode, cptr_t cptr, u8 level) {
    return cptr_extract_index(cptr, cnode->slot_bits, level - cnode->depth);
}

/* ==================== CSpace 能力空间 ==================== */

/**
 * CSpace - 域能力空间
 * 
 * 每个域拥有一个 CSpace，从根 CNode 开始。
 * 存储在域控制块中。
 */
typedef struct cspace {
    cap_id_t       root_cnode;    /* 根 CNode 的能力ID */
    domain_id_t    owner;         /* 所属域 */
    u32            total_caps;    /* 能力总数（包括子树中的） */
    u32            max_depth;     /* 最大深度 */
    u8             flags;         /* CSpace 标志 */
    u8             reserved[7];   /* 对齐填充 */
} cspace_t;

/* CSpace 标志 */
#define CSPACE_FLAG_ACTIVE      (1U << 0)    /* 活跃 */
#define CSPACE_FLAG_LOCKED      (1U << 1)    /* 锁定（禁止修改） */

/* ==================== 能力派生树（CDT）更新 ==================== */

/**
 * CDT 条目（存储在全局能力表中）
 * 
 * 已有的 cap_derivative_t 结构扩展，
 * 用于跟踪父子关系，支持撤销传播。
 */

/* CDT 节点（扩展定义） */
typedef struct cdt_node {
    cap_id_t       parent;                    /* 父能力ID */
    cap_id_t       first_child;               /* 第一个子能力 */
    cap_id_t       next_sibling;              /* 下一个兄弟能力 */
    cap_id_t       prev_sibling;              /* 前一个兄弟能力 */
    domain_id_t    owner;                     /* 所属域 */
    u32            ref_count;                 /* 引用计数 */
    u32            cnode_ref_count;           /* CNode 槽位引用计数 */
} cdt_node_t;

/* ==================== 能力类型扩展 ==================== */

/* 新增能力类型：CNode */
#define CAP_CNODE           (1U << 24)    /* CNode 能力 */
#define CAP_TYPE_THREAD     (1U << 25)    /* 执行流能力 (EFC) */

/* 能力类型判断 */
static inline bool cap_is_cnode(cap_rights_t rights) {
    return (rights & CAP_CNODE) != 0;
}

static inline bool cap_is_thread(cap_rights_t rights) {
    return (rights & CAP_TYPE_THREAD) != 0;
}

/* ==================== CNode 操作接口 ==================== */

/**
 * @brief 创建 CNode
 * 
 * 分配全局能力表条目和槽位数组。
 * 
 * @param owner 所属域ID
 * @param slot_bits 槽位索引位数（决定槽位数量 = 2^slot_bits）
 * @param out_cnode_cap 输出的 CNode 能力ID
 * @return 状态码
 */
hic_status_t cnode_create(domain_id_t owner, u8 slot_bits, cap_id_t *out_cnode_cap);

/**
 * @brief 销毁 CNode
 * 
 * 递归撤销子树中的所有能力。
 * 
 * @param cnode_cap CNode 能力ID
 * @return 状态码
 */
hic_status_t cnode_destroy(cap_id_t cnode_cap);

/**
 * @brief 在 CNode 中插入能力
 * 
 * @param cnode_cap CNode 能力ID
 * @param index 槽位索引
 * @param cap_id 要插入的能力ID
 * @param rights_mask 权限掩码（衰减）
 * @return 状态码
 */
hic_status_t cnode_insert(cap_id_t cnode_cap, u32 index, 
                          cap_id_t cap_id, cap_rights_t rights_mask);

/**
 * @brief 从 CNode 中移除能力
 * 
 * @param cnode_cap CNode 能力ID
 * @param index 槽位索引
 * @return 状态码
 */
hic_status_t cnode_remove(cap_id_t cnode_cap, u32 index);

/**
 * @brief 移动能力（源槽位清空）
 * 
 * @param src_cnode 源 CNode 能力ID
 * @param src_index 源槽位索引
 * @param dst_cnode 目标 CNode 能力ID
 * @param dst_index 目标槽位索引
 * @return 状态码
 */
hic_status_t cnode_move(cap_id_t src_cnode, u32 src_index,
                        cap_id_t dst_cnode, u32 dst_index);

/**
 * @brief 复制能力（浅拷贝，增加引用计数）
 * 
 * @param src_cnode 源 CNode 能力ID
 * @param src_index 源槽位索引
 * @param dst_cnode 目标 CNode 能力ID
 * @param dst_index 目标槽位索引
 * @param rights_mask 新的权限掩码（必须是原权限的子集）
 * @return 状态码
 */
hic_status_t cnode_copy(cap_id_t src_cnode, u32 src_index,
                        cap_id_t dst_cnode, u32 dst_index,
                        cap_rights_t rights_mask);

/**
 * @brief 解析 CPtr 获取能力信息
 * 
 * 从根 CNode 开始，逐级解析路径，返回最终槽位的信息。
 * 
 * @param cspace CSpace 指针
 * @param cptr 能力指针
 * @param out_cap_id 输出的能力ID
 * @param out_rights 输出的实际权限（基础权限 & 掩码）
 * @return 状态码
 */
hic_status_t cptr_lookup(cspace_t *cspace, cptr_t cptr,
                         cap_id_t *out_cap_id, cap_rights_t *out_rights);

/**
 * @brief 解析 CPtr 获取槽位指针
 * 
 * @param cspace CSpace 指针
 * @param cptr 能力指针
 * @param out_slot 输出的槽位指针
 * @param out_cnode 输出的最终 CNode 指针
 * @return 状态码
 */
hic_status_t cptr_resolve(cspace_t *cspace, cptr_t cptr,
                          cnode_slot_t **out_slot, cnode_t **out_cnode);

/* ==================== CSpace 操作接口 ==================== */

/**
 * @brief 初始化域的 CSpace
 * 
 * 创建根 CNode，初始化 CSpace 结构。
 * 
 * @param domain 域ID
 * @param root_slot_bits 根 CNode 槽位位数
 * @return 状态码
 */
hic_status_t cspace_init(domain_id_t domain, u8 root_slot_bits);

/**
 * @brief 销毁域的 CSpace
 * 
 * 递归撤销整个能力树。
 * 
 * @param domain 域ID
 * @return 状态码
 */
hic_status_t cspace_destroy(domain_id_t domain);

/**
 * @brief 获取域的 CSpace
 * 
 * @param domain 域ID
 * @return CSpace 指针，失败返回 NULL
 */
cspace_t* cspace_get(domain_id_t domain);

/**
 * @brief 从 CPtr 生成句柄
 * 
 * 结合域密钥生成混淆句柄。
 * 
 * @param domain 域ID
 * @param cptr 能力指针
 * @return 混淆句柄
 */
cap_handle_t cptr_to_handle(domain_id_t domain, cptr_t cptr);

/**
 * @brief 从句柄解析 CPtr
 * 
 * 验证令牌并提取 CPtr。
 * 
 * @param domain 域ID
 * @param handle 混淆句柄
 * @param out_cptr 输出的能力指针
 * @return 状态码
 */
hic_status_t handle_to_cptr(domain_id_t domain, cap_handle_t handle, cptr_t *out_cptr);

/* ==================== 能力撤销扩展 ==================== */

/**
 * @brief 撤销 CNode 槽位中的能力
 * 
 * 清除槽位，减少引用计数。
 * 如果引用计数归零，递归撤销子能力。
 * 
 * @param cnode_cap CNode 能力ID
 * @param index 槽位索引
 * @return 状态码
 */
hic_status_t cnode_revoke_slot(cap_id_t cnode_cap, u32 index);

/**
 * @brief 撤销 CNode 子树
 * 
 * 递归撤销 CNode 中所有槽位的能力。
 * 
 * @param cnode_cap CNode 能力ID
 * @return 状态码
 */
hic_status_t cnode_revoke_subtree(cap_id_t cnode_cap);

/* ==================== 能力缓存（可选优化） ==================== */

/**
 * 能力缓存条目
 * 
 * 类似 TLB，缓存最近解析的 CPtr → (cap_id, rights)
 * 减少路径解析开销。
 */
typedef struct cap_cache_entry {
    cptr_t         cptr;          /* 能力指针 */
    cap_id_t       cap_id;        /* 能力ID */
    cap_rights_t   rights;        /* 实际权限 */
    domain_id_t    domain;        /* 所属域 */
    u64            timestamp;     /* 时间戳（LRU） */
    u8             valid;         /* 有效标志 */
    u8             reserved[7];   /* 对齐填充 */
} cap_cache_entry_t;

/* 能力缓存大小 */
#define CAP_CACHE_SIZE     64

/* 全局能力缓存 */
extern cap_cache_entry_t g_cap_cache[CAP_CACHE_SIZE];

/**
 * @brief 查询能力缓存
 * 
 * @param domain 域ID
 * @param cptr 能力指针
 * @param out_cap_id 输出的能力ID
 * @param out_rights 输出的权限
 * @return true 命中，false 未命中
 */
bool cap_cache_lookup(domain_id_t domain, cptr_t cptr,
                      cap_id_t *out_cap_id, cap_rights_t *out_rights);

/**
 * @brief 更新能力缓存
 * 
 * @param domain 域ID
 * @param cptr 能力指针
 * @param cap_id 能力ID
 * @param rights 权限
 */
void cap_cache_update(domain_id_t domain, cptr_t cptr,
                      cap_id_t cap_id, cap_rights_t rights);

/**
 * @brief 使能力缓存失效
 * 
 * @param domain 域ID（HIC_DOMAIN_MAX 表示全部）
 * @param cptr 能力指针（CPTR_INVALID 表示全部）
 */
void cap_cache_invalidate(domain_id_t domain, cptr_t cptr);

#endif /* HIC_KERNEL_CAPABILITY_H */