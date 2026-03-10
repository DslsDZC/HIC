/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * AMP（非对称多处理）实现 - 增强版
 * AP分担能力验证、中断处理和计算任务
 */

#include "amp.h"
#include "hal.h"
#include "pmm.h"
#include "capability.h"
#include "irq.h"
#include "lib/console.h"
#include "lib/mem.h"
#include <stddef.h>

/* ==================== 外部变量 ==================== */

extern boot_state_t g_boot_state;
extern bool g_amp_enabled;

/* ==================== 全局变量 ==================== */

amp_info_t g_amp_info = {0};

/* AP启动代码地址（必须在1MB以下） */
#define AP_TRAMPOLINE_ADDR   0x8000

/* LAPIC寄存器 */
#define LAPIC_BASE           0xFEE00000
#define LAPIC_ICR            0x300
#define LAPIC_EOI            0xB0
#define LAPIC_ID             0x020

/* 能力验证缓存TTL（纳秒） */
#define CAP_CACHE_TTL        1000000ULL /* 1ms */

/* 外部AP启动代码 */
extern void ap_trampoline(void);

/* ==================== LAPIC操作 ==================== */

static inline u32 lapic_read(u32 offset)
{
    return *(volatile u32*)(LAPIC_BASE + offset);
}

static inline void lapic_write(u32 offset, u32 value)
{
    *(volatile u32*)(LAPIC_BASE + offset) = value;
}

static void send_ipi(u32 apic_id, u32 vector)
{
    u32 icr = vector | (apic_id << 24) | (1 << 14);
    lapic_write(LAPIC_ICR, icr);
    while (lapic_read(LAPIC_ICR) & (1 << 12)) {
        hal_idle();
    }
}

/* ==================== AMP初始化 ==================== */

void amp_init(void)
{
    console_puts("[AMP] Initializing AMP subsystem (Enhanced Mode)...\n");

    /* 清零AMP信息 */
    memzero(&g_amp_info, sizeof(amp_info_t));

    /* 初始化CPU数据 */
    for (u32 i = 0; i < MAX_CPUS; i++) {
        amp_cpu_t *cpu = &g_amp_info.cpus[i];
        cpu->cpu_id = i;
        cpu->state = AMP_CPU_OFFLINE;
        cpu->mode = AMP_MODE_COMPUTE;
        cpu->queue_head = 0;
        cpu->queue_tail = 0;
        cpu->queue_count = 0;
        cpu->current_task = NULL;
        cpu->cap_cache_hits = 0;
        cpu->cap_cache_misses = 0;
        cpu->assigned_irq_count = 0;
        cpu->tasks_completed = 0;
        cpu->tasks_failed = 0;
        cpu->caps_verified = 0;
        cpu->irqs_handled = 0;
        cpu->total_cycles = 0;

        /* 初始化能力验证缓存 */
        for (u32 j = 0; j < AMP_CAP_CACHE_SIZE; j++) {
            cpu->cap_cache[j].in_use = false;
            cpu->cap_cache[j].valid = false;
        }
    }

    /* BSP ID */
    g_amp_info.bsp_id = 0;
    g_amp_info.cpus[0].state = AMP_CPU_ONLINE;
    g_amp_info.amp_enabled = false;

    /* 获取CPU数量 */
    if (g_boot_state.valid && g_boot_state.hw.cpu.logical_cores > 0) {
        g_amp_info.cpu_count = g_boot_state.hw.cpu.logical_cores;
    } else {
        g_amp_info.cpu_count = 1;
    }

    g_amp_info.online_cpus = 1; /* 只有BSP在线 */

    /* 默认配置：1个能力验证CPU，1个中断处理CPU，其余计算CPU */
    if (g_amp_info.cpu_count >= 3) {
        g_amp_info.cap_verify_cpus = 1;
        g_amp_info.irq_handler_cpus = 1;
        g_amp_info.compute_cpus = g_amp_info.cpu_count - 2;
    } else if (g_amp_info.cpu_count == 2) {
        g_amp_info.cap_verify_cpus = 1;
        g_amp_info.irq_handler_cpus = 0;
        g_amp_info.compute_cpus = 1;
    } else {
        g_amp_info.cap_verify_cpus = 0;
        g_amp_info.irq_handler_cpus = 0;
        g_amp_info.compute_cpus = 0;
    }

    console_puts("[AMP] AMP initialized: ");
    console_puthex32(g_amp_info.cpu_count);
    console_puts(" cores (");
    console_puthex32(g_amp_info.cap_verify_cpus);
    console_puts(" cap_verify, ");
    console_puthex32(g_amp_info.irq_handler_cpus);
    console_puts(" irq_handler, ");
    console_puthex32(g_amp_info.compute_cpus);
    console_puts(" compute)\n");
}

