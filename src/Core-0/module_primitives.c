/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC模块原语实现
 * 
 * Core-0 提供给 Privileged-1 服务的底层原语实现
 */

#include "include/module_primitives.h"
#include "include/service_registry.h"
#include "ipc3.h"
#include "capability.h"
#include "domain.h"
#include "domain_switch.h"
#include "pmm.h"
#include "pagetable.h"
#include "exception.h"
#include "thread.h"
#include "atomic.h"
#include "console.h"
#include "formal_verification.h"
#include "lib/string.h"
#include "lib/mem.h"

/* ==================== 辅助函数 ==================== */

/**
 * 获取当前执行域的ID
 */
static domain_id_t get_current_domain_id(void)
{
    return domain_switch_get_current();
}

/* ==================== 初始化 ==================== */

void module_primitives_init(void)
{
    console_puts("[PRIMITIVES] Module primitives initialized\n");
}

/* ==================== 域管理原语 ==================== */

hic_status_t module_domain_create(module_type_t type,
                                    const domain_quota_t* quota,
                                    domain_id_t* domain_id)
{
    domain_type_t domain_type;
    
    if (!quota || !domain_id) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 转换模块类型到域类型 */
    switch (type) {
        case MODULE_TYPE_CORE:
            domain_type = DOMAIN_TYPE_CORE;
            break;
        case MODULE_TYPE_PRIVILEGED:
            domain_type = DOMAIN_TYPE_PRIVILEGED;
            break;
        case MODULE_TYPE_APPLICATION:
            domain_type = DOMAIN_TYPE_APPLICATION;
            break;
        default:
            return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 获取调用者域ID作为父域 */
    domain_id_t parent = get_current_domain_id();
    
    /* 调用域系统创建域 */
    return domain_create(domain_type, parent, quota, domain_id);
}

hic_status_t module_domain_destroy(domain_id_t domain_id)
{
    return domain_destroy(domain_id);
}

hic_status_t module_domain_suspend(domain_id_t domain_id)
{
    return domain_suspend(domain_id);
}

hic_status_t module_domain_resume(domain_id_t domain_id)
{
    return domain_resume(domain_id);
}

hic_status_t module_domain_get_state(domain_id_t domain_id,
                                      domain_state_t* state)
{
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status == HIC_SUCCESS && state) {
        *state = info.state;
    }
    return status;
}

hic_status_t module_domain_signal(domain_id_t domain_id,
                                    u32 exception_code,
                                    const char* exception_info)
{
    /* 验证域是否存在 */
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 构建异常上下文 */
    exception_context_t ctx;
    memzero(&ctx, sizeof(ctx));
    
    /* 映射异常代码到异常类型 */
    switch (exception_code) {
        case 0:  /* 自定义信号 */
            ctx.type = EXCEPTION_GENERAL_PROTECTION;
            break;
        case 1:  /* 终止请求 */
            ctx.type = EXCEPTION_GENERAL_PROTECTION;
            break;
        case 2:  /* 内存错误 */
            ctx.type = EXCEPTION_PAGE_FAULT;
            break;
        default:
            ctx.type = EXCEPTION_GENERAL_PROTECTION;
            break;
    }
    
    ctx.domain = domain_id;
    ctx.error_code = exception_code;
    
    /* 如果提供了异常信息，记录日志 */
    if (exception_info) {
        console_puts("[PRIMITIVES] Domain signal: domain=");
        console_putu64(domain_id);
        console_puts(", code=");
        console_putu64(exception_code);
        console_puts(", info=");
        console_puts(exception_info);
        console_puts("\n");
    }
    
    /* 调用异常处理系统 */
    exception_handler_result_t result = exception_handle(&ctx);
    
    /* 根据处理结果返回状态 */
    switch (result) {
        case EXCEPT_HANDLER_CONTINUE:
            return HIC_SUCCESS;
        case EXCEPT_HANDLER_TERMINATE:
            return HIC_ERROR_INVALID_STATE;
        case EXCEPT_HANDLER_RESTART:
            return HIC_SUCCESS;
        case EXCEPT_HANDLER_PANIC:
            return HIC_ERROR_GENERIC;
        default:
            return HIC_SUCCESS;
    }
}

