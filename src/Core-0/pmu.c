/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 性能监控抽象层实现
 * 
 * 提供跨平台的性能监控功能
 */

#include "pmu.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"
#include "include/hal.h"

/* 最大计数器数量 */
#define PMU_MAX_COUNTERS    32
#define PMU_MAX_GROUPS      8
#define PMU_MAX_BREAKPOINTS 8
#define PMU_MAX_SAMPLE_BUFFER 1024

/* 架构操作回调 */
static pmu_arch_ops_t g_pmu_arch_ops = {0};

/* PMU 硬件信息 */
static pmu_hw_info_t g_pmu_hw_info = {
    .gp_counter_count = 4,
    .gp_counter_width = 48,
    .fixed_counter_count = 3,
    .fixed_counter_width = 48,
    .supports_sampling = true,
    .max_sample_period = 0xFFFFFFFFULL,
    .event_count = 0,
    .events = NULL,
    .arch_version = 1,
    .arch_flags = 0,
};

/* 计数器池 */
static pmu_counter_t g_counters[PMU_MAX_COUNTERS];
static bool g_counter_used[PMU_MAX_COUNTERS];

/* 计数器组 */
static pmu_counter_group_t g_groups[PMU_MAX_GROUPS];
static bool g_group_used[PMU_MAX_GROUPS];

/* 硬件断点 */
static pmu_breakpoint_t g_breakpoints[PMU_MAX_BREAKPOINTS];
static bool g_breakpoint_used[PMU_MAX_BREAKPOINTS];

/* 采样缓冲区 */
static pmu_sample_buffer_t g_sample_buffer;
static pmu_sample_record_t g_sample_records[PMU_MAX_SAMPLE_BUFFER];
static bool g_sampling_active = false;
static pmu_sample_config_t g_sample_config;

/* 全局统计 */
static u64 g_total_cycles = 0;
static u64 g_total_instructions = 0;
static u64 g_total_cache_misses = 0;

/* ========== 初始化 ========== */

/**
 * 初始化 PMU
 */
void pmu_init(void)
{
    console_puts("[PMU] Initializing performance monitoring\n");
    
    /* 清零计数器池 */
    memzero(g_counters, sizeof(g_counters));
    memzero(g_counter_used, sizeof(g_counter_used));
    
    /* 清零计数器组 */
    memzero(g_groups, sizeof(g_groups));
    memzero(g_group_used, sizeof(g_group_used));
    
    /* 清零断点 */
    memzero(g_breakpoints, sizeof(g_breakpoints));
    memzero(g_breakpoint_used, sizeof(g_breakpoint_used));
    
    /* 初始化采样缓冲区 */
    g_sample_buffer.records = g_sample_records;
    g_sample_buffer.capacity = PMU_MAX_SAMPLE_BUFFER;
    g_sample_buffer.count = 0;
    g_sample_buffer.head = 0;
    g_sample_buffer.tail = 0;
    g_sample_buffer.overwrite = false;
    
    g_sampling_active = false;
    
    /* 获取架构硬件信息 */
    if (g_pmu_arch_ops.get_hw_info) {
        g_pmu_arch_ops.get_hw_info(&g_pmu_hw_info);
    }
    
    console_puts("[PMU] PMU initialized\n");
    console_puts("[PMU]   GP counters: ");
    console_putu32(g_pmu_hw_info.gp_counter_count);
    console_puts(" x ");
    console_putu32(g_pmu_hw_info.gp_counter_width);
    console_puts(" bits\n");
    console_puts("[PMU]   Fixed counters: ");
    console_putu32(g_pmu_hw_info.fixed_counter_count);
    console_puts("\n");
    console_puts("[PMU]   Sampling: ");
    console_puts(g_pmu_hw_info.supports_sampling ? "supported" : "not supported");
    console_puts("\n");
}

/**
 * 关闭 PMU
 */
void pmu_shutdown(void)
{
    console_puts("[PMU] Shutting down performance monitoring\n");
    
    /* 停止采样 */
    if (g_sampling_active) {
        pmu_sampling_stop();
    }
    
    /* 禁用所有计数器 */
    for (u32 i = 0; i < PMU_MAX_COUNTERS; i++) {
        if (g_counter_used[i]) {
            pmu_counter_disable(i);
            pmu_counter_free(i);
        }
    }
    
    /* 清除所有断点 */
    for (u32 i = 0; i < PMU_MAX_BREAKPOINTS; i++) {
        if (g_breakpoint_used[i]) {
            pmu_breakpoint_clear(i);
        }
    }
}

