/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/**
 * HIC Privileged-1服务管理头文件
 * 遵循三层模型文档第2.2节：Privileged-1层特权服务沙箱
 */

#ifndef HIC_KERNEL_PRIVILEGED_SERVICE_H
#define HIC_KERNEL_PRIVILEGED_SERVICE_H

#include "../Core-0/types.h"
#include "../Core-0/domain.h"
#include "../Core-0/capability.h"
#include "../Core-0/module_loader.h"

/* 服务类型 */
typedef enum {
    SERVICE_TYPE_DRIVER,       /* 硬件驱动 */
    SERVICE_TYPE_FS,           /* 文件系统 */
    SERVICE_TYPE_NETWORK,      /* 网络协议栈 */
    SERVICE_TYPE_DISPLAY,      /* 显示服务 */
    SERVICE_TYPE_AUDIO,        /* 音频服务 */
    SERVICE_TYPE_CRYPTO,       /* 加密服务 */
    SERVICE_TYPE_MONITOR,      /* 监控服务 */
    SERVICE_TYPE_AUDIT,        /* 审计服务 */
    SERVICE_TYPE_CUSTOM,       /* 自定义服务 */
} service_type_t;

/* 服务端点（API网关） */
typedef struct service_endpoint {
    cap_id_t        endpoint_cap;    /* 端点能力ID */
    char            name[64];        /* 端点名称 */
    virt_addr_t     handler_addr;    /* 处理函数地址 */
    u32             syscall_num;     /* 系统调用号 */
    
    /* 权限控制 */
    u32             max_message_size;
    u32             timeout_ms;
    
    /* 统计 */
    u64             call_count;
    u64             total_time_ns;
    
    struct service_endpoint *next;
} service_endpoint_t;

/* 服务实例 */
typedef struct privileged_service {
    /* 基本信息 */
    domain_id_t     domain_id;       /* 关联的域ID */
    service_type_t  type;            /* 服务类型 */
    char            name[64];        /* 服务名称 */
    char            uuid[37];        /* 服务UUID */
    
    /* 模块信息 */
    u64             instance_id;     /* 模块实例ID */
    hicmod_instance_t *module;   /* 模块实例指针 */
    
    /* 物理内存映射（直接映射） */
    phys_addr_t     code_base;       /* 代码段物理地址 */
    size_t          code_size;       /* 代码段大小 */
    phys_addr_t     data_base;       /* 数据段物理地址 */
    size_t          data_size;       /* 数据段大小 */
    phys_addr_t     stack_base;      /* 栈物理地址 */
    size_t          stack_size;      /* 栈大小 */
    
    /* 端点列表 */
    service_endpoint_t *endpoints;   /* 服务端点链表 */
    u32             endpoint_count;
    
    /* 资源配额 */
    domain_quota_t  quota;
    
    /* 中断处理 */
    u32             irq_vectors[8];  /* 服务处理的中断向量 */
    u32             irq_count;
    
    /* 设备访问 */
    phys_addr_t     mmio_regions[8]; /* MMIO区域 */
    size_t          mmio_sizes[8];
    u32             mmio_count;
    
    /* 状态 */
    domain_state_t  state;
    u32             restart_count;   /* 重启次数 */
    u64             start_time;      /* 启动时间 */
    u64             crash_count;     /* 崩溃次数 */
    
    /* 统计 */
    u64             cpu_time_total;
    u64             syscalls_total;
    
    /* 依赖关系 */
    domain_id_t     dependencies[8]; /* 依赖的服务域ID */
    u32             dep_count;
    
    /* 链表 */
    struct privileged_service *next;
    struct privileged_service *prev;
} privileged_service_t;

/* 服务管理器 */
typedef struct service_manager {
    privileged_service_t *service_list;  /* 服务链表 */
    u32                     service_count; /* 服务数量 */
    u32                     capacity;      /* 容量 */
    
    /* 端点路由表（静态路由） */
    service_endpoint_t     *irq_route_table[256]; /* 中断路由表 */
    service_endpoint_t     *syscall_route_table[256]; /* 系统调用路由表 */
    
    /* 统计 */
    u64                     total_services;
    u64                     running_services;
    u64                     crashed_services;
} service_manager_t;

