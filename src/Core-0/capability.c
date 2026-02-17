/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK能力系统实现（集成审计日志）
 * 遵循三层模型文档第3.1节
 */

#include "capability.h"
#include "domain.h"
#include "audit.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 能力表 */
static cap_entry_t g_cap_table[CAP_TABLE_SIZE];

/* 能力系统互斥锁 */

/* 初始化能力系统 */
void capability_system_init(void)
{
    memzero(g_cap_table, sizeof(g_cap_table));
}

/* 创建内存能力 */
hik_status_t cap_create_memory(domain_id_t owner, phys_addr_t base, 
                               size_t size, cap_rights_t rights, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || out == 0) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 查找空闲槽位 */
    for (u32 i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].cap_id == 0) {
            g_cap_table[i].cap_id = i;
            g_cap_table[i].type = CAP_MEMORY;
            g_cap_table[i].rights = rights;
            g_cap_table[i].owner = owner;
            g_cap_table[i].memory.base = base;
            g_cap_table[i].memory.size = size;
            g_cap_table[i].ref_count = 1;
            g_cap_table[i].flags = 0;
            
            /* 记录审计日志 */
            AUDIT_LOG_CAP_CREATE(owner, i, true);
            
            *out = i;
            return HIK_SUCCESS;
        }
    }
    
    /* 记录审计日志 */
    AUDIT_LOG_CAP_CREATE(owner, 0, false);
    
    return HIK_ERROR_NO_MEMORY;
}

/* 验证能力访问 */
hik_status_t cap_check_access(domain_id_t domain, cap_id_t cap, cap_rights_t required)
{
    if (domain >= HIK_DOMAIN_MAX || cap >= CAP_TABLE_SIZE) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_cap_table[cap];
    
    /* 检查能力是否有效 */
    if (entry->cap_id == 0 || entry->cap_id != cap) {
        AUDIT_LOG_CAP_VERIFY(domain, cap, false);
        return HIK_ERROR_CAP_INVALID;
    }
    
    /* 检查能力是否被撤销 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        AUDIT_LOG_CAP_VERIFY(domain, cap, false);
        return HIK_ERROR_CAP_REVOKED;
    }
    
    /* 检查所有权 */
    if (entry->owner != domain) {
        AUDIT_LOG_CAP_VERIFY(domain, cap, false);
        return HIK_ERROR_PERMISSION;
    }
    
    /* 检查权限 */
    if ((entry->rights & required) != required) {
        AUDIT_LOG_CAP_VERIFY(domain, cap, false);
        return HIK_ERROR_PERMISSION;
    }
    
    /* 调用形式化验证 */
    extern bool fv_verify_syscall_atomicity(syscall_id_t, u64, u64);
    extern int fv_check_all_invariants(void);
    
    /* 在关键操作前后验证不变式 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[CAP] Invariant violation detected!\n");
    }
    
    AUDIT_LOG_CAP_VERIFY(domain, cap, true);
    return HIK_SUCCESS;
}

/* 获取能力信息 */
hik_status_t cap_get_info(cap_id_t cap, cap_entry_t *info)
{
    if (cap >= CAP_TABLE_SIZE || info == 0) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_cap_table[cap];
    
    if (entry->cap_id == 0 || entry->cap_id != cap) {
        return HIK_ERROR_CAP_INVALID;
    }
    
    memcopy(info, entry, sizeof(cap_entry_t));
    return HIK_SUCCESS;
}

/* 传递能力 */
hik_status_t cap_transfer(domain_id_t from, domain_id_t to, cap_id_t cap)
{
    if (from >= HIK_DOMAIN_MAX || to >= HIK_DOMAIN_MAX || 
        cap >= CAP_TABLE_SIZE) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_cap_table[cap];
    
    /* 验证源域拥有此能力 */
    if (entry->owner != from) {
        AUDIT_LOG_CAP_TRANSFER(from, to, cap, false);
        return HIK_ERROR_PERMISSION;
    }
    
    /* 检查能力是否可转移 */
    if (entry->flags & CAP_FLAG_IMMUTABLE) {
        AUDIT_LOG_CAP_TRANSFER(from, to, cap, false);
        return HIK_ERROR_PERMISSION;
    }
    
    /* 转移所有权 */
    entry->owner = to;
    entry->ref_count++;
    
    /* 记录审计日志 */
    AUDIT_LOG_CAP_TRANSFER(from, to, cap, true);
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[CAP] Invariant violation detected after cap_transfer!\n");
    }
    
    return HIK_SUCCESS;
}

