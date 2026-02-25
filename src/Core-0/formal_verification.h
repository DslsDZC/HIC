/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核形式化验证头文件
 * 遵循TD/三层模型.md文档第12节
 */

#ifndef FORMAL_VERIFICATION_H
#define FORMAL_VERIFICATION_H

#include <stdint.h>
#include "types.h"

/* 形式化验证错误码 */
#define FV_SUCCESS           0
#define FV_CAP_INVALID       1
#define FV_DOMAIN_ISOLATION  2
#define FV_QUOTA_VIOLATION   3
#define FV_ATOMICITY_FAIL    4
#define FV_DEADLOCK_DETECTED 5

/* 常量定义 */
#define MAX_DOMAINS          128    /* 从256减小到128，减少BSS段大小 */
#define MAX_CAPABILITIES     65536
#define MAX_THREADS          256    /* 从1024减小到256，减少BSS段大小 */
#define DEADLOCK_THRESHOLD   5000  // 5秒

/* 内存区域 */
typedef struct mem_region {
    u64 base;
    u64 size;
} mem_region_t;

/* 能力类型 */
typedef enum {
    CAP_MEMORY = 0,
    CAP_DEVICE,
    CAP_IPC,
    CAP_THREAD,
    CAP_SHARED,
    CAP_CAP_DERIVE,        /* 派生能力 */
    CAP_IRQ,               /* 中断能力 */
    CAP_ENDPOINT,          /* IPC端点 */
    CAP_SERVICE,           /* 服务能力 */
    CAP_MMIO,              /* MMIO区域能力 */
    CAP_TYPE_COUNT
} cap_type_t;

/* 对象类型 */
typedef enum {
    OBJ_MEMORY,
    OBJ_DEVICE,
    OBJ_IPC,
    OBJ_THREAD,
    OBJ_SHARED,
    OBJ_TYPE_COUNT
} obj_type_t;

/* 系统调用ID */
typedef u64 syscall_id_t;

/* 系统调用枚举 */
enum {
    SYS_CAP_GRANT = 1,
    SYS_CAP_REVOKE = 2,
    SYS_CAP_DERIVE = 3,
    SYS_MEM_ALLOCATE = 4,
    SYS_MEM_FREE = 5,
    SYS_THREAD_CREATE = 6,
    SYS_THREAD_DESTROY = 7,
};

/* 审计日志级别 */
typedef enum {
    AUDIT_SEVERITY_INFO,
    AUDIT_SEVERITY_WARNING,
    AUDIT_SEVERITY_CRITICAL
} audit_severity_t;

/* 权限标志 */
#define CAP_READ     (1ULL << 0)
#define CAP_WRITE    (1ULL << 1)
#define CAP_EXECUTE  (1ULL << 2)
#define CAP_GRANT    (1ULL << 3)
#define CAP_REVOKE   (1ULL << 4)

/* 无效ID */
#define INVALID_CAP_ID       ((cap_id_t)-1)

/* 形式化验证API */

/**
 * 检查所有不变式
 * 
 * 返回值：FV_SUCCESS 如果所有不变式成立，否则返回错误码
 */
int fv_check_all_invariants(void);

/**
 * 验证系统调用原子性
 * 
 * 参数：
 *   syscall_id - 系统调用ID
 *   pre_state - 调用前状态哈希
 *   post_state - 调用后状态哈希
 * 
 * 返回值：true 如果原子性得到保证，否则 false
 */
bool fv_verify_syscall_atomicity(syscall_id_t syscall_id, u64 pre_state, u64 post_state);

/**
 * 验证域间隔离性
 * 
 * 参数：
 *   d1 - 域1 ID
 *   d2 - 域2 ID
 * 
 * 返回值：true 如果隔离性得到保证，否则 false
 */
bool fv_verify_domain_isolation(domain_id_t d1, domain_id_t d2);

/**
 * 形式化验证初始化
 */
void fv_init(void);

/**
 * 获取验证统计信息
 * 
 * 参数：
 *   total_checks - 总检查次数（可为NULL）
 *   violations - 违反次数（可为NULL）
 *   last_violation_id - 最后违反的不变式ID（可为NULL）
 */
void fv_get_stats(u64* total_checks, u64* violations, u64* last_violation_id);

/**
 * 获取详细的验证报告
 * 
 * 参数：
 *   report - 输出报告缓冲区
 *   size - 缓冲区大小
 * 
 * 返回值：实际写入的字节数
 */
u64 fv_get_report(char* report, u64 size);

/**
 * 验证覆盖率统计
 */
typedef struct fv_coverage {
    u64 total_code_paths;     // 总代码路径数
    u64 verified_paths;       // 已验证路径数
    u64 coverage_percent;     // 覆盖率百分比
    u64 last_verify_time;     // 最后验证时间
} fv_coverage_t;

/**
 * 获取验证覆盖率
 * 
 * 参数：
 *   coverage - 输出覆盖率统计
 */
void fv_get_coverage(fv_coverage_t* coverage);

/* ========== 不变式依赖关系 ========== */

/**
 * 不变式依赖关系
 */
typedef struct invariant_dependency {
    u64 invariant_id;         // 不变式ID
    u64 depends_on[6];        // 依赖的不变式ID列表
    u64 dependency_count;     // 依赖数量
} invariant_dependency_t;

