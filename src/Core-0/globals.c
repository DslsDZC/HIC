/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核全局变量
 */

#include "types.h"
#include "lib/console.h"

/* 系统调用队列状态 */
bool g_syscall_queue_empty = true;

/* 系统调用队列 */
static struct {
    void (*handler)(void);
    u64 args[4];
} g_syscall_queue[256];
static u32 g_syscall_queue_head = 0;
static u32 g_syscall_queue_tail = 0;

/* 性能统计中的定时器计数 */
u64 g_timer_ticks = 0;

/* 检查系统调用队列是否为空 */
bool syscall_queue_empty(void)
{
    return g_syscall_queue_head == g_syscall_queue_tail;
}

/* 处理所有系统调用 */
void g_syscall_process_all(void)
{
    while (g_syscall_queue_head != g_syscall_queue_tail) {
        void (*handler)(void) = g_syscall_queue[g_syscall_queue_head].handler;
        
        if (handler) {
            handler();
        }
        
        g_syscall_queue_head = (g_syscall_queue_head + 1) % 256;
    }
    
    g_syscall_queue_empty = true;
}

/* 添加系统调用到队列 */
void syscall_queue_add(void (*handler)(void))
{
    g_syscall_queue[g_syscall_queue_tail].handler = handler;
    g_syscall_queue_tail = (g_syscall_queue_tail + 1) % 256;
    g_syscall_queue_empty = false;
}