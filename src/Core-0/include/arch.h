/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK架构抽象层 - 仅架构相关的定义
 * 
 * 本文件包含所有架构特定的定义和接口
 * 架构无关的代码不应直接包含此文件
 */

#ifndef HIK_ARCH_H
#define HIK_ARCH_H

#include "types.h"

/* ==================== 架构类型枚举 ==================== */
typedef enum {
    ARCH_UNKNOWN = 0,
    ARCH_X86_64,
    ARCH_ARM64,
    ARCH_RISCV64,
    ARCH_MAX
} arch_type_t;

/* 当前架构 */
#ifdef __x86_64__
#define CURRENT_ARCH ARCH_X86_64
#elif defined(__aarch64__)
#define CURRENT_ARCH ARCH_ARM64
#elif defined(__riscv) && (__riscv_xlen == 64)
#define CURRENT_ARCH ARCH_RISCV64
#else
#define CURRENT_ARCH ARCH_UNKNOWN
#endif

/* ==================== 架构特定常量 ==================== */

/* 页大小（不同架构可能不同） */
#if CURRENT_ARCH == ARCH_X86_64
#define PAGE_SIZE           4096
#define PAGE_SHIFT          12
#elif CURRENT_ARCH == ARCH_ARM64
#define PAGE_SIZE           4096
#define PAGE_SHIFT          12
#elif CURRENT_ARCH == ARCH_RISCV64
#define PAGE_SIZE           4096
#define PAGE_SHIFT          12
#else
#define PAGE_SIZE           4096
#define PAGE_SHIFT          12
#endif

/* 特权级 */
#if CURRENT_ARCH == ARCH_X86_64
#define PRIVILEGE_KERNEL    0  /* Ring 0 */
#define PRIVILEGE_USER      3  /* Ring 3 */
#elif CURRENT_ARCH == ARCH_ARM64
#define PRIVILEGE_KERNEL    0  /* EL1 */
#define PRIVILEGE_USER      0  /* EL0 */
#elif CURRENT_ARCH == ARCH_RISCV64
#define PRIVILEGE_KERNEL    0  /* M/S Mode */
#define PRIVILEGE_USER      0  /* U Mode */
#endif

/* ==================== 架构特定寄存器上下文 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
/* x86_64 寄存器上下文 */
typedef struct {
    /* 通用寄存器 */
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8, r9, r10, r11;
    u64 r12, r13, r14, r15;
    
    /* 特殊寄存器 */
    u64 rip;              /* 指令指针 */
    u64 rflags;           /* 标志寄存器 */
    u64 cr0, cr2, cr3, cr4;  /* 控制寄存器 */
    
    /* 栈信息 */
    u64 kernel_rsp;       /* 内核栈指针 */
    u64 user_rsp;         /* 用户栈指针 */
    
    /* 段寄存器 */
    u64 cs, ds, es, fs, gs, ss;
} arch_context_t;

#elif CURRENT_ARCH == ARCH_ARM64
/* ARM64 寄存器上下文 */
typedef struct {
    /* 通用寄存器 x0-x30 */
    u64 x[31];
    
    /* 特殊寄存器 */
    u64 sp;               /* 栈指针 (x31) */
    u64 pc;               /* 程序计数器 */
    u64 pstate;           /* 处理器状态 */
    
    /* 系统寄存器 */
    u64 sp_el0;           /* 用户模式栈指针 */
    u64 elr_el1;          /* 异常链接寄存器 */
    u64 esr_el1;          /* 异常综合寄存器 */
    
    /* FP/SIMD寄存器 */
    u64 v[32];            /* 向量寄存器 */
    u64 fpsr;             /* FP状态寄存器 */
    u64 fpcr;             /* FP控制寄存器 */
} arch_context_t;

#elif CURRENT_ARCH == ARCH_RISCV64
/* RISC-V64 寄存器上下文 */
typedef struct {
    /* 通用寄存器 x0-x31 */
    u64 x[32];
    
    /* 特殊寄存器 */
    u64 pc;               /* 程序计数器 */
    
    /* 系统寄存器 */
    u64 mstatus;          /* 机器状态寄存器 */
    u64 mepc;             /* 机器异常PC */
    u64 mcause;           /* 机器异常原因 */
    u64 mtval;            /* 机器异常值 */
    
    /* 保存的栈指针 */
    u64 sp;               /* 栈指针 */
    u64 tp;               /* 线程指针 */
} arch_context_t;

#else
/* 未知架构的空上下文 */
typedef struct {
    u64 reserved[32];
} arch_context_t;
#endif