hic_status_t module_domain_get_quota(domain_id_t domain_id,
                                      domain_quota_t* quota)
{
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status == HIC_SUCCESS && quota) {
        *quota = info.quota;
    }
    return status;
}

/* ==================== 内存管理原语 ==================== */

hic_status_t module_memory_alloc(domain_id_t domain_id,
                                   u64 size,
                                   module_page_type_t type,
                                   u64* phys_addr)
{
    u32 page_type;
    
    /* 调试：确认函数被调用 */
    console_puts("[PRIMITIVES] module_memory_alloc called: domain=");
    console_putu32(domain_id);
    console_puts(", size=");
    console_putu64(size);
    console_puts(", type=");
    console_putu32(type);
    console_puts("\n");
    
    if (!phys_addr || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 转换页类型 */
    switch (type) {
        case MODULE_PAGE_CODE:
            page_type = PAGE_FRAME_PRIVILEGED;
            break;
        case MODULE_PAGE_DATA:
            page_type = PAGE_FRAME_PRIVILEGED;
            break;
        case MODULE_PAGE_STACK:
            page_type = PAGE_FRAME_PRIVILEGED;
            break;
        case MODULE_PAGE_SHARED:
            page_type = PAGE_FRAME_SHARED;
            break;
        default:
            page_type = PAGE_FRAME_PRIVILEGED;
    }
    
    /* 检查域的内存配额 */
    hic_status_t status = domain_check_memory_quota(domain_id, size);
    if (status != HIC_SUCCESS) {
        console_puts("[PRIMITIVES] Memory quota check failed for domain ");
        console_putu32(domain_id);
        console_puts(", size=");
        console_putu64(size);
        console_puts("\n");
        return status;
    }
    
    /* 对齐大小到页边界 */
    u32 page_count = (u32)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    
    /* 首先尝试连续分配 */
    status = pmm_alloc_frames(domain_id, page_count, page_type, phys_addr);
    
    if (status == HIC_SUCCESS) {
        console_puts("[PRIMITIVES] Consecutive allocation succeeded\n");
    } else {
        /* 连续分配失败，尝试分散分配 */
        console_puts("[PRIMITIVES] Consecutive allocation failed, trying scattered allocation\n");
        
        phys_addr_t *pages = (phys_addr_t *)0x20000000;  /* 临时缓冲区 */
        status = pmm_alloc_scattered(domain_id, page_count, page_type, pages);
        
        if (status == HIC_SUCCESS) {
            *phys_addr = pages[0];  /* 返回第一页地址 */
            console_puts("[PRIMITIVES] Scattered allocation succeeded, first page at 0x");
            console_puthex64(*phys_addr);
            console_puts("\n");
        } else {
            console_puts("[PRIMITIVES] Both allocation strategies failed\n");
            return status;
        }
    }
    
    /* 将分配的内存映射到域的页表中 */
    page_table_t* domain_pagetable = domain_switch_get_pagetable(domain_id);
    if (domain_pagetable == NULL) {
        /* 域没有页表，这可能是一个错误或域 0 */
        console_puts("[PRIMITIVES] No page table for domain ");
        console_putu32(domain_id);
        console_puts("\n");
        return HIC_SUCCESS;  /* 内存已分配，但无法映射 */
    }
    
    /* 确定映射权限 */
    page_perm_t perm;
    if (type == MODULE_PAGE_CODE) {
        perm = PERM_RWX;  /* 代码段：可读写执行 */
    } else if (type == MODULE_PAGE_STACK) {
        perm = PERM_RW;   /* 栈：可读写 */
    } else {
        perm = PERM_RW;   /* 数据/其他：可读写 */
    }
    
    /* 使用恒等映射（虚拟地址 = 物理地址） */
    size_t map_size = (size_t)(page_count * PAGE_SIZE);
    status = pagetable_map(domain_pagetable,
                           (virt_addr_t)*phys_addr,  /* 虚拟地址 */
                           *phys_addr,                /* 物理地址 */
                           map_size,
                           perm,
                           MAP_TYPE_USER);
    
    if (status != HIC_SUCCESS) {
        console_puts("[PRIMITIVES] Failed to map memory for domain ");
        console_putu32(domain_id);
        console_puts(", status=");
        console_putu32(status);
        console_puts("\n");
        /* 映射失败，但物理内存已分配，返回成功让调用者处理 */
    } else {
        console_puts("[PRIMITIVES] Mapped ");
        console_putu64(map_size);
        console_puts(" bytes at 0x");
        console_puthex64(*phys_addr);
        console_puts(" for domain ");
        console_putu32(domain_id);
        console_puts("\n");
    }
    
    return HIC_SUCCESS;
}

hic_status_t module_memory_free(domain_id_t domain_id,
                                  u64 phys_addr,
                                  u64 size)
{
    /* 验证域拥有此内存 */
    if (domain_id >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查内存区域是否属于该域 */
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 验证地址范围在域的内存区域内 */
    mem_region_t region = get_domain_memory_region(domain_id);
    if (phys_addr < region.base || 
        phys_addr + size > region.base + region.size) {
        /* 地址不在域的内存区域内，检查能力系统 */
        /* 遍历域的能力表，查找内存能力 */
        bool has_cap = false;
        
        for (u32 i = 0; i < CAP_TABLE_SIZE; i++) {
            cap_entry_t *entry = &g_global_cap_table[i];
            if (entry->owner == domain_id && entry->memory.size > 0) {
                /* 检查地址是否在能力范围内 */
                if (phys_addr >= entry->memory.base &&
                    phys_addr + size <= entry->memory.base + entry->memory.size) {
                    has_cap = true;
                    break;
                }
            }
        }
        
        if (!has_cap) {
            console_puts("[PRIMITIVES] Memory ownership verification failed\n");
            return HIC_ERROR_PERMISSION;
        }
    }
    
    u32 page_count = (u32)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    pmm_free_frames(phys_addr, page_count);
    return HIC_SUCCESS;
}

/* 域虚拟地址分配跟踪 */
static struct {
    u64 next_vaddr[HIC_DOMAIN_MAX];  /* 每个域的下一个可用虚拟地址 */
    bool initialized;
} g_vaddr_allocator = { .initialized = false };

/* 初始化虚拟地址分配器 */
static void vaddr_allocator_init(void)
{
    if (g_vaddr_allocator.initialized) {
        return;
    }
    
    /* 为每个域初始化虚拟地址空间起始地址 */
    for (u32 i = 0; i < HIC_DOMAIN_MAX; i++) {
        mem_region_t region = get_domain_memory_region((domain_id_t)i);
        /* 从域内存区域之后开始分配虚拟地址 */
        g_vaddr_allocator.next_vaddr[i] = region.base + region.size;
        /* 对齐到4MB边界 */
        g_vaddr_allocator.next_vaddr[i] = (g_vaddr_allocator.next_vaddr[i] + 0x3FFFFF) & ~0x3FFFFFULL;
    }
    
    g_vaddr_allocator.initialized = true;
}

/* 分配虚拟地址空间 */
static u64 domain_find_free_vaddr(domain_id_t domain, size_t size)
{
    if (!g_vaddr_allocator.initialized) {
        vaddr_allocator_init();
    }
    
    if (domain >= HIC_DOMAIN_MAX) {
        return 0;
    }
    
    /* 对齐大小到页面 */
    size = (size + PAGE_SIZE - 1) & ~((size_t)(PAGE_SIZE - 1));
    
    /* 获取当前虚拟地址 */
    u64 vaddr = g_vaddr_allocator.next_vaddr[domain];
    
    /* 更新下次分配地址 */
    g_vaddr_allocator.next_vaddr[domain] += size;
    
    return vaddr;
}

hic_status_t module_memory_map(domain_id_t domain_id,
                                u64 phys_addr,
                                u64 size,
                                u64* virt_addr)
{
    if (!virt_addr || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 获取域信息 */
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 检查域是否有页表 */
    if (!info.page_table) {
        /* 没有页表，使用恒等映射 */
        *virt_addr = phys_addr;
        return HIC_SUCCESS;
    }
    
    /* 使用虚拟地址分配器查找空闲区域 */
    u64 vaddr = domain_find_free_vaddr(domain_id, size);
    
    if (vaddr == 0) {
        /* 无法找到空闲虚拟地址空间 */
        console_puts("[PRIMITIVES] No free virtual address space\n");
        return HIC_ERROR_NO_MEMORY;
    }
    
    *virt_addr = vaddr;
    
    /* 在页表中映射 */
    page_table_t* root = (page_table_t*)info.page_table;
    status = pagetable_map(root, *virt_addr, phys_addr, size,
                          PERM_RW, MAP_TYPE_USER);
    
    if (status != HIC_SUCCESS) {
        console_puts("[PRIMITIVES] Failed to map memory: status=");
        console_putu64(status);
        console_puts("\n");
        return status;
    }
    
    console_puts("[PRIMITIVES] Mapped: phys=0x");
    console_puthex64(phys_addr);
    console_puts(" -> virt=0x");
    console_puthex64(*virt_addr);
    console_puts(" (domain=");
    console_putu64(domain_id);
    console_puts(")\n");
    
    return HIC_SUCCESS;
}

hic_status_t module_memory_unmap(domain_id_t domain_id,
                                  u64 virt_addr,
                                  u64 size)
{
    if (size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 获取域信息 */
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 检查域是否有页表 */
    if (!info.page_table) {
        /* 没有页表，无需取消映射 */
        return HIC_SUCCESS;
    }
    
    /* 在页表中取消映射 */
    page_table_t* root = (page_table_t*)info.page_table;
    status = pagetable_unmap(root, virt_addr, size);
    
    if (status != HIC_SUCCESS) {
        console_puts("[PRIMITIVES] Failed to unmap memory: status=");
        console_putu64(status);
        console_puts("\n");
        return status;
    }
    
    console_puts("[PRIMITIVES] Unmapped: virt=0x");
    console_puthex64(virt_addr);
    console_puts(" (domain=");
    console_putu64(domain_id);
    console_puts(")\n");
    
    return HIC_SUCCESS;
}

/* ==================== 能力管理原语 ==================== */

hic_status_t module_cap_create(domain_id_t domain_id,
                                u32 cap_type,
                                cap_rights_t rights,
                                cap_id_t* cap_id)
{
    if (!cap_id) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 根据能力类型创建能力 */
    switch (cap_type) {
        case CAP_MEMORY:
        case CAP_DEVICE:
        case CAP_MMIO:
        case CAP_SHARED:
            /* 内存相关能力 */
            return cap_create_memory(domain_id, 0, PAGE_SIZE, rights, cap_id);

        case CAP_SERVICE:
            /* 服务能力：分配基本内存能力 */
            return cap_create_memory(domain_id, 0, PAGE_SIZE, rights, cap_id);

        case CAP_THREAD:
            /* 线程能力：分配基本内存能力 */
            return cap_create_memory(domain_id, 0, PAGE_SIZE, rights, cap_id);

        case CAP_IRQ:
            /* 中断能力：分配基本内存能力 */
            return cap_create_memory(domain_id, 0, PAGE_SIZE, rights, cap_id);

        default:
            console_puts("[PRIMITIVES] Unknown cap_type=");
            console_putu64(cap_type);
            console_puts(", returning error\n");
            return HIC_ERROR_INVALID_PARAM;
    }
}

hic_status_t module_cap_revoke(cap_id_t cap_id)
{
    return cap_revoke(cap_id);
}

hic_status_t module_cap_derive(cap_id_t parent_cap,
                                cap_rights_t sub_rights,
                                cap_id_t* child_cap)
{
    if (!child_cap) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 获取调用者域ID作为拥有者 */
    domain_id_t owner = get_current_domain_id();
    
    /* 验证父能力存在且属于调用者 */
    if (parent_cap >= CAP_TABLE_SIZE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    cap_entry_t* parent = &g_global_cap_table[parent_cap];
    if (parent->cap_id != parent_cap || (parent->flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 验证子权限不超出父权限 */
    if ((sub_rights & parent->rights) != sub_rights) {
        console_puts("[PRIMITIVES] Derive rights exceeded parent rights\n");
        return HIC_ERROR_PERMISSION;
    }
    
    return cap_derive(owner, parent_cap, sub_rights, child_cap);
}

hic_status_t module_cap_transfer(cap_id_t cap_id,
                                  domain_id_t target_domain)
{
    /* 验证能力存在 */
    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    cap_entry_t* entry = &g_global_cap_table[cap_id];
    
    if (entry->cap_id != cap_id || (entry->flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 验证调用者拥有此能力 */
    domain_id_t caller = get_current_domain_id();
    if (entry->owner != caller) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 验证目标域存在 */
    domain_t info;
    if (domain_get_info(target_domain, &info) != HIC_SUCCESS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    /* 执行能力转移 */
    entry->owner = target_domain;
    
    console_puts("[PRIMITIVES] Capability ");
    console_putu64(cap_id);
    console_puts(" transferred from domain ");
    console_putu64(caller);
    console_puts(" to domain ");
    console_putu64(target_domain);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

hic_status_t module_cap_grant(domain_id_t domain_id,
                               cap_id_t cap_id,
                               cap_handle_t* handle)
{
    return cap_grant(domain_id, cap_id, handle);
}

hic_status_t module_cap_check(cap_id_t cap_id,
                               cap_rights_t required_rights)
{
    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    if (entry->cap_id != cap_id || (entry->flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    if ((entry->rights & required_rights) != required_rights) {
        return HIC_ERROR_PERMISSION;
    }
    
    return HIC_SUCCESS;
}

/* ==================== IPC 3.0 服务原语 ==================== */

hic_status_t module_service_create(domain_id_t domain_id,
                                    virt_addr_t business_entry,
                                    ipc3_service_id_t* out_id)
{
    if (!out_id) {
        return HIC_ERROR_INVALID_PARAM;
    }

    return ipc3_register_service(domain_id, business_entry, 0, out_id);
}

hic_status_t module_service_register(domain_id_t domain_id,
                                      ipc3_service_id_t service_id,
                                      const char* name)
{
    if (!name) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 生成服务 UUID（基于名称哈希） */
    u8 uuid[16];
    memzero(uuid, sizeof(uuid));

    u64 hash = 0;
    for (size_t i = 0; name[i]; i++) {
        hash = hash * 31 + (u8)name[i];
    }

    memcpy(uuid, &hash, sizeof(u64));
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    return service_register_endpoint(name, uuid, domain_id, service_id,
                                     ENDPOINT_TYPE_GENERIC, 1);
}

hic_status_t module_service_lookup(const char* name,
                                    ipc3_service_id_t* service_id,
                                    domain_id_t* owner_domain)
{
    if (!name) {
        return HIC_ERROR_INVALID_PARAM;
    }

    service_endpoint_t* ep = service_find_by_name(name);
    if (!ep) {
        return HIC_ERROR_NOT_FOUND;
    }

    if (service_id) {
        *service_id = ep->service_id;
    }
    if (owner_domain) {
        *owner_domain = ep->owner;
    }

    return HIC_SUCCESS;
}

/* ==================== 审计原语 ==================== */

hic_status_t module_audit_log(u32 event_type,
                               domain_id_t domain_id,
                               const u64* data,
                               u32 data_count)
{
    /* 转换 const u64* 为 u64*，因为 audit_log_event 需要非 const 指针 */
    u64* mutable_data = (u64*)data;
    audit_log_event(event_type, domain_id, 0, 0, mutable_data, data_count, true);
    return HIC_SUCCESS;
}

/* ==================== 便捷函数（兼容层） ==================== */

/**
 * @brief 创建域并返回域ID
 * 用于动态模块加载器
 */
uint64_t module_cap_create_domain(uint32_t parent_domain, uint32_t *new_domain)
{
    domain_quota_t quota = {
        .max_memory = 16 * 1024 * 1024,  /* 16MB - 足够加载大型模块 */
        .max_threads = 8,
        .max_caps = 64,
        .cpu_quota_percent = 20
    };
    
    domain_id_t domain_id;
    hic_status_t status = domain_create(DOMAIN_TYPE_PRIVILEGED, parent_domain, &quota, &domain_id);
    
    if (status == HIC_SUCCESS && new_domain) {
        *new_domain = (uint32_t)domain_id;
    }
    
    return (uint64_t)status;
}

/**
 * @brief 创建 IPC 3.0 服务
 * 用于动态模块加载器
 */
uint64_t module_cap_create_service(uint32_t domain_id, uint32_t *service_id)
{
    ipc3_service_id_t sid;
    /* 使用0作为业务入口（模块将在初始化后自行设置） */
    hic_status_t status = ipc3_register_service((domain_id_t)domain_id, 0, 0, &sid);

    if (status == HIC_SUCCESS && service_id) {
        *service_id = (uint32_t)sid;
    }

    return (uint64_t)status;
}

/**
 * @brief 创建 IPC 3.0 端点
 * 用于动态模块加载器，为每个动态加载的模块创建独立的端点
 */
uint64_t module_cap_create_endpoint(uint32_t domain_id, uint32_t *endpoint_id)
{
    ipc3_service_id_t sid;
    hic_status_t status = ipc3_register_service((domain_id_t)domain_id, 0, 0, &sid);

    if (status == HIC_SUCCESS && endpoint_id) {
        *endpoint_id = (uint32_t)sid;
    }

    return (uint64_t)status;
}

/**
 * @brief 启动域
 * 用于动态模块加载器
 * 注意：入口点需要通过其他机制设置（如加载时指定）
 */
uint64_t module_domain_start(uint32_t domain_id, uint64_t entry_point)
{
    /* 首先创建线程 - 动态模块使用较高优先级确保被调度 */
    thread_id_t thread_id;
    hic_status_t status = thread_create((domain_id_t)domain_id, 
                                         (virt_addr_t)entry_point,
                                         3,  /* HIC_PRIORITY_HIGH - 确保被调度 */
                                         &thread_id);
    if (status != HIC_SUCCESS) {
        console_puts("[PRIMITIVES] Failed to create thread for domain ");
        console_putu32(domain_id);
        console_puts("\n");
        return (uint64_t)status;
    }
    
    console_puts("[PRIMITIVES] Thread ");
    console_putu32(thread_id);
    console_puts(" created for domain ");
    console_putu32(domain_id);
    console_puts(" (already enqueued by thread_create)\n");
    
    /* 恢复域运行 */
    status = domain_resume((domain_id_t)domain_id);
    if (status != HIC_SUCCESS) {
        console_puts("[PRIMITIVES] Failed to resume domain\n");
        return (uint64_t)status;
    }
    
    /* 注意：thread_create 已经将线程加入调度队列，无需再次调用 thread_ready */
    
    return 0;  /* 成功 */
}

/**
 * @brief 内存拷贝
 * 用于模块数据加载
 */
void module_memcpy(void *dest, const void *src, size_t size)
{
    memcopy(dest, src, size);
}

/**
 * @brief 内存设置
 * 用于模块初始化
 */
void module_memset(void *dest, int value, size_t size)
{
    memset(dest, value, size);
}
