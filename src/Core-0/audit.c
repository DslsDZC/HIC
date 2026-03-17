/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 审计日志机制层实现
 * 
 * Core-0 提供审计日志记录和查询的原语实现
 * 策略决策由 Privileged-1 的 audit_service 负责
 */

#include "audit.h"
#include "hal.h"
#include "lib/mem.h"
#include "lib/console.h"

/* ========== 审计缓冲区 ========== */

typedef struct audit_buffer {
    void* base;
    size_t size;
    size_t write_offset;
    u64 sequence;
    bool initialized;
} audit_buffer_t;

static audit_buffer_t g_audit_buffer;

/* ========== 初始化 ========== */

void audit_system_init(void)
{
    memzero(&g_audit_buffer, sizeof(audit_buffer_t));
    console_puts("[AUDIT] Audit mechanism initialized\n");
}

void audit_system_init_buffer(phys_addr_t base, size_t size)
{
    if (size < sizeof(audit_entry_t)) {
        console_puts("[AUDIT] Buffer too small\n");
        return;
    }
    
    g_audit_buffer.base = (void*)base;
    g_audit_buffer.size = size;
    g_audit_buffer.write_offset = 0;
    g_audit_buffer.sequence = 1;
    g_audit_buffer.initialized = true;
    
    memzero(g_audit_buffer.base, size);
    
    console_puts("[AUDIT] Buffer at 0x");
    console_puthex64(base);
    console_puts(", size ");
    console_putu64(size);
    console_puts("\n");
}

/* ========== 记录事件 ========== */

void audit_log_event(audit_event_type_t type, domain_id_t domain,
                     cap_id_t cap, thread_id_t thread_id,
                     u64 *data, u32 data_count, u8 result)
{
    if (!g_audit_buffer.initialized) return;
    
    /* 检查空间 */
    if (g_audit_buffer.write_offset + sizeof(audit_entry_t) > g_audit_buffer.size) {
        g_audit_buffer.write_offset = 0;  /* 循环覆盖 */
    }
    
    /* 构造条目 */
    audit_entry_t entry;
    memzero(&entry, sizeof(entry));
    
    entry.timestamp = hal_get_timestamp();
    entry.sequence = (u32)g_audit_buffer.sequence++;
    entry.type = type;
    entry.domain = domain;
    entry.cap_id = cap;
    entry.thread_id = thread_id;
    entry.result = result;
    
    if (data && data_count > 0) {
        u32 copy_count = data_count < 4 ? data_count : 4;
        memcopy(entry.data, data, copy_count * sizeof(u64));
    }
    
    /* 写入缓冲区 */
    u8* write_ptr = (u8*)g_audit_buffer.base + g_audit_buffer.write_offset;
    hal_memory_barrier();
    memcopy(write_ptr, &entry, sizeof(audit_entry_t));
    g_audit_buffer.write_offset += sizeof(audit_entry_t);
    hal_memory_barrier();
}

/* ========== 统计 ========== */

u64 audit_get_entry_count(void)
{
    if (!g_audit_buffer.initialized) return 0;
    return g_audit_buffer.sequence - 1;
}

u64 audit_get_buffer_usage(void)
{
    if (!g_audit_buffer.initialized || g_audit_buffer.size == 0) return 0;
    return (g_audit_buffer.write_offset * 100) / g_audit_buffer.size;
}

/* ========== 查询实现 ========== */

#define DOMAIN_ID_INVALID ((domain_id_t)-1)