/* ==================== 架构特定中断向量 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
/* x86_64 异常和中断向量 */
#define EXC_DIVIDE_ERROR        0
#define EXC_DEBUG               1
#define EXC_NMI                 2
#define EXC_BREAKPOINT          3
#define EXC_OVERFLOW            4
#define EXC_BOUND_RANGE         5
#define EXC_INVALID_OPCODE      6
#define EXC_DEVICE_NOT_AVAIL    7
#define EXC_DOUBLE_FAULT        8
#define EXC_INVALID_TSS         10
#define EXC_SEGMENT_NOT_PRESENT 11
#define EXC_STACK_SEGMENT       12
#define EXC_GENERAL_PROTECTION  13
#define EXC_PAGE_FAULT          14
#define EXC_X87_FPU_ERROR       16
#define EXC_ALIGNMENT_CHECK     17
#define EXC_MACHINE_CHECK       18
#define EXC_SIMD_FP             19
#define EXC_VIRTUALIZATION      20
#define EXC_SECURITY            30
#define IRQ_VECTOR_IRQ_BASE     32
#define IRQ_VECTOR_SYSCALL      128

#elif CURRENT_ARCH == ARCH_ARM64
/* ARM64 异常向量 */
#define EXC_SYNC_EL1_SP0        0
#define EXC_IRQ_EL1_SP0         1
#define EXC_FIQ_EL1_SP0         2
#define EXC_SERROR_EL1_SP0      3
#define EXC_SYNC_EL1_SPX        4
#define EXC_IRQ_EL1_SPX         5
#define EXC_FIQ_EL1_SPX         6
#define EXC_SERROR_EL1_SPX      7
#define IRQ_VECTOR_IRQ_BASE     16
#define IRQ_VECTOR_SYSCALL      64

#elif CURRENT_ARCH == ARCH_RISCV64
/* RISC-V64 异常原因 */
#define EXC_INSTRUCTION_MISALIGNED  0
#define EXC_INSTRUCTION_ACCESS      1
#define EXC_ILLEGAL_INSTRUCTION     2
#define EXC_BREAKPOINT              3
#define EXC_LOAD_MISALIGNED         4
#define EXC_LOAD_ACCESS             5
#define EXC_STORE_MISALIGNED        6
#define EXC_STORE_ACCESS            7
#define EXC_ECALL_U                 8
#define EXC_ECALL_S                 9
#define EXC_ECALL_M                 11
#define EXC_INSTRUCTION_PAGE_FAULT  12
#define EXC_LOAD_PAGE_FAULT         13
#define EXC_STORE_PAGE_FAULT        15
#define IRQ_VECTOR_IRQ_BASE         16
#define IRQ_VECTOR_SYSCALL          8

#endif

