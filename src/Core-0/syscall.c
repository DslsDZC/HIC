/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC系统调用实现（性能优化版）
 * 
 * 性能优化：
 * 1. 热路径移除审计日志（审计由后台线程处理）
 * 2. 使用快速能力检查
 * 3. 减少不必要的函数调用
 * 
 * 目标：系统调用延迟 20-30ns
 * 
 * 兼容层系统调用：
 * - CHAL (0x1000+): 核心硬件抽象层
 * - AAL  (0x1100+): 架构适配层
 * - IMAL (0x1200+): 隔离机制抽象层
 * - PFL  (0x1300+): 平台特性层
 */

#include "syscall.h"
#include "capability.h"
#include "domain.h"
#include "domain_switch.h"
#include "thread.h"
#include "exec_flow.h"
#include "formal_verification.h"
#include "audit.h"
#include "monitor.h"
#include "include/service_registry.h"
#include "lib/console.h"
#include "include/hal.h"
#include "hardware_probe.h"

/* 审计控制：热路径禁用同步审计 */
#define SYSCALL_AUDIT_ENABLED  0

/* 异步审计宏（热路径使用） */
#if SYSCALL_AUDIT_ENABLED
#define SYSCALL_AUDIT_LOG(domain, num, status, success) \
    do { \
        u64 _data[4] = { (u64)(domain), (u64)(num), (u64)(status), 0 }; \
        audit_log_event(AUDIT_EVENT_SYSCALL, domain, 0, 0, _data, 4, success); \
    } while(0)
#else
#define SYSCALL_AUDIT_LOG(domain, num, status, success) ((void)0)
#endif

/* IPC调用实现（优化版） */
hic_status_t syscall_ipc_call(ipc_call_params_t *params)
{
    domain_id_t caller_domain = domain_switch_get_current();

    /* 快速能力检查（内联，无审计开销） */
    cap_entry_t *entry = &g_global_cap_table[params->endpoint_cap];
    
    if (entry->cap_id != params->endpoint_cap || (entry->flags & CAP_FLAG_REVOKED)) {
        SYSCALL_AUDIT_LOG(caller_domain, SYSCALL_IPC_CALL, HIC_ERROR_CAP_INVALID, 0);
        return HIC_ERROR_CAP_INVALID;
    }

    /* 执行域切换 */
    hic_status_t status = domain_switch(caller_domain, entry->endpoint.target,
                           params->endpoint_cap, 0, NULL, 0);
    
    SYSCALL_AUDIT_LOG(caller_domain, SYSCALL_IPC_CALL, status, status == HIC_SUCCESS);
    
    return status;
}

/* 能力传递（优化版） */
hic_status_t syscall_cap_transfer(domain_id_t to, cap_id_t cap)
{
    domain_id_t from = domain_switch_get_current();
    cap_handle_t out_handle;
    hic_status_t status = cap_transfer(from, to, cap, &out_handle);
    
    SYSCALL_AUDIT_LOG(from, SYSCALL_CAP_TRANSFER, status, status == HIC_SUCCESS);
    
    return status;
}

/* 能力派生（优化版） */
hic_status_t syscall_cap_derive(cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out)
{
    domain_id_t owner = domain_switch_get_current();
    hic_status_t status = cap_derive(owner, parent, sub_rights, out);
    
    SYSCALL_AUDIT_LOG(owner, SYSCALL_CAP_DERIVE, status, status == HIC_SUCCESS);
    
    return status;
}

/* 能力撤销（优化版） */
hic_status_t syscall_cap_revoke(cap_id_t cap)
{
    domain_id_t from = domain_switch_get_current();
    hic_status_t status = cap_revoke(cap);
    
    SYSCALL_AUDIT_LOG(from, SYSCALL_CAP_REVOKE, status, status == HIC_SUCCESS);
    
    return status;
}

