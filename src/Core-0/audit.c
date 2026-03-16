/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC审计日志系统实现
 * 遵循文档第3.3节：安全审计与防篡改日志
 * 记录所有安全相关事件
 */

#include "audit.h"
#include "hal.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "lib/string.h"

/* 审计日志缓冲区（全局） */
static audit_buffer_t g_audit_buffer;

/* 审计日志初始化消息 */
static const char audit_init_msg[] = "[AUDIT] Audit system initialized (buffer not yet allocated)\n";

/* 审计日志初始化 */
void audit_system_init(void)
{
    /* 清零审计缓冲区结构 */
    memzero(&g_audit_buffer, sizeof(audit_buffer_t));
    
    g_audit_buffer.initialized = false;
    
    console_puts(audit_init_msg);
}

/* 初始化审计日志缓冲区 */
void audit_system_init_buffer(phys_addr_t base, size_t size)
{
    if (size < sizeof(audit_entry_t)) {
        console_puts("[AUDIT] ERROR: Buffer too small\n");
        return;
    }
    
    g_audit_buffer.base = (void*)base;
    g_audit_buffer.size = size;
    g_audit_buffer.write_offset = 0;
    g_audit_buffer.sequence = 1;
    g_audit_buffer.initialized = true;
    
    /* 清零缓冲区 */
    memzero(g_audit_buffer.base, size);
    
    console_puts("[AUDIT] Audit buffer initialized at 0x");
    console_puthex64(base);
    console_puts(", size ");
    console_putu64(size);
    console_puts(" bytes\n");
}

/* 记录审计事件 */
void audit_log_event(audit_event_type_t type, domain_id_t domain, 
                     cap_id_t cap, thread_id_t thread_id,
                     u64 *data, u32 data_count, u8 result)
{
    if (!g_audit_buffer.initialized) {
        /* 缓冲区未初始化，跳过记录 */
        return;
    }
    
    /* 检查缓冲区空间 */
    if (g_audit_buffer.write_offset + sizeof(audit_entry_t) > g_audit_buffer.size) {
        /* 缓冲区已满，循环写入（覆盖最旧的记录） */
        g_audit_buffer.write_offset = 0;
    }
    
    /* 获取时间戳 */
    u64 timestamp = hal_get_timestamp();
    
    /* 构造审计条目 */
    audit_entry_t entry;
    memzero(&entry, sizeof(audit_entry_t));
    
    entry.timestamp = timestamp;
    entry.sequence = (u32)g_audit_buffer.sequence++;
    entry.type = type;
    entry.domain = domain;
    entry.cap_id = cap;
    entry.thread_id = thread_id;
    entry.result = result;
    
    /* 复制数据 */
    if (data && data_count > 0) {
        u32 copy_count = data_count < 4 ? data_count : 4;
        memcopy(entry.data, data, copy_count * sizeof(u64));
    }
    
    /* 原子写入条目到缓冲区 */
    u8* write_ptr = (u8*)g_audit_buffer.base + g_audit_buffer.write_offset;
    
    /* 内存屏障确保写入顺序 */
    hal_memory_barrier();
    
    /* 写入条目 */
    memcopy(write_ptr, &entry, sizeof(audit_entry_t));
    
    /* 更新写入偏移 */
    g_audit_buffer.write_offset += sizeof(audit_entry_t);
    
    /* 内存屏障确保写入完成 */
    hal_memory_barrier();
}

/* 获取条目总数 */
u64 audit_get_entry_count(void)
{
    if (!g_audit_buffer.initialized) {
        return 0;
    }
    
    /* 计算已使用的条目数 */
    return g_audit_buffer.sequence - 1;
}

/* 获取缓冲区使用率 */
u64 audit_get_buffer_usage(void)
{
    if (!g_audit_buffer.initialized || g_audit_buffer.size == 0) {
        return 0;
    }
    
    return (g_audit_buffer.write_offset * 100) / g_audit_buffer.size;
}

/* 内存安全检测：空指针访问 */
void audit_check_null_pointer(void* ptr, const char* context)
{
    if (ptr == NULL) {
        u64 data[2] = {(u64)ptr, 0};
        u32 context_len = context ? (u32)strlen(context) : 0;
        if (context_len > 0 && context_len <= 64) {
            /* 复制上下文信息（最多64字节） */
            memcopy(&data[1], context, context_len > 32 ? 32 : context_len);
        }
        
        console_puts("[AUDIT] NULL POINTER DETECTED: ");
        if (context) {
            console_puts(context);
        } else {
            console_puts("unknown");
        }
        console_puts("\n");
        
        audit_log_event(AUDIT_EVENT_NULL_POINTER, 0, 0, 0, data, 2, false);
    }
}

/* 内存安全检测：缓冲区溢出 */
void audit_check_buffer_overflow(void* ptr, size_t size, size_t max_size, const char* context)
{
    if (size > max_size) {
        u64 data[4] = {(u64)ptr, (u64)size, (u64)max_size, 0};
        u32 context_len = context ? (u32)strlen(context) : 0;
        if (context_len > 0 && context_len <= 64) {
            /* 复制上下文信息（最多64字节） */
            memcopy(&data[3], context, context_len > 32 ? 32 : context_len);
        }
        
        console_puts("[AUDIT] BUFFER OVERFLOW DETECTED: ");
        if (context) {
            console_puts(context);
        } else {
            console_puts("unknown");
        }
        console_puts(" (size=");
        console_putu64(size);
        console_puts(", max=");
        console_putu64(max_size);
        console_puts(")\n");
        
        audit_log_event(AUDIT_EVENT_BUFFER_OVERFLOW, 0, 0, 0, data, 4, false);
    }
}