/* 派生能力 */
hik_status_t cap_derive(domain_id_t owner, cap_id_t parent, 
                        cap_rights_t sub_rights, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || parent >= CAP_TABLE_SIZE || 
        out == 0) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *parent_entry = &g_cap_table[parent];
    
    /* 验证父能力 */
    if (parent_entry->cap_id == 0 || parent_entry->owner != owner) {
        return HIK_ERROR_CAP_INVALID;
    }
    
    /* 检查子权限是否为父权限的子集 */
    if ((sub_rights & parent_entry->rights) != sub_rights) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 查找空闲槽位创建派生能力 */
    for (u32 i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].cap_id == 0) {
            g_cap_table[i].cap_id = i;
            g_cap_table[i].type = CAP_CAP_DERIVE;
            g_cap_table[i].rights = sub_rights;
            g_cap_table[i].owner = owner;
            g_cap_table[i].derive.parent_cap = parent;
            g_cap_table[i].derive.sub_rights = sub_rights;
            g_cap_table[i].ref_count = 1;
            g_cap_table[i].flags = 0;
            
            /* 增加父能力引用计数 */
            parent_entry->ref_count++;
            
            *out = i;
            
            /* 调用形式化验证 */
            if (fv_check_all_invariants() != FV_SUCCESS) {
                console_puts("[CAP] Invariant violation detected after cap_derive!\n");
            }
            
            return HIK_SUCCESS;
        }
    }
    
    return HIK_ERROR_NO_MEMORY;
}

/* 撤销能力 */
hik_status_t cap_revoke(cap_id_t cap)
{
    if (cap >= CAP_TABLE_SIZE) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_cap_table[cap];
    domain_id_t owner = entry->owner;
    
    /* 检查能力是否已撤销 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        return HIK_ERROR_CAP_REVOKED;
    }
    
    /* 标记为已撤销 */
    entry->flags |= CAP_FLAG_REVOKED;
    entry->ref_count = 0;
    
    /* 记录审计日志 */
    AUDIT_LOG_CAP_REVOKE(owner, cap, true);
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[CAP] Invariant violation detected after cap_revoke!\n");
    }
    
    return HIK_SUCCESS;
}

/* 创建MMIO能力 */
hik_status_t cap_create_mmio(domain_id_t owner, phys_addr_t base, 
                             size_t size, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || out == 0) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    for (u32 i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].cap_id == 0) {
            g_cap_table[i].cap_id = i;
            g_cap_table[i].type = CAP_MMIO;
            g_cap_table[i].rights = CAP_MEM_READ | CAP_MEM_WRITE | CAP_MEM_DEVICE;
            g_cap_table[i].owner = owner;
            g_cap_table[i].mmio.base = base;
            g_cap_table[i].mmio.size = size;
            g_cap_table[i].ref_count = 1;
            g_cap_table[i].flags = 0;
            
            AUDIT_LOG_CAP_CREATE(owner, i, true);
            *out = i;
            return HIK_SUCCESS;
        }
    }
    
    AUDIT_LOG_CAP_CREATE(owner, 0, false);
    return HIK_ERROR_NO_MEMORY;
}

/* ============================================ */
/* 形式化验证接口实现 */
/* ============================================ */

/**
 * 检查能力是否存在
 */
bool capability_exists(cap_id_t cap)
{
    if (cap >= CAP_TABLE_SIZE) {
        return false;
    }
    
    return g_cap_table[cap].cap_id != 0 && 
           !(g_cap_table[cap].flags & CAP_FLAG_REVOKED);
}

/**
 * 获取能力的派生能力列表
 */