hic_status_t audit_query(const audit_query_filter_t *filter,
                          void *buffer, size_t buffer_size, size_t *out_size)
{
    if (!g_audit_buffer.initialized || !filter || !buffer || !out_size) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    size_t max_entries = buffer_size / sizeof(audit_entry_t);
    if (max_entries == 0) {
        *out_size = 0;
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    audit_entry_t *output = (audit_entry_t*)buffer;
    u32 match_count = 0;
    u32 skipped = 0;
    
    size_t total_entries = g_audit_buffer.write_offset / sizeof(audit_entry_t);
    
    for (size_t i = 0; i < total_entries && match_count < filter->max_results; i++) {
        size_t idx = (total_entries - 1 - i);
        audit_entry_t *entry = (audit_entry_t*)((u8*)g_audit_buffer.base +
                                                 idx * sizeof(audit_entry_t));
        
        if (entry->sequence == 0) continue;
        
        bool matches = true;
        
        if (filter->domain != DOMAIN_ID_INVALID && entry->domain != filter->domain) {
            matches = false;
        }
        if (matches && filter->type != 0 && entry->type != filter->type) {
            matches = false;
        }
        if (matches && filter->start_time != 0 && entry->timestamp < filter->start_time) {
            matches = false;
        }
        if (matches && filter->end_time != 0 && entry->timestamp > filter->end_time) {
            matches = false;
        }
        
        if (matches) {
            if (skipped < filter->offset) {
                skipped++;
                continue;
            }
            if (match_count < max_entries) {
                memcopy(&output[match_count], entry, sizeof(audit_entry_t));
                match_count++;
            }
        }
    }
    
    *out_size = match_count * sizeof(audit_entry_t);
    return HIC_SUCCESS;
}

hic_status_t audit_query_by_domain(domain_id_t domain,
                                    audit_entry_t *buffer,
                                    u32 buffer_count, u32 *out_count)
{
    if (!g_audit_buffer.initialized || !buffer || !out_count) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u32 match_count = 0;
    size_t total_entries = g_audit_buffer.write_offset / sizeof(audit_entry_t);
    
    for (size_t i = 0; i < total_entries && match_count < buffer_count; i++) {
        size_t idx = (total_entries - 1 - i);
        audit_entry_t *entry = (audit_entry_t*)((u8*)g_audit_buffer.base +
                                                 idx * sizeof(audit_entry_t));
        
        if (entry->sequence != 0 && entry->domain == domain) {
            memcopy(&buffer[match_count], entry, sizeof(audit_entry_t));
            match_count++;
        }
    }
    
    *out_count = match_count;
    return HIC_SUCCESS;
}

hic_status_t audit_query_by_type(audit_event_type_t type,
                                  audit_entry_t *buffer,
                                  u32 buffer_count, u32 *out_count)
{
    if (!g_audit_buffer.initialized || !buffer || !out_count) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u32 match_count = 0;
    size_t total_entries = g_audit_buffer.write_offset / sizeof(audit_entry_t);
    
    for (size_t i = 0; i < total_entries && match_count < buffer_count; i++) {
        size_t idx = (total_entries - 1 - i);
        audit_entry_t *entry = (audit_entry_t*)((u8*)g_audit_buffer.base +
                                                 idx * sizeof(audit_entry_t));
        
        if (entry->sequence != 0 && entry->type == type) {
            memcopy(&buffer[match_count], entry, sizeof(audit_entry_t));
            match_count++;
        }
    }
    
    *out_count = match_count;
    return HIC_SUCCESS;
}

hic_status_t audit_query_latest(audit_entry_t *buffer, u32 count, u32 *out_count)
{
    if (!g_audit_buffer.initialized || !buffer || !out_count) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    size_t total_entries = g_audit_buffer.write_offset / sizeof(audit_entry_t);
    u32 copy_count = (count < total_entries) ? count : (u32)total_entries;
    
    for (u32 i = 0; i < copy_count; i++) {
        size_t idx = (total_entries - 1 - i);
        audit_entry_t *entry = (audit_entry_t*)((u8*)g_audit_buffer.base +
                                                 idx * sizeof(audit_entry_t));
        memcopy(&buffer[i], entry, sizeof(audit_entry_t));
    }
    
    *out_count = copy_count;
    return HIC_SUCCESS;
}