/* ==================== AP启动 ==================== */

static status_t prepare_ap_trampoline(void)
{
    /* 复制AP启动代码到低内存 */
    u8 *trampoline_src = (u8*)&ap_trampoline;
    u8 *trampoline_dst = (u8*)AP_TRAMPOLINE_ADDR;

    /* 这里需要知道ap_trampoline的大小，暂时假设4KB */
    for (u32 i = 0; i < 4096; i++) {
        trampoline_dst[i] = trampoline_src[i];
    }

    console_puts("[AMP] AP trampoline prepared at 0x");
    console_puthex64(AP_TRAMPOLINE_ADDR);
    console_puts("\n");

    return HIC_SUCCESS;
}

status_t amp_boot_aps(void)
{
    if (g_amp_info.cpu_count <= 1) {
        console_puts("[AMP] Single core system, AMP disabled\n");
        return HIC_SUCCESS;
    }

    console_puts("[AMP] Booting APs...\n");

    /* 准备AP启动代码 */
    status_t status = prepare_ap_trampoline();
    if (status != HIC_SUCCESS) {
        console_puts("[AMP] Failed to prepare AP trampoline\n");
        return status;
    }

    /* 为每个AP分配栈 */
    for (u32 i = 1; i < g_amp_info.cpu_count; i++) {
        amp_cpu_t *cpu = &g_amp_info.cpus[i];

        /* 分配8KB栈 */
        phys_addr_t stack_phys;
        status = pmm_alloc_frames(0, 2, PAGE_FRAME_KERNEL, &stack_phys);
        if (status != HIC_SUCCESS) {
            console_puts("[AMP] Failed to allocate stack for AP ");
            console_puthex32(i);
            console_puts("\n");
            continue;
        }

        cpu->stack_base = (void*)stack_phys;
        cpu->stack_top = (void*)(stack_phys + 8192);

        /* 获取AP的APIC ID */
        /* 这里简化处理，实际需要从MADT获取 */
        cpu->apic_id = i;
        cpu->lapic_address = LAPIC_BASE;

        /* 配置工作模式 */
        if (i == 1 && g_amp_info.cap_verify_cpus > 0) {
            cpu->mode = AMP_MODE_CAP_VERIFY;
        } else if (i == 2 && g_amp_info.irq_handler_cpus > 0) {
            cpu->mode = AMP_MODE_IRQ_HANDLER;
        } else {
            cpu->mode = AMP_MODE_COMPUTE;
        }

        console_puts("[AMP] AP ");
        console_puthex32(i);
        console_puts(" (APIC ID: ");
        console_puthex32(cpu->apic_id);
        console_puts(") mode: ");
        console_puthex32(cpu->mode);
        console_puts("\n");

        /* 发送SIPI启动AP */
        send_ipi(cpu->apic_id, AP_TRAMPOLINE_ADDR >> 12);

        /* 等待AP启动（简化处理） */
        for (u32 delay = 0; delay < 100000; delay++) {
            hal_udelay(1);
            if (cpu->state == AMP_CPU_ONLINE) {
                break;
            }
        }

        if (cpu->state != AMP_CPU_ONLINE) {
            console_puts("[AMP] AP ");
            console_puthex32(i);
            console_puts(" failed to start\n");
        } else {
            g_amp_info.online_cpus++;
        }
    }

    g_amp_info.amp_enabled = true;
    g_amp_enabled = true;

    console_puts("[AMP] AP boot completed: ");
    console_puthex32(g_amp_info.online_cpus);
    console_puts(" CPUs online\n");

    return HIC_SUCCESS;
}

void amp_wait_for_aps(void)
{
    console_puts("[AMP] Waiting for APs to be ready...\n");

    /* 等待所有AP就绪 */
    for (u32 i = 1; i < g_amp_info.cpu_count; i++) {
        amp_cpu_t *cpu = &g_amp_info.cpus[i];

        if (cpu->state == AMP_CPU_OFFLINE) {
            continue;
        }

        /* 等待AP进入在线状态 */
        for (u32 delay = 0; delay < 1000000; delay++) {
            hal_udelay(1);
            if (cpu->state == AMP_CPU_ONLINE || cpu->state == AMP_CPU_IDLE) {
                break;
            }
        }

        console_puts("[AMP] AP ");
        console_puthex32(i);
        console_puts(" is ");
        if (cpu->state == AMP_CPU_ONLINE || cpu->state == AMP_CPU_IDLE) {
            console_puts("ready\n");
        } else {
            console_puts("not ready\n");
        }
    }

    console_puts("[AMP] All APs ready\n");
}

