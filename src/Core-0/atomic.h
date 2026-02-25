/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC原子操作和无锁基础设施
 * 
 * 设计原则：
 * 1. 无锁设计 - 所有数据结构都设计为无锁，避免竞态条件
 * 2. 内存屏障 - 确保多核环境下的内存访问顺序
 * 3. 原子操作 - 使用CPU原子指令确保操作的原子性
 * 4. 单核优化 - 在单核环境下禁用中断即可保证原子性
 * 
 * 注意：此实现基于以下假设：
 * - Core-0层单线程执行（单核或单核调度）
 * - 多核环境下使用CPU原子指令
 * - 禁用中断可以保证单核原子性
 */

#ifndef HIC_ATOMIC_H
#define HIC_ATOMIC_H

#include "types.h"
#include "hal.h"

/* 定义 NULL */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* ==================== 编译器原子操作 ==================== */

/**
 * 原子加载
 */
static inline u64 atomic_load_u64(volatile u64* ptr) {
    u64 value = *ptr;
    hal_memory_barrier();
    return value;
}

/**
 * 原子存储
 */
static inline void atomic_store_u64(volatile u64* ptr, u64 value) {
    hal_memory_barrier();
    *ptr = value;
}

/**
 * 原子加载32位
 */
static inline u32 atomic_load_u32(volatile u32* ptr) {
    u32 value = *ptr;
    hal_memory_barrier();
    return value;
}

/**
 * 原子存储32位
 */
static inline void atomic_store_u32(volatile u32* ptr, u32 value) {
    hal_memory_barrier();
    *ptr = value;
}

/* ==================== 原子比较交换（CAS） ==================== */

/**
 * 原子比较并交换64位
 * @return 成功返回true，失败返回false
 */
static inline bool atomic_cas_u64(volatile u64* ptr, u64 old_val, u64 new_val) {
    bool success;
    
    /* 使用GCC内置原子操作 */
    __asm__ volatile (
        "lock cmpxchgq %2, %1\n"
        "sete %0"
        : "=a" (success), "+m" (*ptr)
        : "r" (new_val), "a" (old_val)
        : "memory", "cc"
    );
    
    return success;
}

/**
 * 原子比较并交换32位
 */
static inline bool atomic_cas_u32(volatile u32* ptr, u32 old_val, u32 new_val) {
    bool success;
    
    __asm__ volatile (
        "lock cmpxchgl %2, %1\n"
        "sete %0"
        : "=a" (success), "+m" (*ptr)
        : "r" (new_val), "a" (old_val)
        : "memory", "cc"
    );
    
    return success;
}

/* ==================== 原子算术操作 ==================== */

/**
 * 原子加
 */
static inline u64 atomic_add_u64(volatile u64* ptr, u64 value) {
    __asm__ volatile (
        "lock xaddq %0, %1"
        : "+r" (value), "+m" (*ptr)
        : 
        : "memory", "cc"
    );
    return value;
}

/**
 * 原子减
 */
static inline u64 atomic_sub_u64(volatile u64* ptr, u64 value) {
    return atomic_add_u64(ptr, -value);
}

/**
 * 原子递增
 */
static inline void atomic_inc_u64(volatile u64* ptr) {
    __asm__ volatile (
        "lock incq %0"
        : "+m" (*ptr)
        : 
        : "memory", "cc"
    );
}

/**
 * 原子递减
 */
static inline void atomic_dec_u64(volatile u64* ptr) {
    __asm__ volatile (
        "lock decq %0"
        : "+m" (*ptr)
        : 
        : "memory", "cc"
    );
}

/* ==================== 原子位操作 ==================== */

/**
 * 原子设置位
 */
static inline void atomic_set_bit(volatile u64* ptr, u32 bit) {
    __asm__ volatile (
        "lock btsq %1, %0"
        : "+m" (*ptr)
        : "r" ((u64)bit)
        : "memory", "cc"
    );
}

