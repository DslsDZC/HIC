/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/**
 * HIK Privileged-1服务管理实现
 * 遵循三层模型文档第2.2节：Privileged-1层特权服务沙箱
 */

#include "privileged_service.h"
#include "../Core-0/domain.h"
#include "../Core-0/capability.h"
#include "../Core-0/module_loader.h"
#include "../Core-0/monitor.h"
#include "../Core-0/domain_switch.h"
#include "../Core-0/audit.h"
#include "../Core-0/pagetable.h"
#include "../Core-0/pmm.h"
#include "../Core-0/lib/console.h"
#include "../Core-0/lib/string.h"
#include "../Core-0/lib/mem.h"

#define MAX_SERVICES  256

/* 字符串比较辅助函数 */
static int strcompare(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}



/* 全局服务管理器 */
static service_manager_t g_service_manager;

/* 初始化服务管理器 */
void privileged_service_init(void)
{
    memzero(&g_service_manager, sizeof(service_manager_t));
    g_service_manager.capacity = MAX_SERVICES;

    console_puts("[PRIV-SVC] Service manager initialized\n");
    console_puts("[PRIV-SVC] Maximum services: ");
    console_putu32(MAX_SERVICES);
    console_puts("\n");
}

/* 从模块加载服务 */
hik_status_t privileged_service_load(u64 module_instance_id,
                                      const char *service_name,
                                      service_type_t type,
                                      const domain_quota_t *quota,
                                      domain_id_t *out_domain_id)
{
    if (!service_name || !quota || !out_domain_id) {
        return HIK_ERROR_INVALID_PARAM;
    }

    /* 查找模块实例 */
    hikmod_instance_t *module = module_get_instance(module_instance_id);
    if (!module) {
        console_puts("[PRIV-SVC] Module instance not found: ");
        console_putu32(module_instance_id);
        console_puts("\n");
        return HIK_ERROR_NOT_FOUND;
    }
    
    /* 检查服务数量限制 */
    if (g_service_manager.service_count >= MAX_SERVICES) {
        console_puts("[PRIV-SVC] Maximum services reached\n");
        return HIK_ERROR_NO_MEMORY;
    }
    
    /* 创建域 */
    domain_id_t domain_id;
    hik_status_t status = domain_create(DOMAIN_TYPE_PRIVILEGED, 0, quota, &domain_id);
    if (status != HIK_SUCCESS) {
        console_puts("[PRIV-SVC] Failed to create domain\n");
        return status;
    }

    /* 分配服务结构 */
    phys_addr_t service_phys;
    status = pmm_alloc_frames(domain_id, 1, PAGE_FRAME_PRIVILEGED, &service_phys);
    if (status != HIK_SUCCESS) {
        domain_destroy(domain_id);
        return HIK_ERROR_NO_MEMORY;
    }

    privileged_service_t *service = (privileged_service_t *)(virt_addr_t)service_phys;

    if (!service) {
        domain_destroy(domain_id);
        return HIK_ERROR_NO_MEMORY;
    }

    memzero(service, sizeof(privileged_service_t));
    
    /* 初始化服务信息 */
    service->domain_id = domain_id;
    service->type = type;
    strncpy(service->name, service_name, sizeof(service->name) - 1);
    service->name[sizeof(service->name) - 1] = '\0';

    /* 生成UUID（简化版：使用模块实例ID） */
    for (int i = 0; i < 16; i++) {
        service->uuid[i] = (u8)(module_instance_id >> (i * 8));
    }

    /* 模块信息 */
    service->instance_id = module_instance_id;
    service->module = module;

    /* 物理内存映射（直接映射） */
    service->code_base = module->code_base;
    
    /* 从模块头获取代码段大小 */
    hikmod_header_t* module_header = (hikmod_header_t*)module->code_base;
    if (module_header->magic == HIKMOD_MAGIC) {
        service->code_size = module_header->code_size;
    } else {
        service->code_size = 4096; /* 默认4KB */
    }
    
    service->data_base = module->data_base;
    
    /* 从模块头获取数据段大小 */
    if (module_header->magic == HIKMOD_MAGIC) {
        service->data_size = module_header->data_size;
    } else {
        service->data_size = 4096; /* 默认4KB */
    }
    
    /* 分配栈空间 */
    size_t stack_size = 64 * 1024; /* 64KB栈 */
    u32 stack_pages = (stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
    phys_addr_t stack_phys;
    status = pmm_alloc_frames(domain_id, stack_pages, PAGE_FRAME_PRIVILEGED, &stack_phys);
    if (status != HIK_SUCCESS) {
        pmm_free_frames(service_phys, 1);
        domain_destroy(domain_id);
        return HIK_ERROR_NO_MEMORY;
    }

    service->stack_base = stack_phys;
    service->stack_size = stack_size;
    
    /* 配额 */
    memcopy(&service->quota, quota, sizeof(domain_quota_t));
    
    /* 状态 */
    service->state = DOMAIN_STATE_INIT;
    service->start_time = hal_get_timestamp();
    
    /* 添加到链表 */
    service->next = g_service_manager.service_list;
    if (g_service_manager.service_list) {
        g_service_manager.service_list->prev = service;
    }
    g_service_manager.service_list = service;
    
    g_service_manager.service_count++;
    g_service_manager.total_services++;
    
    /* 记录审计事件 */
    u64 audit_data[4] = {
        domain_id, module_instance_id, type, quota->max_memory
    };
    audit_log_event(AUDIT_EVENT_SERVICE_CREATE, 0, 0, 0,
                   audit_data, 4, true);
    
    console_puts("[PRIV-SVC] Service loaded: ");
    console_puts(service_name);
    console_puts(" (domain: ");
    console_putu32(domain_id);
    console_puts(")\n");
    
    *out_domain_id = domain_id;
    return HIK_SUCCESS;
}

/* 启动服务 */
hik_status_t privileged_service_start(domain_id_t domain_id)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    if (service->state == DOMAIN_STATE_RUNNING) {
        return HIK_ERROR_ALREADY_EXISTS;
    }
    
    /* 检查依赖 */
    if (!privileged_service_check_dependencies(domain_id)) {
        console_puts("[PRIV-SVC] Service dependencies not satisfied\n");
        return HIK_ERROR_INVALID_STATE;
    }
    
    /* 通知监控服务 */
    monitor_event_t event;
    event.type = MONITOR_EVENT_SERVICE_START;
    event.domain = domain_id;
    event.timestamp = hal_get_timestamp();
    monitor_report_event(&event);
    
    /* 更新状态 */
    service->state = DOMAIN_STATE_RUNNING;
    
    /* 记录审计事件 */
    audit_log_event(AUDIT_EVENT_SERVICE_START, domain_id, 0, 0, NULL, 0, true);
    
    console_puts("[PRIV-SVC] Service started: ");
    console_puts(service->name);
    console_puts("\n");
    
    g_service_manager.running_services++;
    
    return HIK_SUCCESS;
}