/* 系统调用入口（优化版） */
void syscall_handler(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    (void)arg3;
    (void)arg4;
    hic_status_t status = HIC_SUCCESS;

    switch (syscall_num) {
        case SYSCALL_IPC_CALL:
            status = syscall_ipc_call((ipc_call_params_t*)arg1);
            break;
            
        case SYSCALL_CAP_TRANSFER:
            status = syscall_cap_transfer((cap_id_t)arg1, (domain_id_t)arg2);
            break;
            
        case SYSCALL_CAP_DERIVE:
            status = syscall_cap_derive((cap_id_t)arg1, 0, 0);
            break;
            
        case SYSCALL_CAP_REVOKE:
            status = cap_revoke((cap_id_t)arg1);
            break;
            
        /* 监控与安全系统调用 */
        case SYSCALL_MONITOR_SET_RULE:
            status = monitor_set_rule((const monitor_rule_t*)arg1);
            break;
            
        case SYSCALL_MONITOR_GET_RULE:
            status = monitor_get_rule((monitor_event_type_t)arg1, (monitor_rule_t*)arg2);
            break;
            
        case SYSCALL_MONITOR_GET_STATS:
            monitor_get_all_stats((event_stat_t*)arg1, (u32)arg2, (u32*)arg3);
            status = HIC_SUCCESS;
            break;
            
        case SYSCALL_MONITOR_EXEC_ACTION:
            status = monitor_execute_action((monitor_action_t)arg1, (domain_id_t)arg2);
            break;
            
        case SYSCALL_CRASH_DUMP_RETRIEVE:
            status = crash_dump_retrieve((domain_id_t)arg1, (void*)arg2, 
                                          (size_t)arg3, (size_t*)arg4);
            break;
            
        case SYSCALL_CRASH_DUMP_CLEAR:
            crash_dump_clear((domain_id_t)arg1);
            status = HIC_SUCCESS;
            break;
            
        case SYSCALL_AUDIT_QUERY:
            /* 审计查询（机制层）
             * arg1: filter (audit_query_filter_t*)
             * arg2: buffer (输出缓冲区)
             * arg3: buffer_size
             * arg4: out_size (输出实际大小) */
            {
                size_t out_sz = 0;
                status = audit_query((const audit_query_filter_t*)arg1,
                                     (void*)arg2, (size_t)arg3, &out_sz);
                if (status == HIC_SUCCESS && arg4) {
                    *(size_t*)arg4 = out_sz;
                }
            }
            break;
            
        /* DoS 防护系统调用 */
        case SYSCALL_QUOTA_CHECK:
            {
                size_t available = 0;
                quota_check_result_t result = domain_quota_check((domain_id_t)arg1, 
                                                                  (quota_type_t)arg2, 
                                                                  (size_t)arg3,
                                                                  &available);
                status = (result == QUOTA_CHECK_OK) ? HIC_SUCCESS : HIC_ERROR_QUOTA_EXCEEDED;
            }
            break;
            
        case SYSCALL_QUOTA_CONSUME:
            status = domain_quota_consume((domain_id_t)arg1, 
                                          (quota_type_t)arg2, 
                                          (size_t)arg3);
            break;
            
        case SYSCALL_QUOTA_DELEGATE:
            status = domain_quota_delegate((domain_id_t)arg1, 
                                           (domain_id_t)arg2,
                                           (size_t)arg3, 
                                           (u32)arg4);
            break;
            
        case SYSCALL_QUOTA_GET_USAGE:
            /* 通过 domain_get_info 获取配额使用情况 */
            {
                domain_t info;
                status = domain_get_info((domain_id_t)arg1, &info);
                if (status == HIC_SUCCESS && arg2) {
                    domain_quota_usage_t *usage = (domain_quota_usage_t*)arg2;
                    usage->memory_used = info.usage.memory_used;
                    usage->thread_used = info.usage.thread_used;
                    usage->cap_used = info.cap_count;
                    usage->max_memory = info.quota.max_memory;
                    usage->max_threads = info.quota.max_threads;
                    usage->max_caps = info.quota.max_caps;
                }
            }
            break;
            
        case SYSCALL_EMERGENCY_GET_LEVEL:
            status = (hic_status_t)domain_detect_emergency();
            break;
            
        case SYSCALL_EMERGENCY_GET_STATUS:
            domain_get_system_status((system_resource_status_t*)arg1);
            status = HIC_SUCCESS;
            break;
            
        case SYSCALL_EMERGENCY_TRIGGER:
            status = (hic_status_t)domain_trigger_emergency_action(
                        (emergency_level_t)arg1, (bool)arg2);
            break;
            
        case SYSCALL_FLOW_CONTROL_INIT:
            status = flow_control_init((const char*)arg1, 
                                        (flow_control_policy_t)arg2,
                                        (const u32*)arg3);
            break;
            
        case SYSCALL_FLOW_CONTROL_CHECK:
            {
                service_endpoint_t *ep = service_find_by_name((const char*)arg1);
                if (ep) {
                    status = (hic_status_t)flow_control_check(ep, (domain_id_t)arg2);
                } else {
                    status = HIC_ERROR_NOT_FOUND;
                }
            }
            break;
            
        case SYSCALL_FLOW_CONTROL_REFILL:
            status = flow_control_refill_credits((const char*)arg1, (u32)arg2);
            break;
            
        case SYSCALL_FLOW_CONTROL_GET_STATS:
            status = flow_control_get_stats((const char*)arg1, 
                                             (flow_control_state_t*)arg2);
            break;
            
        case SYSCALL_DOMAIN_CLEANUP:
            /* 原子性清理：销毁域自动清理所有资源 */
            status = domain_destroy((domain_id_t)arg1);
            break;
            
        /* 能力传递（带权限衰减）
         * arg1: from_domain
         * arg2: to_domain  
         * arg3: cap_id
         * arg4: struct { cap_rights_t rights; cap_handle_t *out; } */
        case SYSCALL_CAP_TRANSFER_ATTENUATE:
            {
                struct {
                    cap_rights_t rights;
                    cap_handle_t *out;
                } *params = (void*)arg4;
                if (params) {
                    status = cap_transfer_with_attenuation((domain_id_t)arg1,
                                                            (domain_id_t)arg2,
                                                            (cap_id_t)arg3,
                                                            params->rights,
                                                            params->out);
                } else {
                    status = HIC_ERROR_INVALID_PARAM;
                }
            }
            break;
            
        /* 共享内存系统调用
         * arg1: owner domain
         * arg2: size
         * arg3: flags
         * arg4: struct { cap_id_t *out_cap; cap_handle_t *out_handle; } */
        case SYSCALL_SHMEM_ALLOC:
            {
                struct {
                    cap_id_t *out_cap;
                    cap_handle_t *out_handle;
                } *params = (void*)arg4;
                if (params) {
                    status = shmem_alloc((domain_id_t)arg1, (size_t)arg2,
                                         (u32)arg3, params->out_cap, params->out_handle);
                } else {
                    status = HIC_ERROR_INVALID_PARAM;
                }
            }
            break;
            
        /* 共享内存映射
         * arg1: from_domain
         * arg2: to_domain
         * arg3: cap_id
         * arg4: struct { cap_rights_t rights; cap_handle_t *out; } */
        case SYSCALL_SHMEM_MAP:
            {
                struct {
                    cap_rights_t rights;
                    cap_handle_t *out;
                } *params = (void*)arg4;
                if (params) {
                    status = shmem_map((domain_id_t)arg1, (domain_id_t)arg2,
                                       (cap_id_t)arg3, params->rights, params->out);
                } else {
                    status = HIC_ERROR_INVALID_PARAM;
                }
            }
            break;
            
        case SYSCALL_SHMEM_UNMAP:
            status = shmem_unmap((domain_id_t)arg1, (cap_handle_t)arg2);
            break;
            
        case SYSCALL_SHMEM_GET_INFO:
            status = shmem_get_info((cap_id_t)arg1, (shmem_region_t*)arg2);
            break;

        /* 执行流能力系统调用（EFC） */
        case SYSCALL_EXEC_FLOW_CREATE:
            {
                exec_flow_id_t efc;
                status = exec_flow_create((domain_id_t)arg1, (virt_addr_t)arg2, &efc);
                if (status == HIC_SUCCESS) {
                    *(exec_flow_id_t*)arg3 = efc;
                }
            }
            break;

        case SYSCALL_EXEC_FLOW_DESTROY:
            status = exec_flow_destroy((exec_flow_id_t)arg1);
            break;

        case SYSCALL_EXEC_FLOW_DISPATCH:
            status = exec_flow_dispatch((exec_flow_id_t)arg1,
                                        (logical_core_id_t)arg2);
            break;

        case SYSCALL_EXEC_FLOW_BLOCK:
            status = exec_flow_block((exec_flow_id_t)arg1);
            break;

        case SYSCALL_EXEC_FLOW_WAKE:
            status = exec_flow_wake((exec_flow_id_t)arg1);
            break;

        case SYSCALL_EXEC_FLOW_GET_STATE:
            {
                exec_flow_state_t state;
                status = exec_flow_get_state((exec_flow_id_t)arg1, &state);
                if (status == HIC_SUCCESS) {
                    *(exec_flow_state_t*)arg2 = state;
                }
            }
            break;

        case SYSCALL_EXEC_FLOW_YIELD:
            exec_flow_yield();
            status = HIC_SUCCESS;
            break;

        default:
            status = HIC_ERROR_NOT_SUPPORTED;
            break;
    }

    hal_syscall_return(status);
}

/* 检查是否有待处理的系统调用 */
bool syscalls_pending(void)
{
    extern bool syscall_queue_empty(void);
    return !syscall_queue_empty();
}

/* 处理待处理的系统调用 */
void handle_pending_syscalls(void)
{
    extern void syscall_process_queue(void);
    syscall_process_queue();
}

/* 检查系统调用队列是否为空 */
bool syscall_queue_empty(void)
{
    extern bool g_syscall_queue_empty;
    return g_syscall_queue_empty;
}

/* 处理系统调用队列 */
void syscall_process_queue(void)
{
    extern void g_syscall_process_all(void);
    g_syscall_process_all();
}