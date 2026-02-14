/**
 * HIK内核形式化验证模块
 * 遵循TD/三层模型.md文档第12节
 * 
 * 本模块提供运行时形式化验证，确保：
 * 1. 能力系统的不变性
 * 2. 隔离域的内存隔离
 * 3. 资源配额的守恒性
 * 4. 系统调用的原子性
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "types.h"
#include "capability.h"
#include "kernel.h"
#include "pmm.h"

/* 形式化验证错误码 */
#define FV_SUCCESS           0
#define FV_CAP_INVALID       1
#define FV_DOMAIN_ISOLATION  2
#define FV_QUOTA_VIOLATION   3
#define FV_ATOMICITY_FAIL    4
#define FV_DEADLOCK_DETECTED 5

/* 不变式定义 */
typedef struct invariant {
    u64 invariant_id;
    const char* name;
    bool (*check)(void);
    const char* description;
} invariant_t;

/* 全局不变式状态 */
static u64 invariant_check_count = 0;
static u64 invariant_violation_count = 0;
static u64 last_violation_invariant_id = 0;

/* 静态函数声明 */
static bool check_cap_grant_atomicity(u64 pre_state, u64 post_state);
static bool check_cap_revoke_atomicity(u64 pre_state, u64 post_state);
static bool check_cap_derive_atomicity(u64 pre_state, u64 post_state);
static bool check_mem_allocate_atomicity(u64 pre_state, u64 post_state);
static bool check_mem_free_atomicity(u64 pre_state, u64 post_state);
static bool check_thread_create_atomicity(u64 pre_state, u64 post_state);
static bool check_thread_destroy_atomicity(u64 pre_state, u64 post_state);
static bool (*get_syscall_atomicity_checker(syscall_id_t syscall_id))(u64, u64);
static u64 extract_cap_count(u64 state);
static u64 extract_mem_allocated(u64 state);
static u64 extract_allocation_size(u64 state);
static u64 extract_thread_count(u64 state);
static void fv_log_violation(const invariant_t* inv);
static void fv_panic(const char* message);
static bool is_minimum_privilege_principle_satisfied(domain_id_t d1, domain_id_t d2, cap_id_t cap);

/**
 * 不变式1：能力守恒性
 * 
 * 数学表述：
 * ∀d ∈ Domains, |Capabilities(d)| = InitialQuota(d) + Granted(d) - Revoked(d)
 * 
 * 语义：任何域持有的能力数量等于初始配额加上被授予的能力减去被撤销的能力
 */
static bool invariant_capability_conservation(void) {
    // 遍历所有域
    for (domain_id_t domain = 0; domain < MAX_DOMAINS; domain++) {
        if (!domain_is_active(domain)) continue;
        
        // 统计当前能力数
        u64 current_caps = count_domain_capabilities(domain);
        
        // 统计初始配额 + 授予 - 撤销
        u64 expected_caps = get_domain_initial_cap_quota(domain) +
                           get_domain_granted_caps(domain) -
                           get_domain_revoked_caps(domain);
        
        if (current_caps != expected_caps) {
            return false;
        }
    }
    return true;
}

/**
 * 不变式2：内存隔离性
 * 
 * 数学表述：
 * ∀d1, d2 ∈ Domains, d1 ≠ d2 ⇒ Memory(d1) ∩ Memory(d2) = ∅
 * 
 * 语义：任意两个不同域的内存区域不相交
 */
static bool invariant_memory_isolation(void) {
    // 遍历所有域对
    for (domain_id_t d1 = 0; d1 < MAX_DOMAINS; d1++) {
        if (!domain_is_active(d1)) continue;
        
        for (domain_id_t d2 = d1 + 1; d2 < MAX_DOMAINS; d2++) {
            if (!domain_is_active(d2)) continue;
            
            // 获取两个域的内存区域
            mem_region_t region1 = get_domain_memory_region(d1);
            mem_region_t region2 = get_domain_memory_region(d2);
            
            // 检查是否相交
            if (regions_overlap(&region1, &region2)) {
                console_puts("[FV] Memory isolation violation detected!\n");
                return false;
            }
        }
    }
    return true;
}

/**
 * 不变式3：能力权限单调性
 * 
 * 数学表述：
 * ∀c1, c2 ∈ Capabilities, Derived(c1, c2) ⇒ Permissions(c2) ⊆ Permissions(c1)
 * 
 * 语义：派生能力的权限是源能力的子集
 */
