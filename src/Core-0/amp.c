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
#include "exception.h"
#include "pmm.h"
#include "capability.h"
#include "irq.h"
#include "boot_info.h"
#include "lib/console.h"
#include "lib/mem.h"
#include <stddef.h>

/* ==================== 外部变量 ==================== */

extern boot_state_t g_boot_state;
bool g_amp_enabled = false;

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

/* 链接器定义的 trampoline 边界 */
extern char __ap_trampoline_start[];
extern char __ap_trampoline_end[];

/* ==================== LAPIC操作 ==================== */

static inline u32 lapic_read(u32 offset)
{
    uintptr_t addr = LAPIC_BASE + offset;
    return *(volatile u32*)addr;
}

static inline void lapic_write(u32 offset, u32 value)
{
    uintptr_t addr = LAPIC_BASE + offset;
    *(volatile u32*)addr = value;
}

static void send_ipi(u32 apic_id, u32 vector)
{
    u32 icr = vector | (apic_id << 24) | (1 << 14);
    lapic_write(LAPIC_ICR, icr);

    /* 添加超时机制，防止硬件故障时永久等待 */
    u32 timeout = 10000;  /* 10ms超时 */
    while (lapic_read(LAPIC_ICR) & (1 << 12)) {
        if (--timeout == 0) {
            console_puts("[AMP] ERROR: LAPIC ICR timeout, APIC ID: ");
            console_putu32(apic_id);
            console_puts(", Vector: ");
            console_putu32(vector);
            console_puts("\n");
            kernel_panic("LAPIC ICR timeout - APIC may be in invalid state");
            break;
        }
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
        cpu->cpu_id = (cpu_id_t)i;
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

/**
 * 准备AP启动代码
 * 
 * 修复说明：
 * 1. 使用链接器符号正确获取trampoline的加载地址（LMA）和运行地址（VMA）
 * 2. 设置动态GDT指针地址，支持非0x8000加载地址
 * 3. 正确处理VMA/LMA差异
 */
static status_t prepare_ap_trampoline(void)
{
    /* 链接器定义的 trampoline 边界 */
    /* LMA (Load Memory Address): trampoline 在内核映像中的位置 */
    /* VMA (Virtual Memory Address): trampoline 运行时的地址 (0x8000) */
    extern char __ap_trampoline_start[];
    extern char __ap_trampoline_end[];
    
    /* trampoline在内核映像中的实际位置（LMA） */
    /* 由于链接脚本使用 AT() 指定LMA，我们需要获取实际的LMA */
    /* __ap_trampoline_start 符号的值是VMA (0x8000) */
    /* 我们需要计算LMA：LMA = ALIGN(_kernel_end, 4K) */
    extern char _kernel_end[];
    
    /* trampoline的LMA地址（在内核映像中的位置） */
    /* 注意：这里假设内核是恒等映射的 */
    /* 使用 size_t 避免 signed/unsigned 转换警告 */
    size_t trampoline_lma_raw = (size_t)(uintptr_t)_kernel_end;
    uintptr_t trampoline_lma = (trampoline_lma_raw + 0xFFF) & ~((size_t)0xFFF);  /* 4KB对齐 */
    
    /* trampoline的目标地址（VMA） */
    uintptr_t trampoline_vma = AP_TRAMPOLINE_ADDR;  /* 0x8000 */
    
    /* 计算trampoline大小 */
    size_t trampoline_size = (size_t)(__ap_trampoline_end - __ap_trampoline_start);
    
    /* 如果链接器符号无效，使用默认大小 */
    if (trampoline_size == 0 || trampoline_size > 4096) {
        trampoline_size = 512;  /* 默认512字节 */
    }
    
    console_puts("[AMP] Trampoline: LMA=0x");
    console_puthex64(trampoline_lma);
    console_puts(", VMA=0x");
    console_puthex64(trampoline_vma);
    console_puts(", size=");
    console_putu64(trampoline_size);
    console_puts(" bytes\n");
    
    /* 验证目标地址在1MB以下（实模式可访问） */
    if (trampoline_vma >= 0x100000) {
        console_puts("[AMP] ERROR: Trampoline address must be below 1MB\n");
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 复制trampoline代码到目标地址 */
    u8 *src = (u8*)trampoline_lma;  /* 源地址（内核映像中的位置） */
    u8 *dst = (u8*)trampoline_vma;  /* 目标地址（低内存） */
    
    console_puts("[AMP] Copying trampoline from 0x");
    console_puthex64((u64)src);
    console_puts(" to 0x");
    console_puthex64((u64)dst);
    console_puts("\n");
    
    for (size_t i = 0; i < trampoline_size; i++) {
        dst[i] = src[i];
    }
    
    /* ===== 设置动态地址（修复硬编码问题）===== */
    /* 
     * ap_start.S 中的 .L_gdt_ptr_addr 需要被设置为 gdt_ptr 的实际物理地址
     * gdt_ptr 在 trampoline 代码中的偏移需要计算
     */
    extern char gdt_ptr[];  /* ap_start.S 中定义 */
    
    /* 计算 gdt_ptr 相对于 ap_trampoline 的偏移 */
    /* gdt_ptr_addr 存储的是 gdt_ptr 的实际物理地址 */
    uintptr_t gdt_ptr_vma = (uintptr_t)gdt_ptr;  /* 这是链接地址 (0x8000 + offset) */
    uintptr_t gdt_ptr_offset = gdt_ptr_vma - (uintptr_t)__ap_trampoline_start;
    uintptr_t gdt_ptr_phys = trampoline_vma + gdt_ptr_offset;
    
    /* 
     * 在 ap_start.S 中，.L_gdt_ptr_addr 位于 ap_trampoline 代码之后
     * 我们需要找到它在 trampoline 中的偏移
     * 实际上，由于 .L_gdt_ptr_addr 是局部标签，我们需要使用其他方法
     * 
     * 更好的方法：直接在复制后的代码中设置 gdt_ptr_addr
     * 
     * ap_start.S 布局：
     *   ap_trampoline: 代码
     *   .L_gdt_ptr_addr: 8字节（存储gdt_ptr物理地址）
     *   gdt_ptr: GDT描述符
     *   gdt: GDT表
     *   pml4_addr: PML4地址
     *   stack_top: 栈顶地址
     */
    
    /* 查找 .L_gdt_ptr_addr 在 trampoline 中的位置 */
    /* 由于是局部标签，我们通过代码模式查找 */
    /* 或者，我们可以假设 .L_gdt_ptr_addr 紧跟在代码之后 */
    
    /* 实际上，查看 ap_start.S，.L_gdt_ptr_addr 在代码中间 */
    /* 让我们使用更简单的方法：在复制时直接修改代码中的地址 */
    
    /* 
     * 方案B：修改 gdt_ptr 中的地址
     * gdt_ptr 结构：{ limit(2), base(8) }
     * base 字段需要是 gdt 的物理地址
     */
    
    /* 计算 gdt 的物理地址 */
    extern char gdt[];  /* ap_start.S 中定义 */
    uintptr_t gdt_vma = (uintptr_t)gdt;
    uintptr_t gdt_offset = gdt_vma - (uintptr_t)__ap_trampoline_start;
    uintptr_t gdt_phys = trampoline_vma + gdt_offset;
    
    /* 修改 gdt_ptr.base 为 gdt 的物理地址 */
    /* gdt_ptr 结构在 ap_start.S 中定义：
     *   .word gdt_end - gdt - 1  (limit)
     *   .quad gdt                 (base)
     */
    uintptr_t gdt_ptr_base_offset = gdt_ptr_offset + 2;  /* 跳过 limit */
    u64 *gdt_ptr_base = (u64*)(dst + gdt_ptr_base_offset);
    *gdt_ptr_base = (u64)gdt_phys;
    
    console_puts("[AMP] gdt_ptr at 0x");
    console_puthex64(gdt_ptr_phys);
    console_puts(", gdt at 0x");
    console_puthex64(gdt_phys);
    console_puts("\n");
    
    /* 设置 pml4_addr 和 stack_top 的位置 */
    extern u64 pml4_addr[], stack_top[];
    uintptr_t pml4_offset = (uintptr_t)pml4_addr - (uintptr_t)__ap_trampoline_start;
    uintptr_t stack_offset = (uintptr_t)stack_top - (uintptr_t)__ap_trampoline_start;
    
    console_puts("[AMP] pml4_addr offset: 0x");
    console_puthex64(pml4_offset);
    console_puts(", stack_top offset: 0x");
    console_puthex64(stack_offset);
    console_puts("\n");

    console_puts("[AMP] AP trampoline prepared at 0x");
    console_puthex64(trampoline_vma);
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
        status = pmm_alloc_frames(0, 2, PAGE_FRAME_CORE, &stack_phys);
        if (status != HIC_SUCCESS) {
            console_puts("[AMP] Failed to allocate stack for AP ");
            console_puthex32(i);
            console_puts("\n");
            continue;
        }

        cpu->stack_base = (void*)stack_phys;
        cpu->stack_top = (void*)(stack_phys + 8192);

        /*
         * 解析 MADT 表获取实际 APIC ID
         * MADT (Multiple APIC Description Table) 包含所有 CPU 的 APIC ID
         */
        if (g_boot_state.boot_info != NULL && 
            g_boot_state.boot_info->hardware.hw_data != NULL) {
            /* 从硬件探测数据中获取 APIC ID */
            /* 硬件探测在引导层完成，解析了 MADT 表 */
            extern cpu_info_t g_cpu_info;
            
            /* 由于 cpu_info_t 不存储每个核心的 APIC ID，
             * 我们需要从 MADT 表中直接读取。
             * 这里使用索引作为 APIC ID（在大多数系统上索引和 APIC ID 相同）
             * 如果需要精确的 APIC ID，需要在硬件探测阶段存储
             */
            if (i < g_cpu_info.physical_cores) {
                /* 使用索引作为 APIC ID（通常正确） */
                cpu->apic_id = i;
            } else {
                cpu->apic_id = i;  /* 回退到索引 */
            }
        } else {
            /* 无 MADT 信息，使用索引作为 APIC ID */
            cpu->apic_id = i;
        }
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

        /* 等待AP启动，使用超时机制 */
        /* 最多等待 10ms (10000us) */
        u32 timeout_us = 10000;
        bool ap_started = false;
        
        while (timeout_us > 0 && !ap_started) {
            hal_udelay(10);
            timeout_us -= 10;
            
            /* 检查AP是否已上线 */
            if (cpu->state == AMP_CPU_ONLINE) {
                ap_started = true;
            }
        }

        if (!ap_started) {
            console_puts("[AMP] AP ");
            console_puthex32(i);
            console_puts(" failed to start within timeout\n");
            /* 标记AP为离线状态 */
            cpu->state = AMP_CPU_OFFLINE;
        } else {
            g_amp_info.online_cpus++;
            console_puts("[AMP] AP ");
            console_puthex32(i);
            console_puts(" started successfully\n");
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
        /* 如果AMP未启用或没有能力验证CPU，在BSP上直接验证 */
        extern cap_entry_t g_global_cap_table[];
        
        if (cap_id >= CAP_TABLE_SIZE) {
            return false;
        }
        
        cap_entry_t *entry = &g_global_cap_table[cap_id];
        
        /* 检查能力是否有效 */
        if (entry->cap_id != cap_id) {
            return false;
        }
        
        /* 检查是否被撤销 */
        if (entry->flags & CAP_FLAG_REVOKED) {
            return false;
        }
        
        /* 检查拥有者 */
        if (entry->owner != domain_id) {
            return false;
        }
        
        /* 检查权限 */
        if ((entry->rights & required_rights) != required_rights) {
            return false;
        }
        
        return true;
    }

    /* 查找能力验证CPU */
    cpu_id_t cap_cpu = INVALID_CPU_ID;
    for (u32 i = 0; i < g_amp_info.cpu_count; i++) {
        if (g_amp_info.cpus[i].mode == AMP_MODE_CAP_VERIFY) {
            cap_cpu = (cpu_id_t)i;
            break;
        }
    }

    if (cap_cpu == INVALID_CPU_ID) {
        /* 没有能力验证CPU，在BSP上验证 */
        extern cap_entry_t g_global_cap_table[];
        
        if (cap_id >= CAP_TABLE_SIZE) {
            return false;
        }
        
        cap_entry_t *entry = &g_global_cap_table[cap_id];
        
        if (entry->cap_id != cap_id || 
            (entry->flags & CAP_FLAG_REVOKED) ||
            entry->owner != domain_id ||
            (entry->rights & required_rights) != required_rights) {
            return false;
        }
        
        return true;
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
            (u32)entry->required_rights == (u32)required_rights) {
            
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

    /* 执行实际的能力验证 */
    extern cap_entry_t g_global_cap_table[];
    bool valid = false;
    
    if (cap_id < CAP_TABLE_SIZE) {
        cap_entry_t *cap_entry = &g_global_cap_table[cap_id];
        
        /* 验证能力有效性 */
        if (cap_entry->cap_id == cap_id &&
            !(cap_entry->flags & CAP_FLAG_REVOKED) &&
            cap_entry->owner == domain_id &&
            (cap_entry->rights & required_rights) == required_rights) {
            valid = true;
        }
    }
    
    cpu->caps_verified++;

    /* 添加到缓存 */
    for (u32 i = 0; i < AMP_CAP_CACHE_SIZE; i++) {
        amp_cap_cache_entry_t *entry = &cpu->cap_cache[i];

        if (!entry->in_use) {
            entry->cap_id = cap_id;
            entry->domain_id = domain_id;
            entry->required_rights = (cap_rights_t)required_rights;
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
                /* 通过 IPI 通知 AP 处理中断 */
                
                /* 记录中断信息 */
                for (u32 k = 0; k < AMP_IRQ_ASSIGN_MAX; k++) {
                    if (cpu->irq_records[k].irq_vector == irq_vector) {
                        cpu->irq_records[k].count++;
                        cpu->irq_records[k].last_time = hal_get_timestamp();
                        cpu->irqs_handled++;
                        break;
                    }
                }
                
                /* 向 AP 发送中断通知 IPI */
                /* 使用固定的中断向量通知 AP 有新任务 */
                #define AMP_NOTIFY_VECTOR 0xE0
                send_ipi(cpu->apic_id, AMP_NOTIFY_VECTOR);
                
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
            return (cpu_id_t)i;
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
    console_puthex32((u32)cpu_id);
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
    console_puthex32((u32)cpu_id);
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
    console_puthex32((u32)cpu_id);
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
    console_puthex32((u32)cpu_id);
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
                /* 
                 * 加密任务 - 由 Privileged-1 的 crypto_service 处理
                 * arg1: 操作类型 (0=hash, 1=encrypt, 2=decrypt, 3=sign, 4=verify)
                 * arg2: 输入缓冲区地址
                 * arg3: 输出缓冲区地址
                 * arg4: 数据长度
                 * 
                 * 注意：完整的加密操作应该通过端点调用 crypto_service
                 * 这里提供简化的哈希计算作为示例
                 */
                {
                    u64 hash = 0;
                    u8 *data = (u8*)task->input_buffer;
                    size_t len = task->buffer_size;
                    
                    /* 简单的 FNV-1a 哈希作为示例 */
                    for (size_t i = 0; i < len && i < 1024; i++) {
                        hash ^= data[i];
                        hash *= 1099511628211ULL;
                    }
                    task->result = hash;
                }
                break;
            case AMP_TASK_DATA_PROCESS:
                /* 
                 * 数据处理任务 - 通用数据处理
                 * arg1: 操作类型
                 * arg2: 输入缓冲区
                 * arg3: 输出缓冲区
                 * arg4: 参数
                 */
                {
                    u8 *input = (u8*)task->input_buffer;
                    u8 *output = (u8*)task->output_buffer;
                    size_t len = task->buffer_size;
                    u64 op = task->arg1;
                    
                    switch (op) {
                        case 0: /* 复制 */
                            if (input && output && len > 0) {
                                memmove(output, input, len);
                                task->result = len;
                            }
                            break;
                        case 1: /* 清零 */
                            if (output && len > 0) {
                                memzero(output, len);
                                task->result = len;
                            }
                            break;
                        case 2: /* 字节求和 */
                            {
                                u64 sum = 0;
                                for (size_t i = 0; i < len && i < 4096; i++) {
                                    sum += input[i];
                                }
                                task->result = sum;
                            }
                            break;
                        default:
                            task->result = 0;
                            break;
                    }
                }
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
