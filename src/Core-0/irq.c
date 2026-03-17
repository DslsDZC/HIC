/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 中断控制器 - 静态为主、动态为辅实现
 *
 * 核心设计：
 * - 构建时静态路由，固定设备零查找
 * - 动态中断池，热插拔设备可扩展
 * - 中断直接送达服务，Core-0 仅异常介入
 * - 无优先级/亲和性管理，硬件静态配置
 *
 * 静态设备延迟目标：<0.5μs
 * 动态设备延迟目标：<1μs
 */

#include "irq.h"
#include "capability.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "boot_info.h"
#include "build_config.h"

/* 外部变量 */
extern boot_state_t g_boot_state;

/* ===== 中断路由表（构建时静态生成） ===== */

volatile irq_route_entry_t irq_table[256];

/* ===== 动态中断池管理（无锁设计） ===== */

/* 动态池分配位图 */
static volatile u32 g_dynamic_bitmap[IRQ_DYNAMIC_COUNT / 32 + 1];

/* 动态池统计 */
static volatile u32 g_dynamic_allocated = 0;
static volatile u32 g_dynamic_high_watermark = 0;

/* 无锁位图操作 */

/**
 * 原子测试并设置位
 * @return 0 表示之前为空，成功分配；非0 表示之前已设置
 */
static inline u32 atomic_bts(volatile u32 *addr, u32 bit)
{
    u32 mask = 1U << bit;
    u32 old;
    
    /* 使用原子交换实现无锁测试并设置 */
    do {
        old = *addr;
        if (old & mask) {
            return 1;  /* 已被占用 */
        }
    } while (!__sync_bool_compare_and_swap(addr, old, old | mask));
    
    return 0;  /* 成功设置 */
}

/**
 * 原子清除位
 */
static inline void atomic_btr(volatile u32 *addr, u32 bit)
{
    u32 mask = 1U << bit;
    u32 old;
    
    do {
        old = *addr;
    } while (!__sync_bool_compare_and_swap(addr, old, old & ~mask));
}

/**
 * 原子递增（带上限检查）
 */
static inline u32 atomic_inc_if_below(volatile u32 *addr, u32 limit)
{
    u32 old, new_val;
    
    do {
        old = *addr;
        if (old >= limit) {
            return old;
        }
        new_val = old + 1;
    } while (!__sync_bool_compare_and_swap(addr, old, new_val));
    
    return new_val;
}

/* ===== 初始化 ===== */

void irq_controller_init(void)
{
    console_puts("[IRQ] Init: static routing + dynamic pool\n");
    
    /* 清空路由表 */
    memzero((void*)irq_table, sizeof(irq_table));
    
    /* 初始化动态池位图 */
    memzero((void*)g_dynamic_bitmap, sizeof(g_dynamic_bitmap));
    g_dynamic_allocated = 0;
    g_dynamic_high_watermark = 0;
    
    /* 从构建配置加载静态路由 */
    for (u32 i = 0; i < g_build_config.num_interrupt_routes; i++) {
        interrupt_route_t *route = &g_build_config.interrupt_routes[i];
        u32 vector = route->irq_vector;
        
        /* 只加载静态范围的路由 */
        if (vector >= IRQ_STATIC_START && vector <= IRQ_STATIC_END) {
            irq_table[vector].handler_address = route->handler_address;
            irq_table[vector].endpoint_cap = route->endpoint_cap;
            irq_table[vector].trigger_type = 
                (route->flags & IRQ_FLAG_LEVEL) ? IRQ_TRIGGER_LEVEL : IRQ_TRIGGER_EDGE;
            irq_table[vector].is_dynamic = 0;
            irq_table[vector].initialized = 1;
        }
    }
    
    /* 标记动态池范围为动态 */
    for (u32 v = IRQ_DYNAMIC_START; v <= IRQ_DYNAMIC_END; v++) {
        irq_table[v].is_dynamic = 1;
        irq_table[v].initialized = 0;
    }
    
    /* 内存屏障确保写入可见 */
    atomic_full_barrier();
    
    console_puts("[IRQ] Static routes: ");
    console_putu32(g_build_config.num_interrupt_routes);
    console_puts(", Dynamic pool: ");
    console_putu32(IRQ_DYNAMIC_COUNT);
    console_puts(" vectors\n");
}

/* ===== 硬件控制 ===== */

static void ioapic_mask_unmask(u32 vector, bool mask)
{
    u32 irq = vector - 32;
    volatile u32* ioapic_base = (volatile u32*)(g_boot_state.hw.io_irq.base_address);
    
    if (!ioapic_base) return;
    
    u32 index = irq * 2;
    
    ioapic_base[0] = index;
    u32 low = ioapic_base[4];
    
    if (mask) {
        low |= 0x00010000;
    } else {
        low &= 0xFFFEFFFF;
        
        if (irq_table[vector].trigger_type == IRQ_TRIGGER_LEVEL) {
            low |= 0x00008000;
        } else {
            low &= 0xFFFF7FFF;
        }
    }
    
    ioapic_base[0] = index;
    ioapic_base[4] = low;
}