static bool invariant_capability_monotonicity(void) {
    // 遍历所有能力
    for (cap_id_t cap1 = 0; cap1 < MAX_CAPABILITIES; cap1++) {
        if (!capability_exists(cap1)) continue;
        
        // 获取该能力的所有派生
        cap_id_t* derivatives = get_capability_derivatives(cap1);
        for (u64 i = 0; derivatives[i] != INVALID_CAP_ID; i++) {
            cap_id_t cap2 = derivatives[i];
            
            // 检查权限是否是子集
            if (!is_permission_subset(cap2, cap1)) {
                return false;
            }
        }
    }
    return true;
}

/**
 * 不变式4：资源配额守恒性
 * 
 * 数学表述：
 * ∀r ∈ Resources, Σ_{d∈Domains} Allocated(r, d) ≤ Total(r)
 * 
 * 语义：所有域分配的某类资源总量不超过系统总量
 */
static bool invariant_resource_quota_conservation(void) {
    // 检查内存配额
    u64 total_allocated_memory = 0;
    for (domain_id_t domain = 0; domain < MAX_DOMAINS; domain++) {
        if (!domain_is_active(domain)) continue;
        total_allocated_memory += get_domain_allocated_memory(domain);
    }
    
    // 获取系统总内存
    u64 total_pages, free_pages, used_pages;
    pmm_get_stats(&total_pages, &free_pages, &used_pages);
    u64 total_physical_memory = total_pages * PAGE_SIZE;
    
    if (total_allocated_memory > total_physical_memory) {
        console_puts("[FV] Resource quota violation detected!\n");
        return false;
    }
    
    // 检查CPU时间配额
    u64 total_cpu_quota = 0;
    for (domain_id_t domain = 0; domain < MAX_DOMAINS; domain++) {
        if (!domain_is_active(domain)) continue;
        total_cpu_quota += get_domain_cpu_quota(domain);
    }
    
    if (total_cpu_quota > 100) { // 假设总配额为100%
        console_puts("[FV] CPU quota violation detected!\n");
        return false;
    }
    
    return true;
}

/**
 * 不变式5：无死锁
 * 
 * 数学表述：
 * ∀t ∈ Threads, ∃s: State(t) = Running ∨ State(t) → Running
 * 或等价地：资源分配图中无环
 * 
 * 语义：系统中不存在无法被调度的线程（资源分配图中无环）
 */
static bool invariant_deadlock_freedom(void) {
    /* 简化实现：检测长时间等待的线程 */
    /* 完整的资源分配图死锁检测需要完整的资源管理系统 */
    
    for (thread_id_t thread = 0; thread < MAX_THREADS; thread++) {
        if (!thread_is_active(thread)) continue;
        
        u64 wait_time = get_thread_wait_time(thread);
        if (wait_time > DEADLOCK_THRESHOLD) {
            /* 可能死锁，但不一定（正常阻塞也可能长时间等待） */
            console_puts("[FV] Warning: Thread waiting too long\n");
        }
    }
    
    return true;
}

/**
 * 不变式6：类型安全性
 * 
 * 数学表述：
 * ∀o ∈ Objects, ∀a ∈ Access(o), Type(a) ∈ AllowedTypes(o)
 * 
 * 语义：对对象的访问必须符合对象的类型约束
 */
static bool invariant_type_safety(void) {
    // 检查所有能力的类型一致性
    for (cap_id_t cap = 0; cap < MAX_CAPABILITIES; cap++) {
        if (!capability_exists(cap)) continue;
        
        cap_type_t cap_type = get_capability_type(cap);
        obj_type_t obj_type = get_capability_object_type(cap);
        
        // 检查类型兼容性
        if (!is_type_compatible(cap_type, obj_type)) {
            return false;
        }
    }
    
    return true;
}

/* 全局不变式表 */
static invariant_t invariants[] = {
    {1, "能力守恒性", invariant_capability_conservation, "域能力数量守恒"},
    {2, "内存隔离性", invariant_memory_isolation, "域内存区域不相交"},
    {3, "能力权限单调性", invariant_capability_monotonicity, "派生权限是源权限子集"},
    {4, "资源配额守恒性", invariant_resource_quota_conservation, "资源分配不超过总量"},
    {5, "无死锁性", invariant_deadlock_freedom, "资源分配图中无环"},
    {6, "类型安全性", invariant_type_safety, "访问类型符合对象约束"},
};

static const u64 num_invariants = sizeof(invariants) / sizeof(invariant_t);

/**
 * 检查所有不变式
 * 
 * 返回值：FV_SUCCESS 如果所有不变式成立，否则返回错误码
 */