/**
 * 注册架构操作回调
 */
void pmu_register_arch_ops(const pmu_arch_ops_t *ops)
{
    if (ops) {
        memcopy(&g_pmu_arch_ops, ops, sizeof(pmu_arch_ops_t));
        console_puts("[PMU] Architecture ops registered\n");
    }
}

/* ========== 硬件信息 ========== */

/**
 * 获取 PMU 硬件信息
 */
const pmu_hw_info_t* pmu_get_hw_info(void)
{
    return &g_pmu_hw_info;
}

/**
 * 检查事件是否支持
 */
bool pmu_event_supported(pmu_event_type_t event)
{
    /* 基本事件都支持 */
    if (event < PMU_EVENT_RAW_START) {
        return true;
    }
    
    /* TODO: 检查架构特定事件 */
    return false;
}

/* ========== 计数器管理 ========== */

/**
 * 分配计数器
 */
u32 pmu_counter_alloc(void)
{
    /* 尝试架构分配 */
    if (g_pmu_arch_ops.counter_alloc) {
        u32 arch_id = g_pmu_arch_ops.counter_alloc();
        if (arch_id != (u32)-1) {
            /* 查找空闲槽位 */
            for (u32 i = 0; i < PMU_MAX_COUNTERS; i++) {
                if (!g_counter_used[i]) {
                    g_counter_used[i] = true;
                    g_counters[i].counter_id = arch_id;
                    g_counters[i].value = 0;
                    g_counters[i].enabled_time = 0;
                    g_counters[i].running_time = 0;
                    g_counters[i].flags = 0;
                    return i;
                }
            }
            /* 没有空闲槽位，释放架构计数器 */
            if (g_pmu_arch_ops.counter_free) {
                g_pmu_arch_ops.counter_free(arch_id);
            }
        }
    } else {
        /* 无架构支持，使用软件模拟 */
        for (u32 i = 0; i < PMU_MAX_COUNTERS; i++) {
            if (!g_counter_used[i]) {
                g_counter_used[i] = true;
                g_counters[i].counter_id = i;
                g_counters[i].value = 0;
                g_counters[i].enabled_time = 0;
                g_counters[i].running_time = 0;
                g_counters[i].flags = 0;
                return i;
            }
        }
    }
    
    return (u32)-1;  /* 无可用计数器 */
}

/**
 * 释放计数器
 */
void pmu_counter_free(u32 counter_id)
{
    if (counter_id >= PMU_MAX_COUNTERS || !g_counter_used[counter_id]) {
        return;
    }
    
    /* 禁用计数器 */
    pmu_counter_disable(counter_id);
    
    /* 释放架构计数器 */
    if (g_pmu_arch_ops.counter_free) {
        g_pmu_arch_ops.counter_free(g_counters[counter_id].counter_id);
    }
    
    g_counter_used[counter_id] = false;
    g_counters[counter_id].flags = 0;
}

/**
 * 配置计数器
 */
hic_status_t pmu_counter_config(u32 counter_id, const pmu_event_attr_t *attr)
{
    if (counter_id >= PMU_MAX_COUNTERS || !g_counter_used[counter_id]) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (!attr) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查事件是否支持 */
    if (!pmu_event_supported(attr->type)) {
        return HIC_ERROR_NOT_SUPPORTED;
    }
    
    g_counters[counter_id].event_type = attr->type;
    g_counters[counter_id].flags |= PMU_EVENT_FLAG_ENABLED;
    
    /* 架构配置 */
    if (g_pmu_arch_ops.counter_config) {
        g_pmu_arch_ops.counter_config(g_counters[counter_id].counter_id, attr);
    }
    
    return HIC_SUCCESS;
}

/**
 * 启用计数器
 */
void pmu_counter_enable(u32 counter_id)
{
    if (counter_id >= PMU_MAX_COUNTERS || !g_counter_used[counter_id]) {
        return;
    }
    
    g_counters[counter_id].enabled_time = hal_get_timestamp();
    g_counters[counter_id].flags |= PMU_EVENT_FLAG_ENABLED;
    
    if (g_pmu_arch_ops.counter_enable) {
        g_pmu_arch_ops.counter_enable(g_counters[counter_id].counter_id);
    }
}

/**
 * 禁用计数器
 */
void pmu_counter_disable(u32 counter_id)
{
    if (counter_id >= PMU_MAX_COUNTERS || !g_counter_used[counter_id]) {
        return;
    }
    
    g_counters[counter_id].flags &= ~PMU_EVENT_FLAG_ENABLED;
    
    if (g_pmu_arch_ops.counter_disable) {
        g_pmu_arch_ops.counter_disable(g_counters[counter_id].counter_id);
    }
}

