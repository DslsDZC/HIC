/**
 * HIK系统调用实现（集成域切换）
 * 遵循文档第3.2节：统一API访问模型
 */

#include "syscall.h"
#include "capability.h"
#include "domain.h"
#include "domain_switch.h"
#include "thread.h"
#include "formal_verification.h"
#include "lib/console.h"

/* IPC调用实现 */
hik_status_t syscall_ipc_call(ipc_call_params_t *params)
{
    if (params == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 1. 验证调用者是否持有目标端点能力 */
    thread_id_t current_thread = get_current_thread();
    if (current_thread == INVALID_THREAD) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    thread_t* thread = get_thread(current_thread);
    if (thread == NULL) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    domain_id_t caller_domain = thread->domain_id;
    
    hik_status_t status = cap_check_access(caller_domain, 
                                            params->endpoint_cap, 0);
    if (status != HIK_SUCCESS) {
        return HIK_ERROR_PERMISSION;
    }
    
    /* 2. 获取端点信息 */
    cap_entry_t endpoint_info;
    status = cap_get_info(params->endpoint_cap, &endpoint_info);
    if (status != HIK_SUCCESS) {
        return HIK_ERROR_CAP_INVALID;
    }
    
    /* 3. 安全切换到目标服务域 */
    status = domain_switch(caller_domain, endpoint_info.endpoint.target_domain,
                           params->endpoint_cap, 0, NULL, 0);
    if (status != HIK_SUCCESS) {
        return status;
    }
    
    console_puts("[SYSCALL] IPC call to domain ");
    console_putu64(endpoint_info.endpoint.target_domain);
    console_puts("\n");
    
    /* 记录IPC调用审计日志 */
    AUDIT_LOG_IPC_CALL(caller_domain, params->endpoint_cap, true);
    
    return HIK_SUCCESS;
}

/* 能力传递 */
hik_status_t syscall_cap_transfer(domain_id_t to, cap_id_t cap)
{
    domain_id_t from = domain_switch_get_current();
    return cap_transfer(from, to, cap);
}

/* 能力派生 */
hik_status_t syscall_cap_derive(cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out)
{
    domain_id_t owner = domain_switch_get_current();
    return cap_derive(owner, parent, sub_rights, out);
}

/* 能力撤销 */
hik_status_t syscall_cap_revoke(cap_id_t cap)
{
    return cap_revoke(cap);
}

/* 系统调用入口 */
void syscall_handler(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    hik_status_t status = HIK_SUCCESS;
    
    /* 获取当前域和线程 */
    thread_id_t current_thread = get_current_thread();
    thread_t* thread = get_thread(current_thread);
    domain_id_t current_domain = thread ? thread->domain_id : 0;
    
    switch (syscall_num) {
        case SYSCALL_IPC_CALL:
            status = syscall_ipc_call((ipc_call_params_t *)arg1);
            break;
        case SYSCALL_CAP_TRANSFER:
            status = syscall_cap_transfer((domain_id_t)arg1, (cap_id_t)arg2);
            break;
        case SYSCALL_CAP_DERIVE:
            status = syscall_cap_derive((cap_id_t)arg1, (cap_rights_t)arg2, (cap_id_t*)arg3);
            break;
        case SYSCALL_CAP_REVOKE:
            status = syscall_cap_revoke((cap_id_t)arg1);
            break;
        default:
            console_puts("[SYSCALL] Unknown syscall: ");
            console_putu64(syscall_num);
            console_puts("\n");
            status = HIK_ERROR_NOT_SUPPORTED;
            break;
    }
    
    /* 记录系统调用审计日志 */
    AUDIT_LOG_SYSCALL(current_domain, syscall_num, (status == HIK_SUCCESS));
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[SYSCALL] Invariant violation detected after syscall!\n");
    }
}
            break;
            
        case SYSCALL_CAP_TRANSFER:
            status = syscall_cap_transfer((domain_id_t)arg1, (cap_id_t)arg2);
            break;
            
        case SYSCALL_CAP_DERIVE:
            status = syscall_cap_derive((cap_id_t)arg1, (cap_rights_t)arg2, 
                                         (cap_id_t *)arg3);
            break;
            
        case SYSCALL_CAP_REVOKE:
            status = syscall_cap_revoke((cap_id_t)arg1);
            break;
            
        default:
            console_puts("[SYSCALL] Unknown syscall: ");
            console_putu64(syscall_num);
            console_puts("\n");
            status = HIK_ERROR_NOT_SUPPORTED;
            break;
    }
    
    /* 记录系统调用审计日志 */
    AUDIT_LOG_SYSCALL(current_domain, syscall_num, (status == HIK_SUCCESS));
    
    /* 返回值放在RAX寄存器中 */
    hal_set_syscall_return(status);
}