/* 内存安全检测：无效内存访问 */
void audit_check_invalid_memory(void* ptr, const char* context)
{
    /* 检查指针是否在合理的内存范围内 */
    if ((u64)ptr < 0x1000 || (u64)ptr >= 0x800000000000ULL) {
        u64 data[2] = {(u64)ptr, 0};
        u32 context_len = context ? (u32)strlen(context) : 0;
        if (context_len > 0 && context_len <= 64) {
            /* 复制上下文信息（最多64字节） */
            memcopy(&data[1], context, context_len > 32 ? 32 : context_len);
        }
        
        console_puts("[AUDIT] INVALID MEMORY ACCESS: ");
        if (context) {
            console_puts(context);
        } else {
            console_puts("unknown");
        }
        console_puts(" (ptr=0x");
        console_puthex64((u64)ptr);
        console_puts(")\n");
        
        audit_log_event(AUDIT_EVENT_INVALID_MEMORY, 0, 0, 0, data, 2, false);
    }
}

/* ==================== 审计查询机制实现 ==================== */

#define DOMAIN_ID_INVALID  ((domain_id_t)-1)

/**
 * 查询审计日志（机制层核心实现）
 */
hic_status_t audit_query(const audit_query_filter_t *filter,
                          void *buffer, size_t buffer_size, size_t *out_size)
{
    if (!g_audit_buffer.initialized || !filter || !buffer || !out_size) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 计算最大可返回条目数 */
    size_t max_entries = buffer_size / sizeof(audit_entry_t);
    if (max_entries == 0) {
        *out_size = 0;
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    audit_entry_t *output = (audit_entry_t*)buffer;
    u32 match_count = 0;
    u32 total_matches = 0;
    u32 skipped = 0;
    
    /* 计算缓冲区中的条目总数 */
    size_t total_entries = g_audit_buffer.write_offset / sizeof(audit_entry_t);
    
    /* 从最新条目开始向前遍历（逆序） */
    for (size_t i = 0; i < total_entries && match_count < filter->max_results; i++) {
        size_t idx = (total_entries - 1 - i);
        audit_entry_t *entry = (audit_entry_t*)((u8*)g_audit_buffer.base + 
                                                 idx * sizeof(audit_entry_t));
        
        /* 检查有效性 */
        if (entry->sequence == 0) continue;
        
        /* 应用过滤器 */
        bool matches = true;
        
        /* 按域过滤 */
        if (filter->domain != DOMAIN_ID_INVALID && entry->domain != filter->domain) {
            matches = false;
        }
        
        /* 按事件类型过滤 */
        if (matches && filter->type != 0 && entry->type != filter->type) {
            matches = false;
        }
        
        /* 按时间范围过滤 */
        if (matches && filter->start_time != 0 && entry->timestamp < filter->start_time) {
            matches = false;
        }
        if (matches && filter->end_time != 0 && entry->timestamp > filter->end_time) {
            matches = false;
        }
        
        if (matches) {
            total_matches++;
            
            /* 应用偏移 */
            if (skipped < filter->offset) {
                skipped++;
                continue;
            }
            
            /* 复制到输出缓冲区 */
            if (match_count < max_entries) {
                memcopy(&output[match_count], entry, sizeof(audit_entry_t));
                match_count++;
            }
        }
    }
    
    *out_size = match_count * sizeof(audit_entry_t);
    return HIC_SUCCESS;
}

/**
 * 按域查询审计日志
 */
hic_status_t audit_query_by_domain(domain_id_t domain,
                                    audit_entry_t *buffer, 
                                    u32 buffer_count,
                                    u32 *out_count)
{
    if (!g_audit_buffer.initialized || !buffer || !out_count) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u32 match_count = 0;
    size_t total_entries = g_audit_buffer.write_offset / sizeof(audit_entry_t);
    
    /* 从最新开始遍历 */
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

/**
 * 按事件类型查询审计日志
 */
hic_status_t audit_query_by_type(audit_event_type_t type,
                                  audit_entry_t *buffer,
                                  u32 buffer_count,
                                  u32 *out_count)
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

/**
 * 获取最新N条审计日志
 */
hic_status_t audit_query_latest(audit_entry_t *buffer, u32 count, u32 *out_count)
{
    if (!g_audit_buffer.initialized || !buffer || !out_count) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    size_t total_entries = g_audit_buffer.write_offset / sizeof(audit_entry_t);
    u32 copy_count = (count < total_entries) ? count : (u32)total_entries;
    
    /* 从最新开始复制 */
    for (u32 i = 0; i < copy_count; i++) {
        size_t idx = (total_entries - 1 - i);
        audit_entry_t *entry = (audit_entry_t*)((u8*)g_audit_buffer.base + 
                                                 idx * sizeof(audit_entry_t));
        memcopy(&buffer[i], entry, sizeof(audit_entry_t));
    }
    
    *out_count = copy_count;
    return HIC_SUCCESS;
}