/**
 * 读取计数器值
 */
u64 pmu_counter_read(u32 counter_id)
{
    if (counter_id >= PMU_MAX_COUNTERS || !g_counter_used[counter_id]) {
        return 0;
    }
    
    if (g_pmu_arch_ops.counter_read) {
        g_counters[counter_id].value = g_pmu_arch_ops.counter_read(
            g_counters[counter_id].counter_id);
    }
    
    return g_counters[counter_id].value;
}

/**
 * 重置计数器
 */
void pmu_counter_reset(u32 counter_id)
{
    if (counter_id >= PMU_MAX_COUNTERS || !g_counter_used[counter_id]) {
        return;
    }
    
    g_counters[counter_id].value = 0;
    g_counters[counter_id].enabled_time = 0;
    g_counters[counter_id].running_time = 0;
    
    if (g_pmu_arch_ops.counter_write) {
        g_pmu_arch_ops.counter_write(g_counters[counter_id].counter_id, 0);
    }
}

/* ========== 计数器组 ========== */

/**
 * 创建计数器组
 */
u32 pmu_group_create(u32 counter_count, const pmu_event_attr_t *attrs)
{
    if (counter_count == 0 || counter_count > PMU_MAX_COUNTERS || !attrs) {
        return (u32)-1;
    }
    
    /* 查找空闲组 */
    u32 group_id;
    for (group_id = 0; group_id < PMU_MAX_GROUPS; group_id++) {
        if (!g_group_used[group_id]) {
            break;
        }
    }
    
    if (group_id >= PMU_MAX_GROUPS) {
        return (u32)-1;
    }
    
    /* 分配计数器 */
    u32 *counter_ids = (u32*)0;  /* 简化：不实际分配内存 */
    
    for (u32 i = 0; i < counter_count; i++) {
        u32 cid = pmu_counter_alloc();
        if (cid == (u32)-1) {
            /* 回滚 */
            for (u32 j = 0; j < i; j++) {
                pmu_counter_free(g_groups[group_id].counters[j].counter_id);
            }
            return (u32)-1;
        }
        
        pmu_counter_config(cid, &attrs[i]);
    }
    
    g_group_used[group_id] = true;
    g_groups[group_id].group_id = group_id;
    g_groups[group_id].counter_count = counter_count;
    g_groups[group_id].enabled = false;
    g_groups[group_id].enabled_time = 0;
    
    return group_id;
}

/**
 * 销毁计数器组
 */
void pmu_group_destroy(u32 group_id)
{
    if (group_id >= PMU_MAX_GROUPS || !g_group_used[group_id]) {
        return;
    }
    
    /* 禁用并释放计数器 */
    for (u32 i = 0; i < g_groups[group_id].counter_count; i++) {
        pmu_counter_disable(g_groups[group_id].counters[i].counter_id);
        pmu_counter_free(g_groups[group_id].counters[i].counter_id);
    }
    
    g_group_used[group_id] = false;
}

/**
 * 启用计数器组
 */
void pmu_group_enable(u32 group_id)
{
    if (group_id >= PMU_MAX_GROUPS || !g_group_used[group_id]) {
        return;
    }
    
    g_groups[group_id].enabled = true;
    g_groups[group_id].enabled_time = hal_get_timestamp();
    
    for (u32 i = 0; i < g_groups[group_id].counter_count; i++) {
        pmu_counter_enable(g_groups[group_id].counters[i].counter_id);
    }
}

/**
 * 禁用计数器组
 */
void pmu_group_disable(u32 group_id)
{
    if (group_id >= PMU_MAX_GROUPS || !g_group_used[group_id]) {
        return;
    }
    
    g_groups[group_id].enabled = false;
    
    for (u32 i = 0; i < g_groups[group_id].counter_count; i++) {
        pmu_counter_disable(g_groups[group_id].counters[i].counter_id);
    }
}

/**
 * 读取计数器组
 */
void pmu_group_read(u32 group_id, u64 *values, u32 count)
{
    if (group_id >= PMU_MAX_GROUPS || !g_group_used[group_id] || !values) {
        return;
    }
    
    u32 read_count = count < g_groups[group_id].counter_count ? 
                     count : g_groups[group_id].counter_count;
    
    for (u32 i = 0; i < read_count; i++) {
        values[i] = pmu_counter_read(g_groups[group_id].counters[i].counter_id);
    }
}