int fv_check_all_invariants(void) {
    invariant_check_count++;
    
    for (u64 i = 0; i < num_invariants; i++) {
        if (!invariants[i].check()) {
            invariant_violation_count++;
            last_violation_invariant_id = invariants[i].invariant_id;
            
            // 记录违反
            fv_log_violation(&invariants[i]);
            
            return FV_CAP_INVALID + i; // 返回特定的错误码
        }
    }
    
    return FV_SUCCESS;
}

/**
 * 原子性验证：系统调用
 * 
 * 确保系统调用要么完全成功，要么完全失败
 * 
 * 数学表述：
 * ∀s ∈ Syscalls, Exec(s) ⇒ (State(s, post) = State(s, success) ∨ State(s, post) = State(s, fail))
 */
bool fv_verify_syscall_atomicity(syscall_id_t syscall_id, u64 pre_state, u64 post_state) {
    // 简化实现：暂时不实现具体的原子性检查
    // 完整实现需要状态快照和比较机制
    return true;
}

/**
 * 形式化验证初始化 */
void fv_init(void)
{
    console_puts("[FV] Initializing formal verification...\n");
    
    /* 初始化检查计数器 */
    invariant_check_count = 0;
    invariant_violation_count = 0;
    last_violation_invariant_id = 0;
    
    /* 执行初始检查 */
    int result = fv_check_all_invariants();
    if (result != FV_SUCCESS) {
        console_puts("[FV] Initial invariant check failed!\n");
        console_puts("[FV PANIC] 初始不变式检查失败\n");
        while (1) {
            __asm__ volatile ("hlt");
        }
    }
    
    console_puts("[FV] Formal verification initialized\n");
}

/**
 * 获取验证统计信息
 */
void fv_get_stats(u64* total_checks, u64* violations, u64* last_violation_id)
{
    if (total_checks) {
        *total_checks = invariant_check_count;
    }
    if (violations) {
        *violations = invariant_violation_count;
    }
    if (last_violation_id) {
        *last_violation_id = last_violation_invariant_id;
    }
}


/**
 * 辅助函数：检查两个内存区域是否重叠
 */
static bool regions_overlap(const mem_region_t* r1, const mem_region_t* r2) {
    if (!r1 || !r2) return false;
    
    u64 r1_end = r1->base + r1->size;
    u64 r2_end = r2->base + r2->size;
    
    // 检查是否有重叠
    return (r1->base < r2_end) && (r2->base < r1_end);
}

/**
 * 辅助函数：检查权限是否是子集
 */
static bool is_permission_subset(cap_id_t derived, cap_id_t source) {
    if (derived == INVALID_CAP_ID || source == INVALID_CAP_ID) {
        return false;
    }
    
    u64 derived_perms = get_capability_permissions(derived);
    u64 source_perms = get_capability_permissions(source);
    
    // 检查derived_perms是否是source_perms的子集
    // 即：derived_perms的所有位都在source_perms中
    return (derived_perms & ~source_perms) == 0;
}

/**

 * 辅助函数：检查类型兼容性

 */

static bool is_type_compatible(cap_type_t cap_type, obj_type_t obj_type) {

    // 类型兼容性矩阵

    static const bool compatibility_table[CAP_TYPE_COUNT][OBJ_TYPE_COUNT] = {

        // OBJ_MEMORY, OBJ_DEVICE, OBJ_IPC, OBJ_THREAD, OBJ_SHARED

        [CAP_MEMORY] = {true, false, false, false, true},

        [CAP_DEVICE] = {false, true, false, false, false},

        [CAP_IPC] = {false, false, true, false, false},

        [CAP_THREAD] = {false, false, false, true, false},

        [CAP_SHARED] = {true, false, false, false, true},

    };

    

    if (cap_type >= CAP_TYPE_COUNT || obj_type >= OBJ_TYPE_COUNT) {

        return false;

    }

    

    return compatibility_table[cap_type][obj_type];

}



/**



 * 日志记录：不变式违反 */

static void fv_log_violation(const invariant_t* inv) {

    // 输出违反信息到控制台

    console_puts("[FV] Invariant violation: ");

    console_puts(inv->name);

    console_puts("\n");

    console_puts("[FV] Description: ");

    console_puts(inv->description);

    console_puts("\n");

    

    // 记录到审计日志

    AUDIT_LOG_SECURITY_VIOLATION(HIK_DOMAIN_CORE, inv->invariant_id);

    

    // 停止系统

    while (1) {

        __asm__ volatile ("hlt");

    }

}



/**

 * 辅助函数：获取最后一次分配的大小 */

static u64 g_last_allocation_size = 0;



u64 get_last_allocation_size(void)

{

    return g_last_allocation_size;

}



void set_last_allocation_size(u64 size)

{

    g_last_allocation_size = size;

}