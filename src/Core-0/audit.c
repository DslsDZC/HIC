/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC审计日志系统实现
 * 遵循文档第3.3节：安全审计与防篡改日志
 */

#include "audit.h"
#include "hal.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "lib/string.h"

/* 审计日志缓冲区（全局） */
static audit_buffer_t g_audit_buffer;

/* 审计日志初始化 */
void audit_system_init(void)
{
    memzero(&g_audit_buffer, sizeof(audit_buffer_t));
    g_audit_buffer.initialized = false;
    
    console_puts("[AUDIT] Audit system initialized (buffer not yet allocated)\n");
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