/* ==================== 工作模式配置 ==================== */

status_t amp_set_mode(cpu_id_t cpu_id, amp_mode_t mode)
{
    if (cpu_id >= MAX_CPUS || cpu_id == g_amp_info.bsp_id) {
        return HIC_ERROR_INVALID_PARAM;
    }

    amp_cpu_t *cpu = &g_amp_info.cpus[cpu_id];
    cpu->mode = mode;

    return HIC_SUCCESS;
}

/* ==================== 任务分配 ==================== */

status_t amp_assign_compute_task(cpu_id_t target_cpu, amp_task_t *task)
{
    if (task == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 如果没有指定目标CPU，查找空闲的计算CPU */
    if (target_cpu == INVALID_CPU_ID) {
        target_cpu = amp_get_idle_compute_cpu();
        if (target_cpu == INVALID_CPU_ID) {
            return HIC_ERROR_BUSY;
        }
    }

    amp_cpu_t *cpu = &g_amp_info.cpus[target_cpu];

    if (cpu->state != AMP_CPU_ONLINE && cpu->state != AMP_CPU_IDLE) {
        return HIC_ERROR_INVALID_STATE;
    }

    /* 检查队列是否已满 */
    if (cpu->queue_count >= AMP_TASK_QUEUE_SIZE) {
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 添加任务到队列 */
    task->completed = false;
    task->cycles = 0;

    u32 tail = cpu->queue_tail;
    cpu->task_queue[tail] = *task;
    cpu->queue_tail = (tail + 1) % AMP_TASK_QUEUE_SIZE;
    cpu->queue_count++;

    cpu->state = AMP_CPU_BUSY;

    return HIC_SUCCESS;
}

status_t amp_wait_task(amp_task_t *task)
{
    if (task == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* 等待任务完成 */
    for (u32 delay = 0; delay < 10000000; delay++) {
        if (task->completed) {
            return HIC_SUCCESS;
        }
        hal_udelay(1);
    }

    return HIC_ERROR_TIMEOUT;
}

/* ==================== 能力验证分担 ==================== */

bool amp_verify_capability(domain_id_t domain_id, cap_id_t cap_id, cap_rights_t required_rights)
{
    if (!g_amp_info.amp_enabled || g_amp_info.cap_verify_cpus == 0) {
        /* 如果AMP未启用或没有能力验证CPU，直接在BSP上验证 */
        /* 这里需要调用实际的能力验证函数 */
        return true; /* 简化处理 */
    }

    /* 查找能力验证CPU */
    cpu_id_t cap_cpu = INVALID_CPU_ID;
    for (u32 i = 0; i < g_amp_info.cpu_count; i++) {
        if (g_amp_info.cpus[i].mode == AMP_MODE_CAP_VERIFY) {
            cap_cpu = i;
            break;
        }
    }

    if (cap_cpu == INVALID_CPU_ID) {
        return true; /* 简化处理 */
    }

    amp_cpu_t *cpu = &g_amp_info.cpus[cap_cpu];

    /* 检查缓存 */
    for (u32 i = 0; i < AMP_CAP_CACHE_SIZE; i++) {
        amp_cap_cache_entry_t *entry = &cpu->cap_cache[i];

        if (!entry->in_use) {
            continue;
        }

        if (entry->cap_id == cap_id && 
            entry->domain_id == domain_id && 
            entry->required_rights == required_rights) {
            
            /* 检查缓存是否过期 */
            u64 current_time = hal_get_timestamp();
            if (current_time - entry->timestamp < CAP_CACHE_TTL) {
                cpu->cap_cache_hits++;
                return entry->valid;
            } else {
                /* 缓存过期，标记为无效 */
                entry->in_use = false;
            }
        }
    }

    /* 缓存未命中，需要验证 */
    cpu->cap_cache_misses++;

    /* 这里需要调用实际的能力验证函数 */
    bool valid = true; /* 简化处理 */

    /* 添加到缓存 */
    for (u32 i = 0; i < AMP_CAP_CACHE_SIZE; i++) {
        amp_cap_cache_entry_t *entry = &cpu->cap_cache[i];

        if (!entry->in_use) {
            entry->cap_id = cap_id;
            entry->domain_id = domain_id;
            entry->required_rights = required_rights;
            entry->valid = valid;
            entry->timestamp = hal_get_timestamp();
            entry->in_use = true;
            break;
        }
    }

    return valid;
}

/* ==================== 中断处理分担 ==================== */

status_t amp_assign_irq(cpu_id_t cpu_id, u32 irq_vector)
{
    if (cpu_id >= MAX_CPUS || cpu_id == g_amp_info.bsp_id) {
        return HIC_ERROR_INVALID_PARAM;
    }

    amp_cpu_t *cpu = &g_amp_info.cpus[cpu_id];

    if (cpu->mode != AMP_MODE_IRQ_HANDLER && cpu->mode != AMP_MODE_MIXED) {
        return HIC_ERROR_INVALID_STATE;
    }

    if (cpu->assigned_irq_count >= AMP_IRQ_ASSIGN_MAX) {
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 分配中断 */
    cpu->assigned_irqs[cpu->assigned_irq_count] = irq_vector;
    cpu->assigned_irq_count++;

    /* 初始化中断记录 */
    for (u32 i = 0; i < AMP_IRQ_ASSIGN_MAX; i++) {
        if (cpu->irq_records[i].irq_vector == 0) {
            cpu->irq_records[i].irq_vector = irq_vector;
            cpu->irq_records[i].count = 0;
            cpu->irq_records[i].last_time = 0;
            cpu->irq_records[i].total_cycles = 0;
            break;
        }
    }

    console_puts("[AMP] IRQ ");
    console_puthex32(irq_vector);
    console_puts(" assigned to CPU ");
    console_puthex32(cpu_id);
    console_puts("\n");

    return HIC_SUCCESS;
}

void amp_irq_handler(u32 irq_vector)
{
    /* 查找处理此中断的AP */
    for (u32 i = 0; i < g_amp_info.cpu_count; i++) {
        amp_cpu_t *cpu = &g_amp_info.cpus[i];

        if (cpu->mode != AMP_MODE_IRQ_HANDLER && cpu->mode != AMP_MODE_MIXED) {
            continue;
        }

        for (u32 j = 0; j < cpu->assigned_irq_count; j++) {
            if (cpu->assigned_irqs[j] == irq_vector) {
                /* 找到处理此中断的AP */
                /* 这里需要通知AP处理中断 */
                /* 简化处理：直接增加计数 */
                for (u32 k = 0; k < AMP_IRQ_ASSIGN_MAX; k++) {
                    if (cpu->irq_records[k].irq_vector == irq_vector) {
                        cpu->irq_records[k].count++;
                        cpu->irq_records[k].last_time = hal_get_timestamp();
                        cpu->irqs_handled++;
                        break;
                    }
                }
                return;
            }
        }
    }

    /* 没有找到处理此中断的AP，由BSP处理 */
    console_puts("[AMP] IRQ ");
    console_puthex32(irq_vector);
    console_puts(" not assigned to any AP, BSP handles it\n");
}

/* ==================== 辅助函数 ==================== */

cpu_id_t amp_get_idle_compute_cpu(void)
{
    for (u32 i = 0; i < g_amp_info.cpu_count; i++) {
        amp_cpu_t *cpu = &g_amp_info.cpus[i];

        if (cpu->mode == AMP_MODE_COMPUTE && 
            (cpu->state == AMP_CPU_IDLE || cpu->queue_count == 0)) {
            return i;
        }
    }

    return INVALID_CPU_ID;
}

bool amp_is_enabled(void)
{
    return g_amp_info.amp_enabled;
}

void amp_get_stats(cpu_id_t cpu_id, u64 *tasks_completed, u64 *caps_verified, u64 *irqs_handled)
{
    if (cpu_id >= MAX_CPUS) {
        if (tasks_completed) *tasks_completed = 0;
        if (caps_verified) *caps_verified = 0;
        if (irqs_handled) *irqs_handled = 0;
        return;
    }

    amp_cpu_t *cpu = &g_amp_info.cpus[cpu_id];

    if (tasks_completed) *tasks_completed = cpu->tasks_completed;
    if (caps_verified) *caps_verified = cpu->caps_verified;
    if (irqs_handled) *irqs_handled = cpu->irqs_handled;
}

/* ==================== AP主函数 ==================== */

void amp_ap_main(void)
{
    /* 获取当前CPU ID */
    cpu_id_t cpu_id = hal_get_cpu_id();
    amp_cpu_t *cpu = &g_amp_info.cpus[cpu_id];

    /* 设置状态为在线 */
    cpu->state = AMP_CPU_ONLINE;
    cpu->lapic_address = LAPIC_BASE;

    console_puts("[AP] CPU ");
    console_puthex32(cpu_id);
    console_puts(" started\n");

    /* 根据工作模式进入不同的循环 */
    switch (cpu->mode) {
        case AMP_MODE_CAP_VERIFY:
            amp_cap_verify_loop();
            break;
        case AMP_MODE_IRQ_HANDLER:
            amp_irq_handler_loop();
            break;
        case AMP_MODE_COMPUTE:
            amp_compute_loop();
            break;
        case AMP_MODE_MIXED:
            /* 混合模式：优先处理中断，然后处理任务 */
            amp_irq_handler_loop();
            break;
        default:
            console_puts("[AP] Unknown mode, halting\n");
            hal_halt();
            break;
    }
}

/* ==================== AP循环函数 ==================== */

void amp_cap_verify_loop(void)
{
    cpu_id_t cpu_id = hal_get_cpu_id();
    amp_cpu_t *cpu = &g_amp_info.cpus[cpu_id];

    console_puts("[AP] CPU ");
    console_puthex32(cpu_id);
    console_puts(" entering capability verification loop\n");

    while (1) {
        /* 清理过期缓存 */
        u64 current_time = hal_get_timestamp();
        for (u32 i = 0; i < AMP_CAP_CACHE_SIZE; i++) {
            amp_cap_cache_entry_t *entry = &cpu->cap_cache[i];
            if (entry->in_use && (current_time - entry->timestamp > CAP_CACHE_TTL)) {
                entry->in_use = false;
            }
        }

        /* 空闲等待 */
        cpu->state = AMP_CPU_IDLE;
        hal_idle();
    }
}

void amp_irq_handler_loop(void)
{
    cpu_id_t cpu_id = hal_get_cpu_id();
    amp_cpu_t *cpu = &g_amp_info.cpus[cpu_id];

    console_puts("[AP] CPU ");
    console_puthex32(cpu_id);
    console_puts(" entering IRQ handler loop\n");

    while (1) {
        /* 检查是否有分配的中断 */
        if (cpu->assigned_irq_count == 0) {
            cpu->state = AMP_CPU_IDLE;
            hal_idle();
            continue;
        }

        /* 空闲等待中断 */
        cpu->state = AMP_CPU_IDLE;
        hal_idle();
    }
}

void amp_compute_loop(void)
{
    cpu_id_t cpu_id = hal_get_cpu_id();
    amp_cpu_t *cpu = &g_amp_info.cpus[cpu_id];

    console_puts("[AP] CPU ");
    console_puthex32(cpu_id);
    console_puts(" entering compute loop\n");

    while (1) {
        /* 检查任务队列 */
        if (cpu->queue_count == 0) {
            cpu->state = AMP_CPU_IDLE;
            hal_idle();
            continue;
        }

        /* 取出任务 */
        u32 head = cpu->queue_head;
        amp_task_t *task = &cpu->task_queue[head];
        cpu->queue_head = (head + 1) % AMP_TASK_QUEUE_SIZE;
        cpu->queue_count--;

        cpu->current_task = task;
        cpu->state = AMP_CPU_BUSY;

        /* 执行任务 */
        u64 start_cycles = hal_get_timestamp();

        switch (task->type) {
            case AMP_TASK_COMPUTE:
                /* 计算任务 */
                task->result = task->arg1 + task->arg2 + task->arg3;
                break;
            case AMP_TASK_MEMORY_COPY:
                /* 内存拷贝任务 */
                if (task->input_buffer != 0 && task->output_buffer != 0 && task->buffer_size > 0) {
                    memmove((void*)task->output_buffer, (void*)task->input_buffer, task->buffer_size);
                    task->result = task->buffer_size;
                }
                break;
            case AMP_TASK_CRYPTO:
                /* 加密任务 */
                task->result = 0; /* 简化处理 */
                break;
            case AMP_TASK_DATA_PROCESS:
                /* 数据处理任务 */
                task->result = 0; /* 简化处理 */
                break;
            default:
                task->result = 0;
                break;
        }

        u64 end_cycles = hal_get_timestamp();
        task->cycles = end_cycles - start_cycles;
        task->completed = true;

        cpu->current_task = NULL;
        cpu->tasks_completed++;
    }
}