/* 停止服务 */
hik_status_t privileged_service_stop(domain_id_t domain_id)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    if (service->state != DOMAIN_STATE_RUNNING) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    /* 通知监控服务 */
    monitor_event_t event;
    event.type = MONITOR_EVENT_SERVICE_STOP;
    event.domain = domain_id;
    event.timestamp = hal_get_timestamp();
    monitor_report_event(&event);
    
    /* 更新状态 */
    service->state = DOMAIN_STATE_SUSPENDED;
    
    /* 记录审计事件 */
    audit_log_event(AUDIT_EVENT_SERVICE_STOP, domain_id, 0, 0, NULL, 0, true);
    
    console_puts("[PRIV-SVC] Service stopped: ");
    console_puts(service->name);
    console_puts("\n");
    
    g_service_manager.running_services--;
    
    return HIK_SUCCESS;
}

/* 重启服务 */
hik_status_t privileged_service_restart(domain_id_t domain_id)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    console_puts("[PRIV-SVC] Restarting service: ");
    console_puts(service->name);
    console_puts("\n");
    
    /* 停止服务 */
    hik_status_t status = privileged_service_stop(domain_id);
    if (status != HIK_SUCCESS && status != HIK_ERROR_INVALID_STATE) {
        return status;
    }
    
    /* 重新启动 */
    status = privileged_service_start(domain_id);
    if (status == HIK_SUCCESS) {
        service->restart_count++;
    }
    
    return status;
}