/* ========== 采样 ========== */

/**
 * 启动采样
 */
hic_status_t pmu_sampling_start(const pmu_sample_config_t *config)
{
    if (!config) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (!g_pmu_hw_info.supports_sampling) {
        return HIC_ERROR_NOT_SUPPORTED;
    }
    
    /* 清空缓冲区 */
    g_sample_buffer.count = 0;
    g_sample_buffer.head = 0;
    g_sample_buffer.tail = 0;
    
    /* 保存配置 */
    memcopy(&g_sample_config, config, sizeof(pmu_sample_config_t));
    
    /* 架构启动采样 */
    if (g_pmu_arch_ops.sampling_start) {
        g_pmu_arch_ops.sampling_start(config);
    }
    
    g_sampling_active = true;
    
    console_puts("[PMU] Sampling started for event ");
    console_putu32(config->event);
    console_puts(", period ");
    console_putu64(config->sample_period);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/**
 * 停止采样
 */
void pmu_sampling_stop(void)
{
    if (!g_sampling_active) {
        return;
    }
    
    if (g_pmu_arch_ops.sampling_stop) {
        g_pmu_arch_ops.sampling_stop();
    }
    
    g_sampling_active = false;
    
    console_puts("[PMU] Sampling stopped, ");
    console_putu32(g_sample_buffer.count);
    console_puts(" records captured\n");
}

/**
 * 读取采样记录
 */
u32 pmu_sampling_read(pmu_sample_record_t *records, u32 max_count)
{
    if (!records || max_count == 0) {
        return 0;
    }
    
    u32 read_count = 0;
    
    while (g_sample_buffer.head != g_sample_buffer.tail && read_count < max_count) {
        records[read_count++] = g_sample_buffer.records[g_sample_buffer.head];
        g_sample_buffer.head = (g_sample_buffer.head + 1) % g_sample_buffer.capacity;
        g_sample_buffer.count--;
    }
    
    return read_count;
}

/**
 * 清空采样缓冲区
 */
void pmu_sampling_clear(void)
{
    g_sample_buffer.count = 0;
    g_sample_buffer.head = 0;
    g_sample_buffer.tail = 0;
}

/* ========== 调试支持 ========== */

/**
 * 设置硬件断点
 */
u32 pmu_breakpoint_set(u64 addr, u64 len, pmu_breakpoint_type_t type)
{
    /* 查找空闲槽位 */
    u32 bp_id;
    for (bp_id = 0; bp_id < PMU_MAX_BREAKPOINTS; bp_id++) {
        if (!g_breakpoint_used[bp_id]) {
            break;
        }
    }
    
    if (bp_id >= PMU_MAX_BREAKPOINTS) {
        return (u32)-1;
    }
    
    /* 架构设置断点 */
    u32 arch_bp_id = bp_id;
    if (g_pmu_arch_ops.breakpoint_set) {
        arch_bp_id = g_pmu_arch_ops.breakpoint_set(addr, len, type);
        if (arch_bp_id == (u32)-1) {
            return (u32)-1;
        }
    }
    
    g_breakpoint_used[bp_id] = true;
    g_breakpoints[bp_id].bp_id = arch_bp_id;
    g_breakpoints[bp_id].type = type;
    g_breakpoints[bp_id].addr = addr;
    g_breakpoints[bp_id].len = len;
    g_breakpoints[bp_id].enabled = true;
    g_breakpoints[bp_id].hit_count = 0;
    
    return bp_id;
}

/**
 * 清除断点
 */
void pmu_breakpoint_clear(u32 bp_id)
{
    if (bp_id >= PMU_MAX_BREAKPOINTS || !g_breakpoint_used[bp_id]) {
        return;
    }
    
    if (g_pmu_arch_ops.breakpoint_clear) {
        g_pmu_arch_ops.breakpoint_clear(g_breakpoints[bp_id].bp_id);
    }
    
    g_breakpoint_used[bp_id] = false;
    g_breakpoints[bp_id].enabled = false;
}

/**
 * 启用断点
 */
void pmu_breakpoint_enable(u32 bp_id)
{
    if (bp_id >= PMU_MAX_BREAKPOINTS || !g_breakpoint_used[bp_id]) {
        return;
    }
    
    g_breakpoints[bp_id].enabled = true;
    
    /* 架构重新设置断点 */
    if (g_pmu_arch_ops.breakpoint_set) {
        g_pmu_arch_ops.breakpoint_set(
            g_breakpoints[bp_id].addr,
            g_breakpoints[bp_id].len,
            g_breakpoints[bp_id].type);
    }
}

/**
 * 禁用断点
 */
void pmu_breakpoint_disable(u32 bp_id)
{
    if (bp_id >= PMU_MAX_BREAKPOINTS || !g_breakpoint_used[bp_id]) {
        return;
    }
    
    g_breakpoints[bp_id].enabled = false;
    
    if (g_pmu_arch_ops.breakpoint_clear) {
        g_pmu_arch_ops.breakpoint_clear(g_breakpoints[bp_id].bp_id);
    }
}

/**
 * 获取断点信息
 */
const pmu_breakpoint_t* pmu_breakpoint_get(u32 bp_id)
{
    if (bp_id >= PMU_MAX_BREAKPOINTS || !g_breakpoint_used[bp_id]) {
        return NULL;
    }
    
    /* 更新命中计数 */
    if (g_pmu_arch_ops.breakpoint_hit) {
        if (g_pmu_arch_ops.breakpoint_hit(g_breakpoints[bp_id].bp_id)) {
            g_breakpoints[bp_id].hit_count++;
        }
    }
    
    return &g_breakpoints[bp_id];
}

/**
 * 获取断点数量
 */
u32 pmu_breakpoint_get_count(void)
{
    u32 count = 0;
    for (u32 i = 0; i < PMU_MAX_BREAKPOINTS; i++) {
        if (g_breakpoint_used[i]) {
            count++;
        }
    }
    return count;
}

/* ========== 跟踪 ========== */

/**
 * 启动跟踪
 */
hic_status_t pmu_trace_start(const pmu_trace_config_t *config)
{
    if (!config) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* TODO: 实现架构相关的跟踪 */
    console_puts("[PMU] Trace started (not implemented)\n");
    
    return HIC_SUCCESS;
}

/**
 * 停止跟踪
 */
void pmu_trace_stop(void)
{
    console_puts("[PMU] Trace stopped\n");
}

/**
 * 读取跟踪数据
 */
u64 pmu_trace_read(void *buffer, u64 size)
{
    (void)buffer;
    (void)size;
    return 0;
}

/* ========== 性能统计 ========== */

/**
 * 获取全局性能统计
 */
void pmu_get_global_stats(u64 *cycles, u64 *instructions, u64 *cache_misses)
{
    if (cycles) {
        *cycles = g_total_cycles;
    }
    if (instructions) {
        *instructions = g_total_instructions;
    }
    if (cache_misses) {
        *cache_misses = g_total_cache_misses;
    }
}

/**
 * 打印性能统计
 */
void pmu_print_stats(void)
{
    console_puts("\n[PMU] ===== Performance Statistics =====\n");
    
    console_puts("[PMU] Total cycles: ");
    console_putu64(g_total_cycles);
    console_puts("\n");
    
    console_puts("[PMU] Total instructions: ");
    console_putu64(g_total_instructions);
    console_puts("\n");
    
    if (g_total_instructions > 0) {
        console_puts("[PMU] IPC: ");
        u64 ipc_x100 = (g_total_instructions * 100) / g_total_cycles;
        console_putu64(ipc_x100 / 100);
        console_puts(".");
        console_putu64(ipc_x100 % 100);
        console_puts("\n");
    }
    
    console_puts("[PMU] Cache misses: ");
    console_putu64(g_total_cache_misses);
    console_puts("\n");
    
    console_puts("[PMU] Active counters: ");
    u32 active = 0;
    for (u32 i = 0; i < PMU_MAX_COUNTERS; i++) {
        if (g_counter_used[i]) {
            active++;
        }
    }
    console_putu32(active);
    console_puts("\n");
    
    console_puts("[PMU] Breakpoints: ");
    console_putu32(pmu_breakpoint_get_count());
    console_puts("\n");
    
    console_puts("[PMU] ===================================\n\n");
}

/* ========== 事件映射 ========== */

/* 架构特定函数声明 */
extern u64 arch_pmu_event_to_config(pmu_event_type_t event);
extern pmu_event_type_t arch_pmu_config_to_event(u64 arch_config);

/**
 * 标准事件到架构配置映射
 */
u64 pmu_event_to_arch_config(pmu_event_type_t event)
{
    return arch_pmu_event_to_config(event);
}

/**
 * 架构配置到标准事件映射
 */
pmu_event_type_t pmu_arch_config_to_event(u64 arch_config)
{
    return arch_pmu_config_to_event(arch_config);
}