cap_id_t* get_capability_derivatives(cap_id_t cap)
{
    static cap_id_t derivative_cache[256];
    
    if (cap >= CAP_TABLE_SIZE || !capability_exists(cap)) {
        derivative_cache[0] = INVALID_CAP_ID;
        return derivative_cache;
    }
    
    /* 查找所有派生自该能力的能力 */
    u32 count = 0;
    for (cap_id_t i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].type == CAP_CAP_DERIVE &&
            g_cap_table[i].derive.parent_cap == cap &&
            capability_exists(i)) {
            
            if (count < 255) {
                derivative_cache[count++] = i;
            }
        }
    }
    
    derivative_cache[count] = INVALID_CAP_ID;
    return derivative_cache;
}

/**
 * 获取能力的权限
 */
u64 get_capability_permissions(cap_id_t cap)
{
    if (cap >= CAP_TABLE_SIZE || !capability_exists(cap)) {
        return 0;
    }
    
    return g_cap_table[cap].rights;
}

/**
 * 获取能力的类型
 */
cap_type_t get_capability_type(cap_id_t cap)
{
    if (cap >= CAP_TABLE_SIZE || !capability_exists(cap)) {
        return CAP_TYPE_COUNT;
    }
    
    return g_cap_table[cap].type;
}

/**
 * 获取能力关联的对象类型
 */
obj_type_t get_capability_object_type(cap_id_t cap)
{
    if (cap >= CAP_TABLE_SIZE || !capability_exists(cap)) {
        return OBJ_TYPE_COUNT;
    }
    
    /* 根据能力类型返回对象类型 */
    switch (g_cap_table[cap].type) {
    case CAP_MEMORY:
        return OBJ_MEMORY;
    case CAP_MMIO:
        return OBJ_DEVICE;
    case CAP_ENDPOINT:
        return OBJ_IPC;
    case CAP_CAP_DERIVE:
        return OBJ_SHARED;
    default:
        return OBJ_TYPE_COUNT;
    }
    return OBJ_TYPE_COUNT;  /* 永不执行，但满足编译器 */
}

/**
 * 获取两个域共享的能力
 */
cap_id_t* get_shared_capabilities(domain_id_t d1, domain_id_t d2)
{
    static cap_id_t shared_cache[256];
    
    if (d1 >= MAX_DOMAINS || d2 >= MAX_DOMAINS) {
        shared_cache[0] = INVALID_CAP_ID;
        return shared_cache;
    }
    
    domain_t domain1, domain2;
    if (domain_get_info(d1, &domain1) != HIK_SUCCESS ||
        domain_get_info(d2, &domain2) != HIK_SUCCESS) {
        shared_cache[0] = INVALID_CAP_ID;
        return shared_cache;
    }
    
    /* 查找两个域都持有的能力 */
    u32 count = 0;
    for (u32 i = 0; i < domain1.cap_count && count < 255; i++) {
        cap_id_t cap1 = domain1.cap_space[i].cap_id;
        
        /* 检查domain2是否也持有该能力 */
        for (u32 j = 0; j < domain2.cap_count && count < 255; j++) {
            cap_id_t cap2 = domain2.cap_space[j].cap_id;
            
            if (cap1 == cap2 && capability_exists(cap1)) {
                shared_cache[count++] = cap1;
                break;
            }
        }
    }
    
    shared_cache[count] = INVALID_CAP_ID;
    return shared_cache;
}

/* 创建IRQ能力 */
hik_status_t cap_create_irq(domain_id_t owner, irq_vector_t vector, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || out == 0) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    for (u32 i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].cap_id == 0) {
            g_cap_table[i].cap_id = i;
            g_cap_table[i].type = CAP_IRQ;
            g_cap_table[i].rights = 0;
            g_cap_table[i].owner = owner;
            g_cap_table[i].irq.vector = vector;
            g_cap_table[i].ref_count = 1;
            g_cap_table[i].flags = 0;
            
            AUDIT_LOG_CAP_CREATE(owner, i, true);
            *out = i;
            return HIK_SUCCESS;
        }
    }
    
    AUDIT_LOG_CAP_CREATE(owner, 0, false);
    return HIK_ERROR_NO_MEMORY;
}