/* ==================== 架构特定内存屏障 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
static inline void arch_memory_barrier(void) {
    __asm__ volatile("mfence" ::: "memory");
}

static inline void arch_read_barrier(void) {
    __asm__ volatile("" ::: "memory");
}

static inline void arch_write_barrier(void) {
    __asm__ volatile("" ::: "memory");
}

#elif CURRENT_ARCH == ARCH_ARM64
static inline void arch_memory_barrier(void) {
    __asm__ volatile("dmb ish" ::: "memory");
}

static inline void arch_read_barrier(void) {
    __asm__ volatile("dmb ishld" ::: "memory");
}

static inline void arch_write_barrier(void) {
    __asm__ volatile("dmb ish" ::: "memory");
}

#elif CURRENT_ARCH == ARCH_RISCV64
static inline void arch_memory_barrier(void) {
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

static inline void arch_read_barrier(void) {
    __asm__ volatile("fence ir, ir" ::: "memory");
}

static inline void arch_write_barrier(void) {
    __asm__ volatile("fence ow, ow" ::: "memory");
}

#endif

/* ==================== 架构特定中断控制 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
static inline void arch_disable_interrupts(void) {
    __asm__ volatile("cli");
}

static inline void arch_enable_interrupts(void) {
    __asm__ volatile("sti");
}

#elif CURRENT_ARCH == ARCH_ARM64
static inline void arch_disable_interrupts(void) {
    __asm__ volatile("msr daifset, #2");
}

static inline void arch_enable_interrupts(void) {
    __asm__ volatile("msr daifclr, #2");
}

#elif CURRENT_ARCH == ARCH_RISCV64
static inline void arch_disable_interrupts(void) {
    __asm__ volatile("csrc mstatus, 8");
}

static inline void arch_enable_interrupts(void) {
    __asm__ volatile("csrs mstatus, 8");
}

#endif

/* ==================== 架构特定时间戳 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
static inline u64 arch_get_timestamp(void) {
    u32 low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((u64)high << 32) | low;
}

#elif CURRENT_ARCH == ARCH_ARM64
static inline u64 arch_get_timestamp(void) {
    u64 cnt;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(cnt));
    return cnt;
}

#elif CURRENT_ARCH == ARCH_RISCV64
static inline u64 arch_get_timestamp(void) {
    u64 time;
    __asm__ volatile("rdtime %0" : "=r"(time));
    return time;
}

#else
static inline u64 arch_get_timestamp(void) {
    return 0;
}

#endif

/* ==================== 架构特定特权级查询 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
static inline u32 arch_get_privilege_level(void) {
    u64 cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    return (cs >> 3) & 3;
}

#elif CURRENT_ARCH == ARCH_ARM64
static inline u32 arch_get_privilege_level(void) {
    u64 currentel;
    __asm__ volatile("mrs %0, currentel" : "=r"(currentel));
    return (currentel >> 2) & 3;
}

#elif CURRENT_ARCH == ARCH_RISCV64
static inline u32 arch_get_privilege_level(void) {
    u64 status;
    __asm__ volatile("csrr %0, mstatus" : "=r"(status));
    return (status >> 3) & 3;
}

#else
static inline u32 arch_get_privilege_level(void) {
    return 0;
}

#endif

/* ==================== 架构特定控制寄存器 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
static inline u64 arch_read_cr(u32 reg) {
    u64 val;
    switch (reg) {
        case 0:
            __asm__ volatile("mov %%cr0, %0" : "=r"(val));
            break;
        case 2:
            __asm__ volatile("mov %%cr2, %0" : "=r"(val));
            break;
        case 3:
            __asm__ volatile("mov %%cr3, %0" : "=r"(val));
            break;
        case 4:
            __asm__ volatile("mov %%cr4, %0" : "=r"(val));
            break;
        default:
            val = 0;
    }
    return val;
}

static inline void arch_write_cr(u32 reg, u64 val) {
    switch (reg) {
        case 0:
            __asm__ volatile("mov %0, %%cr0" : : "r"(val));
            break;
        case 2:
            __asm__ volatile("mov %0, %%cr2" : : "r"(val));
            break;
        case 3:
            __asm__ volatile("mov %0, %%cr3" : : "r"(val));
            break;
        case 4:
            __asm__ volatile("mov %0, %%cr4" : : "r"(val));
            break;
    }
}

#elif CURRENT_ARCH == ARCH_ARM64
static inline u64 arch_read_cr(u32 reg) {
    u64 val;
    switch (reg) {
        case 0:
            __asm__ volatile("mrs %0, sctlr_el1" : "=r"(val));
            break;
        default:
            val = 0;
    }
    return val;
}

static inline void arch_write_cr(u32 reg, u64 val) {
    switch (reg) {
        case 0:
            __asm__ volatile("msr sctlr_el1, %0" : : "r"(val));
            break;
    }
}

#elif CURRENT_ARCH == ARCH_RISCV64
static inline u64 arch_read_cr(u32 reg) {
    u64 val;
    switch (reg) {
        case 0:
            __asm__ volatile("csrr %0, mstatus" : "=r"(val));
            break;
        default:
            val = 0;
    }
    return val;
}

static inline void arch_write_cr(u32 reg, u64 val) {
    switch (reg) {
        case 0:
            __asm__ volatile("csrw mstatus, %0" : : "r"(val));
            break;
    }
}

#endif

/* ==================== 架构特定IO端口（仅x86） ==================== */

#if CURRENT_ARCH == ARCH_X86_64
static inline u8 arch_inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void arch_outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

#else
static inline u8 arch_inb(u16 port) {
    (void)port;
    return 0xFF;
}

static inline void arch_outb(u16 port, u8 value) {
    (void)port;
    (void)value;
}

#endif

/* ==================== 架构特定CPUID/MSR操作 ==================== */

#if CURRENT_ARCH == ARCH_X86_64
static inline u32 arch_cpuid(u32 leaf, u32 subleaf, u32 *eax, u32 *ebx, 
                              u32 *ecx, u32 *edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
    return *eax;
}

static inline u64 arch_rdmsr(u32 msr) {
    u32 low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((u64)high << 32) | low;
}

static inline void arch_wrmsr(u32 msr, u64 value) {
    u32 low = value & 0xFFFFFFFF;
    u32 high = value >> 32;
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

#else
static inline u32 arch_cpuid(u32 leaf, u32 subleaf, u32 *eax, u32 *ebx, 
                              u32 *ecx, u32 *edx) {
    (void)leaf; (void)subleaf;
    *eax = *ebx = *ecx = *edx = 0;
    return 0;
}

static inline u64 arch_rdmsr(u32 msr) {
    (void)msr;
    return 0;
}

static inline void arch_wrmsr(u32 msr, u64 value) {
    (void)msr; (void)value;
}

#endif

/* ==================== 架构特定操作 ==================== */

static inline void arch_halt(void) {
#if CURRENT_ARCH == ARCH_X86_64
    __asm__ volatile("hlt");
#elif CURRENT_ARCH == ARCH_ARM64
    __asm__ volatile("wfi");
#elif CURRENT_ARCH == ARCH_RISCV64
    __asm__ volatile("wfi");
#endif
}

/* ==================== 架构特定名称 ==================== */

static inline const char* arch_get_name(void) {
    switch (CURRENT_ARCH) {
        case ARCH_X86_64:   return "x86_64";
        case ARCH_ARM64:    return "ARM64";
        case ARCH_RISCV64:  return "RISC-V64";
        default:            return "Unknown";
    }
}

#endif /* HIK_ARCH_H */