/**
 * HIK能力系统实现（集成审计日志）
 * 遵循三层模型文档第3.1节
 */

#include "capability.h"
#include "domain.h"
#include "audit.h"
#include "lib/string.h"
#include "lib/mem.h"

#define CAP_TABLE_SIZE  (HIK_DOMAIN_MAX * 256)

/* 全局能力表 */
static cap_entry_t g_cap_table[CAP_TABLE_SIZE];
static u64 g_cap_counter = 1;

/* 初始化能力系统 */
void capability_system_init(void)
{
    memzero(g_cap_table, sizeof(g_cap_table));
}

/* 创建内存能力 */
hik_status_t cap_create_memory(domain_id_t owner, phys_addr_t base, 
                               size_t size, cap_rights_t rights, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || out == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 查找空闲槽位 */
    for (u32 i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].cap_id == 0) {
            g_cap_table[i].cap_id = i;
            g_cap_table[i].type = CAP_TYPE_MEMORY;
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
    
    /* 记录审计日志 */
    AUDIT_LOG_CAP_VERIFY(domain, cap, true);
    
    return HIK_SUCCESS;
}

/* 获取能力信息 */
hik_status_t cap_get_info(cap_id_t cap, cap_entry_t *info)
{
    if (cap >= CAP_TABLE_SIZE || info == NULL) {
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
    
    return HIK_SUCCESS;
}

/* 派生能力 */
hik_status_t cap_derive(domain_id_t owner, cap_id_t parent, 
                        cap_rights_t sub_rights, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || parent >= CAP_TABLE_SIZE || 
        out == NULL) {
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
            g_cap_table[i].type = CAP_TYPE_CAP_DERIVE;
            g_cap_table[i].rights = sub_rights;
            g_cap_table[i].owner = owner;
            g_cap_table[i].derive.parent_cap = parent;
            g_cap_table[i].derive.sub_rights = sub_rights;
            g_cap_table[i].ref_count = 1;
            g_cap_table[i].flags = 0;
            
            /* 增加父能力引用计数 */
            parent_entry->ref_count++;
            
            *out = i;
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
    
    return HIK_SUCCESS;
}

/* 创建MMIO能力 */
hik_status_t cap_create_mmio(domain_id_t owner, phys_addr_t base, 
                             size_t size, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || out == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    for (u32 i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].cap_id == 0) {
            g_cap_table[i].cap_id = i;
            g_cap_table[i].type = CAP_TYPE_MMIO;
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

/* 创建IRQ能力 */
hik_status_t cap_create_irq(domain_id_t owner, irq_vector_t vector, cap_id_t *out)
{
    if (owner >= HIK_DOMAIN_MAX || out == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    for (u32 i = 1; i < CAP_TABLE_SIZE; i++) {
        if (g_cap_table[i].cap_id == 0) {
            g_cap_table[i].cap_id = i;
            g_cap_table[i].type = CAP_TYPE_IRQ;
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