/**
 * 获取不变式依赖关系
 * 
 * 参数：
 *   deps - 输出依赖关系数组
 *   count - 数组大小
 * 
 * 返回值：实际写入的依赖关系数量
 */
u64 fv_get_invariant_dependencies(invariant_dependency_t* deps, u64 count);

/* ========== 验证时序保证 ========== */

/**
 * 验证时序状态
 */
typedef enum {
    FV_STATE_IDLE,           // 空闲
    FV_STATE_CHECKING,       // 检查中
    FV_STATE_VIOLATED,       // 违反
    FV_STATE_RECOVERING      // 恢复中
} fv_state_t;

/**
 * 获取当前验证状态
 * 
 * 返回值：当前验证状态
 */
fv_state_t fv_get_state(void);

/**
 * 设置验证状态
 * 
 * 参数：
 *   state - 新的验证状态
 */
void fv_set_state(fv_state_t state);

/* ========== 证明检查点 ========== */

/**
 * 证明检查点
 */
typedef struct proof_checkpoint {
    u64 checkpoint_id;       // 检查点ID
    u64 theorem_id;          // 关联的定理ID
    u64 invariant_id;        // 关联的不变式ID
    const char* proof_step;  // 证明步骤描述
    bool verified;           // 是否已验证
    u64 verify_time;         // 验证时间戳
} proof_checkpoint_t;

/**
 * 注册证明检查点
 * 
 * 参数：
 *   theorem_id - 定理ID
 *   invariant_id - 不变式ID
 *   proof_step - 证明步骤描述
 * 
 * 返回值：检查点ID
 */
u64 fv_register_checkpoint(u64 theorem_id, u64 invariant_id, const char* proof_step);

/**
 * 验证检查点
 * 
 * 参数：
 *   checkpoint_id - 检查点ID
 * 
 * 返回值：true 如果验证成功
 */
bool fv_verify_checkpoint(u64 checkpoint_id);

/**
 * 获取所有检查点
 * 
 * 参数：
 *   checkpoints - 输出检查点数组
 *   count - 数组大小
 * 
 * 返回值：实际写入的检查点数量
 */
u64 fv_get_checkpoints(proof_checkpoint_t* checkpoints, u64 count);

/* ========== 验证状态机 ========== */

/**
 * 验证状态机事件
 */
typedef enum {
    FV_EVENT_START_CHECK,    // 开始检查
    FV_EVENT_CHECK_PASS,     // 检查通过
    FV_EVENT_CHECK_FAIL,     // 检查失败
    FV_EVENT_RECOVERY_START, // 开始恢复
    FV_EVENT_RECOVERY_END,   // 恢复结束
} fv_event_t;

/**
 * 验证状态机转换
 */
typedef struct fv_transition {
    fv_state_t from_state;   // 源状态
    fv_event_t event;        // 事件
    fv_state_t to_state;     // 目标状态
    bool (*action)(void);    // 转换动作
} fv_transition_t;

/**
 * 触发验证事件
 * 
 * 参数：
 *   event - 事件类型
 * 
 * 返回值：true 如果状态转换成功
 */
bool fv_trigger_event(fv_event_t event);

/* ========== 形式化规范 ========== */

/**
 * 不变式形式化规范
 */
typedef struct invariant_spec {
    u64 invariant_id;         // 不变式ID
    const char* name;         // 名称
    const char* formal_expr;  // 形式化表达式
    const char* description;  // 描述
    bool (*check)(void);      // 检查函数
} invariant_spec_t;

/**
 * 获取不变式形式化规范
 * 
 * 参数：
 *   specs - 输出规范数组
 *   count - 数组大小
 * 
 * 返回值：实际写入的规范数量
 */
u64 fv_get_invariant_specs(invariant_spec_t* specs, u64 count);

/* 内部辅助函数（用于形式化证明） */

/**
 * 注册代码路径（用于覆盖率统计）
 */
void fv_register_code_path(void);

/**
 * 标记路径已验证（用于覆盖率统计）
 */
void fv_mark_path_verified(void);

/* 外部依赖接口（由其他内核模块提供） */

/* 域管理接口 */
bool domain_is_active(domain_id_t domain);
u64 count_domain_capabilities(domain_id_t domain);
u64 get_domain_initial_cap_quota(domain_id_t domain);
u64 get_domain_granted_caps(domain_id_t domain);
u64 get_domain_revoked_caps(domain_id_t domain);
mem_region_t get_domain_memory_region(domain_id_t domain);
u64 get_domain_allocated_memory(domain_id_t domain);
u64 get_domain_cpu_quota(domain_id_t domain);

/* 能力管理接口 */
bool capability_exists(cap_id_t cap);
cap_id_t* get_capability_derivatives(cap_id_t cap);
u64 get_capability_permissions(cap_id_t cap);
cap_type_t get_capability_type(cap_id_t cap);
obj_type_t get_capability_object_type(cap_id_t cap);
cap_id_t* get_shared_capabilities(domain_id_t d1, domain_id_t d2);

/* 线程管理接口 */
/* thread.h 中已定义，此处移除重复声明 */
/* bool thread_is_active(thread_id_t thread); */
u64 get_thread_wait_time(thread_id_t thread);

/* 系统服务接口 */
bool fv_verify_syscall_atomicity(syscall_id_t syscall_id, u64 pre_state, u64 post_state);

#endif /* FORMAL_VERIFICATION_H */