/* 卸载服务 */
hik_status_t privileged_service_unload(domain_id_t domain_id)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    /* 停止服务 */
    if (service->state == DOMAIN_STATE_RUNNING) {
        privileged_service_stop(domain_id);
    }
    
    /* 释放栈空间 */
    if (service->stack_base != 0) {
        u32 stack_pages = (service->stack_size + PAGE_SIZE - 1) / PAGE_SIZE;
        pmm_free_frames(service->stack_base, stack_pages);
    }

    /* 销毁域 */
    domain_destroy(domain_id);

    /* 从链表移除 */
    if (service->prev) {
        service->prev->next = service->next;
    } else {
        g_service_manager.service_list = service->next;
    }

    if (service->next) {
        service->next->prev = service->prev;
    }

    /* 释放服务结构 */
    pmm_free_frames((phys_addr_t)service, 1);
    
    g_service_manager.service_count--;

    /* 记录审计事件 */
    audit_log_event(AUDIT_EVENT_SERVICE_DESTROY, domain_id, 0, 0, NULL, 0, true);

    console_puts("[PRIV-SVC] Service unloaded: domain ");
    console_putu32(domain_id);
    console_puts("\n");

    return HIK_SUCCESS;
}

/* 注册服务端点 */
hik_status_t privileged_service_register_endpoint(domain_id_t domain_id,
                                                  const char *name,
                                                  virt_addr_t handler_addr,
                                                  u32 syscall_num,
                                                  cap_id_t *out_cap_id)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service || !name || !out_cap_id) {
        return HIK_ERROR_INVALID_PARAM;
    }

    /* 分配端点结构 */
    phys_addr_t endpoint_phys;
    hik_status_t status = pmm_alloc_frames(domain_id, 1, PAGE_FRAME_PRIVILEGED, &endpoint_phys);
    if (status != HIK_SUCCESS) {
        return HIK_ERROR_NO_MEMORY;
    }

    service_endpoint_t *endpoint = (service_endpoint_t *)(virt_addr_t)endpoint_phys;

    if (!endpoint) {
        return HIK_ERROR_NO_MEMORY;
    }

    memzero(endpoint, sizeof(service_endpoint_t));

    /* 初始化端点 */
    strncpy(endpoint->name, name, sizeof(endpoint->name) - 1);
    endpoint->name[sizeof(endpoint->name) - 1] = '\0';
    endpoint->handler_addr = handler_addr;
    endpoint->syscall_num = syscall_num;
    endpoint->max_message_size = 4096;
    endpoint->timeout_ms = 5000;

    /* 避免未使用参数警告 */
    (void)handler_addr;
    (void)syscall_num;
    
    /* 创建端点能力 - 使用 endpoint 类型的能力 */
    cap_id_t cap_id;
    status = cap_create_memory(domain_id, handler_addr, 4096, CAP_MEM_READ | CAP_MEM_EXEC, &cap_id);
    if (status != HIK_SUCCESS) {
        pmm_free_frames((phys_addr_t)endpoint, 1);
        return status;
    }
    
    endpoint->endpoint_cap = cap_id;
    
    /* 添加到端点链表 */
    endpoint->next = service->endpoints;
    service->endpoints = endpoint;
    service->endpoint_count++;
    
    /* 添加到路由表 */
    if (syscall_num < 256) {
        g_service_manager.syscall_route_table[syscall_num] = endpoint;
    }
    
    console_puts("[PRIV-SVC] Endpoint registered: ");
    console_puts(name);
    console_puts(" (syscall: ");
    console_putu32(syscall_num);
    console_puts(")\n");
    
    *out_cap_id = cap_id;
    return HIK_SUCCESS;
}

/* 查找端点 */
service_endpoint_t* privileged_service_find_endpoint(cap_id_t endpoint_cap)
{
    privileged_service_t *service = g_service_manager.service_list;
    
    while (service) {
        service_endpoint_t *endpoint = service->endpoints;
        
        while (endpoint) {
            if (endpoint->endpoint_cap == endpoint_cap) {
                return endpoint;
            }
            endpoint = endpoint->next;
        }
        
        service = service->next;
    }
    
    return NULL;
}

/* 通过系统调用号查找端点 */
service_endpoint_t* privileged_service_find_endpoint_by_syscall(u32 syscall_num)
{
    if (syscall_num < 256) {
        return g_service_manager.syscall_route_table[syscall_num];
    }
    return NULL;
}

/* 注册中断处理函数 */
hik_status_t privileged_service_register_irq_handler(domain_id_t domain_id,
                                                     u32 irq_vector,
                                                     virt_addr_t handler_addr)
{
    /* 避免未使用参数警告 */
    (void)handler_addr;

    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    if (service->irq_count >= 8) {
        return HIK_ERROR_NO_MEMORY;
    }
    
    service->irq_vectors[service->irq_count] = irq_vector;
    service->irq_count++;
    
    console_puts("[PRIV-SVC] IRQ handler registered: IRQ ");
    console_putu32(irq_vector);
    console_puts(" -> service ");
    console_puts(service->name);
    console_puts("\n");
    
    return HIK_SUCCESS;
}