void irq_enable(u32 vector)
{
    if (vector >= 32 && vector < 256) {
        ioapic_mask_unmask(vector, false);
    }
}

void irq_disable(u32 vector)
{
    if (vector >= 32 && vector < 256) {
        ioapic_mask_unmask(vector, true);
    }
}

/* ===== 动态中断管理（无锁） ===== */

u32 irq_dynamic_alloc(void)
{
    u32 vector = (u32)-1;
    
    /* 无锁扫描动态池位图 */
    for (u32 i = 0; i < IRQ_DYNAMIC_COUNT; i++) {
        u32 word_idx = i / 32;
        u32 bit_idx = i % 32;
        
        /* 原子测试并设置 */
        if (atomic_bts(&g_dynamic_bitmap[word_idx], bit_idx) == 0) {
            /* 成功分配 */
            vector = IRQ_DYNAMIC_START + i;
            
            /* 原子更新统计 */
            u32 allocated = __sync_add_and_fetch(&g_dynamic_allocated, 1);
            
            /* 更新高水位（允许竞态，不影响正确性） */
            if (allocated > g_dynamic_high_watermark) {
                g_dynamic_high_watermark = allocated;
            }
            break;
        }
    }
    
    return vector;
}

void irq_dynamic_free(u32 vector)
{
    if (!irq_is_dynamic(vector)) {
        return;
    }
    
    u32 index = vector - IRQ_DYNAMIC_START;
    u32 word_idx = index / 32;
    u32 bit_idx = index % 32;
    
    /* 原子清除位 */
    atomic_btr(&g_dynamic_bitmap[word_idx], bit_idx);
    
    /* 清除路由表（单次写入，无需原子） */
    irq_table[vector].initialized = 0;
    irq_table[vector].handler_address = 0;
    irq_table[vector].endpoint_cap = CAP_HANDLE_INVALID;
    
    /* 原子更新统计 */
    __sync_sub_and_fetch(&g_dynamic_allocated, 1);
    
    /* 禁用硬件中断 */
    irq_disable(vector);
}

bool irq_dynamic_register(u32 vector, u64 handler, 
                          cap_handle_t cap, u8 trigger_type)
{
    /* 仅允许注册动态池范围 */
    if (!irq_is_dynamic(vector)) {
        console_puts("[IRQ] Reject: vector ");
        console_putu32(vector);
        console_puts(" not in dynamic pool\n");
        return false;
    }
    
    /* 检查是否已分配 */
    u32 index = vector - IRQ_DYNAMIC_START;
    u32 word_idx = index / 32;
    u32 bit_idx = index % 32;
    
    /* 检查位是否已设置 */
    if (!(g_dynamic_bitmap[word_idx] & (1U << bit_idx))) {
        return false;  /* 未分配 */
    }
    
    /* 设置路由（顺序写入，内存屏障保证可见性） */
    irq_table[vector].handler_address = handler;
    irq_table[vector].endpoint_cap = cap;
    irq_table[vector].trigger_type = trigger_type;
    
    /* 内存屏障确保写入顺序 */
    __sync_synchronize();
    
    /* 最后设置 initialized 标志 */
    irq_table[vector].initialized = 1;
    
    /* 启用硬件中断 */
    irq_enable(vector);
    
    console_puts("[IRQ] Dynamic route registered: vector ");
    console_putu32(vector);
    console_puts("\n");
    
    return true;
}

void irq_dynamic_unregister(u32 vector)
{
    if (!irq_is_dynamic(vector)) {
        return;
    }
    
    /* 先清除 initialized 标志（阻止新中断进入） */
    irq_table[vector].initialized = 0;
    
    /* 内存屏障 */
    __sync_synchronize();
    
    /* 清除其他字段 */
    irq_table[vector].handler_address = 0;
    
    irq_disable(vector);
}

void irq_dynamic_get_stats(irq_dynamic_stats_t *stats)
{
    if (!stats) return;
    
    /* 读取原子变量（可能不一致，但足够用于监控） */
    stats->total_count = IRQ_DYNAMIC_COUNT;
    stats->allocated_count = g_dynamic_allocated;
    stats->free_count = IRQ_DYNAMIC_COUNT - g_dynamic_allocated;
    stats->high_watermark = g_dynamic_high_watermark;
}

/* ===== 分发（核心路径） ===== */

void irq_dispatch(u32 vector)
{
    volatile irq_route_entry_t *entry = &irq_table[vector];
    
    /* 快速检查：路由是否有效 */
    if (!entry->initialized || entry->handler_address == 0) {
        return;
    }
    
    /* 快速能力验证 */
    if (entry->endpoint_cap != CAP_HANDLE_INVALID && 
        !cap_fast_check_rights(entry->endpoint_cap, 0)) {
        return;
    }
    
    /* 直接跳转到服务入口点 */
    typedef void (*handler_t)(void);
    ((handler_t)entry->handler_address)();
    
    /* 发送 EOI */
    if (vector >= 32 && vector < 256) {
        volatile u32* lapic = (volatile u32*)0xFEE00000;
        lapic[0xB0 / 4] = 0;
    }
}