/**
 * 原子清除位
 */
static inline void atomic_clear_bit(volatile u64* ptr, u32 bit) {
    __asm__ volatile (
        "lock btrq %1, %0"
        : "+m" (*ptr)
        : "r" ((u64)bit)
        : "memory", "cc"
    );
}

/**
 * 原子测试并设置位
 * @return 返回位的原始值
 */
static inline bool atomic_test_and_set_bit(volatile u64* ptr, u32 bit) {
    bool result;
    __asm__ volatile (
        "lock btsq %2, %0\n"
        "setc %1"
        : "+m" (*ptr), "=r" (result)
        : "r" ((u64)bit)
        : "memory", "cc"
    );
    return result;
}

/* ==================== 单核原子性保证 ==================== */

/**
 * 禁用中断保证原子性（单核环境）
 */
static inline bool atomic_enter_critical(void) {
    return hal_disable_interrupts();
}

/**
 * 恢复中断状态
 */
static inline void atomic_exit_critical(bool irq_state) {
    hal_restore_interrupts(irq_state);
}

/* ==================== 内存屏障增强 ==================== */

/**
 * 获取屏障（读操作完成后）
 */
static inline void atomic_acquire_barrier(void) {
    hal_read_barrier();
}

/**
 * 释放屏障（写操作前）
 */
static inline void atomic_release_barrier(void) {
    hal_write_barrier();
}

/**
 * 完整屏障（读写操作之间）
 */
static inline void atomic_full_barrier(void) {
    hal_memory_barrier();
}

/* ==================== 无锁数据结构辅助宏 ==================== */

/**
 * 无锁链表节点
 */
typedef struct lockfree_node {
    volatile struct lockfree_node* next;
    u64 data;
} lockfree_node_t;

/**
 * 无锁环形缓冲区
 */
typedef struct lockfree_ringbuffer {
    volatile u64 head;  /* 生产者索引 */
    volatile u64 tail;  /* 消费者索引 */
    u64 size;           /* 缓冲区大小（必须是2的幂） */
    u64 mask;           /* size - 1 */
    void* buffer[];     /* 柔性数组 */
} lockfree_ringbuffer_t;

/**
 * 初始化无锁环形缓冲区
 */
static inline void lockfree_ringbuffer_init(lockfree_ringbuffer_t* rb, u64 size) {
    rb->head = 0;
    rb->tail = 0;
    rb->size = size;
    rb->mask = size - 1;
}

/**
 * 向无锁环形缓冲区写入
 * @return 成功返回true，失败返回false（缓冲区满）
 */
static inline bool lockfree_ringbuffer_push(lockfree_ringbuffer_t* rb, void* item) {
    u64 head = atomic_load_u64(&rb->head);
    u64 next = (head + 1) & rb->mask;
    
    /* 检查缓冲区是否已满 */
    if (next == atomic_load_u64(&rb->tail)) {
        return false;  /* 缓冲区满 */
    }
    
    /* 写入数据 */
    rb->buffer[head] = item;
    
    /* 更新head指针（使用内存屏障确保写入可见） */
    atomic_store_u64(&rb->head, next);
    
    return true;
}

/**
 * 从无锁环形缓冲区读取
 * @return 成功返回数据指针，失败返回NULL（缓冲区空）
 */
static inline void* lockfree_ringbuffer_pop(lockfree_ringbuffer_t* rb) {
    u64 tail = atomic_load_u64(&rb->tail);
    
    /* 检查缓冲区是否为空 */
    if (tail == atomic_load_u64(&rb->head)) {
        return NULL;  /* 缓冲区空 */
    }
    
    /* 读取数据 */
    void* item = rb->buffer[tail];
    
    /* 更新tail指针 */
    atomic_store_u64(&rb->tail, (tail + 1) & rb->mask);
    
    return item;
}

#endif /* HIC_ATOMIC_H */