/* ============================================================
 * 服务管理接口
 * ============================================================ */

/* 初始化服务管理器 */
void privileged_service_init(void);

/* 从模块加载服务 */
hic_status_t privileged_service_load(u64 module_instance_id, 
                                      const char *service_name,
                                      service_type_t type,
                                      const domain_quota_t *quota,
                                      domain_id_t *out_domain_id);

/* 启动服务 */
hic_status_t privileged_service_start(domain_id_t domain_id);

/* 停止服务 */
hic_status_t privileged_service_stop(domain_id_t domain_id);

/* 重启服务 */
hic_status_t privileged_service_restart(domain_id_t domain_id);

/* 卸载服务 */
hic_status_t privileged_service_unload(domain_id_t domain_id);

/* ============================================================
 * 端点管理接口
 * ============================================================ */

/* 注册服务端点 */
hic_status_t privileged_service_register_endpoint(domain_id_t domain_id,
                                                  const char *name,
                                                  virt_addr_t handler_addr,
                                                  u32 syscall_num,
                                                  cap_id_t *out_cap_id);

/* 注销服务端点 */
hic_status_t privileged_service_unregister_endpoint(domain_id_t domain_id,
                                                    cap_id_t endpoint_cap);

/* 查找端点 */
service_endpoint_t* privileged_service_find_endpoint(cap_id_t endpoint_cap);

/* 通过系统调用号查找端点 */
service_endpoint_t* privileged_service_find_endpoint_by_syscall(u32 syscall_num);

/* ============================================================
 * 中断处理接口
 * ============================================================ */

/* 注册中断处理函数 */
hic_status_t privileged_service_register_irq_handler(domain_id_t domain_id,
                                                     u32 irq_vector,
                                                     virt_addr_t handler_addr);

/* 注销中断处理函数 */
hic_status_t privileged_service_unregister_irq_handler(domain_id_t domain_id,
                                                       u32 irq_vector);

/* 分发中断到服务 */
void privileged_service_dispatch_irq(u32 irq_vector);

/* ============================================================
 * 设备访问接口
 * ============================================================ */

/* 分配MMIO区域 */
hic_status_t privileged_service_map_mmio(domain_id_t domain_id,
                                         phys_addr_t mmio_phys,
                                         size_t size,
                                         virt_addr_t *out_virt_addr);

/* 释放MMIO区域 */
hic_status_t privileged_service_unmap_mmio(domain_id_t domain_id,
                                           virt_addr_t virt_addr);

/* ============================================================
 * 查询接口
 * ============================================================ */

/* 获取服务信息 */
privileged_service_t* privileged_service_get_info(domain_id_t domain_id);

/* 通过名称查找服务 */
privileged_service_t* privileged_service_find_by_name(const char *name);

/* 通过UUID查找服务 */
privileged_service_t* privileged_service_find_by_uuid(const char *uuid);

/* 遍历所有服务 */
typedef void (*service_callback_t)(privileged_service_t *service, void *arg);
void privileged_service_foreach(service_callback_t callback, void *arg);

/* ============================================================
 * 资源管理接口
 * ============================================================ */

/* 检查内存配额 */
hic_status_t privileged_service_check_memory_quota(domain_id_t domain_id,
                                                   size_t size);

/* 检查CPU配额 */
hic_status_t privileged_service_check_cpu_quota(domain_id_t domain_id);

/* 更新服务统计 */
void privileged_service_update_stats(domain_id_t domain_id,
                                    u64 cpu_time_ns,
                                    u64 syscall_count);

/* ============================================================
 * 依赖管理接口
 * ============================================================ */

/* 添加依赖 */
hic_status_t privileged_service_add_dependency(domain_id_t domain_id,
                                               domain_id_t dep_domain_id);

/* 检查依赖是否满足 */
bool privileged_service_check_dependencies(domain_id_t domain_id);

/* 获取依赖列表 */
u32 privileged_service_get_dependencies(domain_id_t domain_id,
                                        domain_id_t *deps,
                                        u32 max_deps);

#endif /* HIC_KERNEL_PRIVILEGED_SERVICE_H */