/* 分发中断到服务 */
void privileged_service_dispatch_irq(u32 irq_vector)
{
    /* 查找处理该中断的服务 */
    privileged_service_t *service = g_service_manager.service_list;
    
    while (service) {
        for (u32 i = 0; i < service->irq_count; i++) {
            if (service->irq_vectors[i] == irq_vector) {
                /* 找到处理服务，中断处理逻辑由服务自己实现 */
                /* 实现中断处理调用机制 */
                
                /* 保存当前上下文 */
                hal_context_t old_context;
                domain_switch_save_context(HIK_DOMAIN_CORE, &old_context);
                
                /* 切换到服务上下文 */
                domain_switch(HIK_DOMAIN_CORE, service->domain_id, 0, 0, 0, 0);
                
                /* 调用服务的中断处理函数 */
                /* 服务应该通过系统调用注册中断处理函数 */
                /* 这里简化处理，直接返回 */
                
                /* 恢复上下文 */
                domain_switch_restore_context(HIK_DOMAIN_CORE, &old_context);
                
                return;
            }
        }
        service = service->next;
    }
}

/* 分配MMIO区域 */
hik_status_t privileged_service_map_mmio(domain_id_t domain_id,
                                         phys_addr_t mmio_phys,
                                         size_t size,
                                         virt_addr_t *out_virt_addr)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service || !out_virt_addr) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    if (service->mmio_count >= 8) {
        return HIK_ERROR_NO_MEMORY;
    }
    
    /* 记录MMIO区域 */
    service->mmio_regions[service->mmio_count] = mmio_phys;
    service->mmio_sizes[service->mmio_count] = size;
    service->mmio_count++;
    
    /* 使用直接映射（虚拟地址 = 物理地址） */
    *out_virt_addr = mmio_phys;
    
    console_puts("[PRIV-SVC] MMIO mapped: 0x");
    console_puthex64(mmio_phys);
    console_puts(" (size: ");
    console_putu32(size);
    console_puts(")\n");
    
    return HIK_SUCCESS;
}

/* 获取服务信息 */
privileged_service_t* privileged_service_get_info(domain_id_t domain_id)
{
    privileged_service_t *service = g_service_manager.service_list;
    
    while (service) {
        if (service->domain_id == domain_id) {
            return service;
        }
        service = service->next;
    }
    
    return NULL;
}

/* 通过名称查找服务 */
privileged_service_t* privileged_service_find_by_name(const char *name)
{
    privileged_service_t *service = g_service_manager.service_list;
    
    while (service) {
        if (strcompare(service->name, name) == 0) {
            return service;
        }
        service = service->next;
    }
    
    return NULL;
}

/* 遍历所有服务 */
void privileged_service_foreach(service_callback_t callback, void *arg)
{
    privileged_service_t *service = g_service_manager.service_list;
    
    while (service) {
        callback(service, arg);
        service = service->next;
    }
}

/* 检查内存配额 */
hik_status_t privileged_service_check_memory_quota(domain_id_t domain_id,
                                                   size_t size)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return HIK_ERROR_NOT_FOUND;
    }

    domain_t *domain = NULL;
    hik_status_t status = domain_get_info(domain_id, domain);
    if (status != HIK_SUCCESS) {
        return status;
    }
    
    if (domain->usage.memory_used + size > domain->quota.max_memory) {
        return HIK_ERROR_NO_MEMORY;
    }
    
    return HIK_SUCCESS;
}

/* 添加依赖 */
hik_status_t privileged_service_add_dependency(domain_id_t domain_id,
                                               domain_id_t dep_domain_id)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    if (service->dep_count >= 8) {
        return HIK_ERROR_NO_MEMORY;
    }
    
    service->dependencies[service->dep_count] = dep_domain_id;
    service->dep_count++;
    
    return HIK_SUCCESS;
}

/* 检查依赖是否满足 */
bool privileged_service_check_dependencies(domain_id_t domain_id)
{
    privileged_service_t *service = privileged_service_get_info(domain_id);
    if (!service) {
        return false;
    }
    
    for (u32 i = 0; i < service->dep_count; i++) {
        privileged_service_t *dep = privileged_service_get_info(
            service->dependencies[i]);
        
        if (!dep || dep->state != DOMAIN_STATE_RUNNING) {
            return false;
        }
    }
    
    return true;
}


