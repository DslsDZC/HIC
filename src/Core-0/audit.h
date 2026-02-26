/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC审计日志系统
 * 遵循文档第3.3节：安全审计与防篡改日志
 */

#ifndef HIC_KERNEL_AUDIT_H
#define HIC_KERNEL_AUDIT_H

#include "types.h"
#include "capability.h"
#include "domain.h"

/* 审计日志事件类型 */
typedef enum {
    AUDIT_EVENT_CAP_VERIFY,       /* 能力验证 */
    AUDIT_EVENT_CAP_CREATE,       /* 能力创建 */
    AUDIT_EVENT_CAP_TRANSFER,     /* 能力传递 */
    AUDIT_EVENT_CAP_DERIVE,       /* 能力派生 */
    AUDIT_EVENT_CAP_REVOKE,       /* 能力撤销 */
    AUDIT_EVENT_DOMAIN_CREATE,    /* 域创建 */
    AUDIT_EVENT_DOMAIN_DESTROY,   /* 域销毁 */
    AUDIT_EVENT_DOMAIN_SUSPEND,   /* 域暂停 */
    AUDIT_EVENT_DOMAIN_RESUME,    /* 域恢复 */
    AUDIT_EVENT_THREAD_CREATE,    /* 线程创建 */
    AUDIT_EVENT_THREAD_DESTROY,   /* 线程销毁 */
    AUDIT_EVENT_THREAD_SWITCH,    /* 线程切换 */
    AUDIT_EVENT_SYSCALL,          /* 系统调用 */
    AUDIT_EVENT_IRQ,              /* 中断处理 */
    AUDIT_EVENT_IPC_CALL,         /* IPC调用 */
    AUDIT_EVENT_PRIVILEGED_CALL,  /* 特权调用 */
    AUDIT_EVENT_EXCEPTION,        /* 异常事件 */
    AUDIT_EVENT_SECURITY_VIOLATION, /* 安全违规 */
    AUDIT_EVENT_PMM_ALLOC,        /* 物理内存分配 */
    AUDIT_EVENT_PMM_FREE,         /* 物理内存释放 */
    AUDIT_EVENT_PAGETABLE_MAP,    /* 页表映射 */
    AUDIT_EVENT_PAGETABLE_UNMAP,  /* 页表解映射 */
    AUDIT_EVENT_SERVICE_CREATE,   /* 服务创建 */
    AUDIT_EVENT_SERVICE_START,    /* 服务启动 */
    AUDIT_EVENT_SERVICE_STOP,     /* 服务停止 */
    AUDIT_EVENT_SERVICE_DESTROY,  /* 服务销毁 */
    AUDIT_EVENT_SERVICE_CRASH,    /* 服务崩溃 */
    AUDIT_EVENT_MODULE_LOAD,      /* 模块加载 */
    AUDIT_EVENT_MODULE_UNLOAD,    /* 模块卸载 */
    AUDIT_EVENT_MONITOR_ACTION,   /* 监控操作 */
} audit_event_type_t;

/* 审计日志条目 */
typedef struct audit_entry {
    u64 timestamp;               /* 高精度时间戳 */
    u32 sequence;                /* 序列号 */
    audit_event_type_t type;     /* 事件类型 */
    domain_id_t domain;          /* 相关域ID */
    cap_id_t cap_id;             /* 相关能力ID */
    thread_id_t thread_id;       /* 相关线程ID */
    u64 data[4];                 /* 事件特定数据 */
    u8 result;                   /* 结果：0=失败，1=成功 */
    u8 reserved[3];
} audit_entry_t;

/* 审计日志缓冲区 */
typedef struct audit_buffer {
    void* base;                  /* 物理基地址 */
    size_t size;                 /* 缓冲区大小 */
    size_t write_offset;         /* 写入偏移 */
    u64 sequence;                /* 当前序列号 */
    bool initialized;            /* 是否已初始化 */
} audit_buffer_t;

/* 审计日志系统接口 */
void audit_system_init(void);
void audit_system_init_buffer(phys_addr_t base, size_t size);

/* 记录审计事件 */
void audit_log_event(audit_event_type_t type, domain_id_t domain, 
                     cap_id_t cap, thread_id_t thread_id,
                     u64 *data, u32 data_count, u8 result);

/* 简化的审计日志宏（用于系统调用） */
#define AUDIT_LOG_SYSCALL(domain, syscall_num, result) \
    do { \
        u64 _audit_data[4] = { (u64)(domain), (u64)(syscall_num), (u64)(result), 0 }; \
        audit_log_event(AUDIT_EVENT_SYSCALL, domain, 0, 0, 0, \
                       _audit_data, 4, (result) == HIC_SUCCESS ? 1 : 0); \
    } while(0)

/* 域切换审计日志 */
#define AUDIT_LOG_DOMAIN_SWITCH(from, to, cap) \
    do { \
        u64 _audit_data[4] = { (u64)(from), (u64)(to), (u64)(cap), 0 }; \
        audit_log_event(AUDIT_EVENT_IPC_CALL, from, cap, 0, \
                       _audit_data, 3, 1); \
    } while(0)

/* 便捷宏 */
#define AUDIT_LOG_CAP_VERIFY(domain, cap, result) \
    audit_log_event(AUDIT_EVENT_CAP_VERIFY, domain, cap, 0, NULL, 0, result)

#define AUDIT_LOG_CAP_CREATE(domain, cap, result) \
    audit_log_event(AUDIT_EVENT_CAP_CREATE, domain, cap, 0, NULL, 0, result)

#define AUDIT_LOG_CAP_TRANSFER(from, to, cap, result) \
    do { u64 data[2] = {from, to}; \
         audit_log_event(AUDIT_EVENT_CAP_TRANSFER, from, cap, 0, data, 2, result); \
    } while(0)

#define AUDIT_LOG_CAP_REVOKE(domain, cap, result) \
    audit_log_event(AUDIT_EVENT_CAP_REVOKE, domain, cap, 0, NULL, 0, result)

#define AUDIT_LOG_DOMAIN_CREATE(domain, result) \
    audit_log_event(AUDIT_EVENT_DOMAIN_CREATE, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_DOMAIN_DESTROY(domain, result) \
    audit_log_event(AUDIT_EVENT_DOMAIN_DESTROY, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_DOMAIN_SUSPEND(domain, result) \
    audit_log_event(AUDIT_EVENT_DOMAIN_SUSPEND, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_DOMAIN_RESUME(domain, result) \
    audit_log_event(AUDIT_EVENT_DOMAIN_RESUME, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_THREAD_CREATE(domain, thread, result) \
    audit_log_event(AUDIT_EVENT_THREAD_CREATE, domain, 0, thread, NULL, 0, result)

#define AUDIT_LOG_THREAD_DESTROY(domain, thread, result) \
    audit_log_event(AUDIT_EVENT_THREAD_DESTROY, domain, 0, thread, NULL, 0, result)

#define AUDIT_LOG_THREAD_SWITCH(from, to, thread) \
    do { u64 data[2] = {from, to}; \
         audit_log_event(AUDIT_EVENT_THREAD_SWITCH, 0, 0, thread, data, 2, true); \
    } while(0)

#define AUDIT_LOG_IRQ(vector, domain, result) \
    audit_log_event(AUDIT_EVENT_IRQ, domain, 0, 0, &vector, 1, result)

#define AUDIT_LOG_IPC_CALL(caller, cap, result) \
    audit_log_event(AUDIT_EVENT_IPC_CALL, caller, cap, 0, NULL, 0, result)

#define AUDIT_LOG_EXCEPTION(domain, exc_type, result) \
    do { u64 _exc = exc_type; \
         audit_log_event(AUDIT_EVENT_EXCEPTION, domain, 0, 0, &_exc, 1, result); \
    } while(0)

#define AUDIT_LOG_SECURITY_VIOLATION(domain, reason) \
    do { u64 _reason = reason; \
         audit_log_event(AUDIT_EVENT_SECURITY_VIOLATION, domain, 0, 0, &_reason, 1, false); \
    } while(0)

#define AUDIT_LOG_PMM_ALLOC(domain, addr, count, result) \
    do { u64 data[2] = {addr, count}; \
         audit_log_event(AUDIT_EVENT_PMM_ALLOC, domain, 0, 0, data, 2, result); \
    } while(0)

#define AUDIT_LOG_PMM_FREE(domain, addr, count, result) \
    do { u64 data[2] = {addr, count}; \
         audit_log_event(AUDIT_EVENT_PMM_FREE, domain, 0, 0, data, 2, result); \
    } while(0)

#define AUDIT_LOG_PAGETABLE_MAP(domain, virt, phys, result) \
    do { u64 data[2] = {virt, phys}; \
         audit_log_event(AUDIT_EVENT_PAGETABLE_MAP, domain, 0, 0, data, 2, result); \
    } while(0)

#define AUDIT_LOG_PAGETABLE_UNMAP(domain, addr, result) \
    audit_log_event(AUDIT_EVENT_PAGETABLE_UNMAP, domain, 0, 0, &addr, 1, result)

#define AUDIT_LOG_SERVICE_START(domain, result) \
    audit_log_event(AUDIT_EVENT_SERVICE_START, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_SERVICE_STOP(domain, result) \
    audit_log_event(AUDIT_EVENT_SERVICE_STOP, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_SERVICE_CRASH(domain, reason) \
    audit_log_event(AUDIT_EVENT_SERVICE_CRASH, domain, 0, 0, &reason, 1, false)

#define AUDIT_LOG_MODULE_LOAD(domain, instance_id, result) \
    audit_log_event(AUDIT_EVENT_MODULE_LOAD, domain, 0, 0, &instance_id, 1, result)

#define AUDIT_LOG_MODULE_UNLOAD(domain, instance_id, result) \
    audit_log_event(AUDIT_EVENT_MODULE_UNLOAD, domain, 0, 0, &instance_id, 1, result)

#define AUDIT_LOG_MONITOR_ACTION(action, domain, result) \
    audit_log_event(AUDIT_EVENT_MONITOR_ACTION, domain, 0, 0, &action, 1, result)

/* 获取统计信息 */
u64 audit_get_entry_count(void);
u64 audit_get_buffer_usage(void);

#endif /* HIC_KERNEL_AUDIT_H */
