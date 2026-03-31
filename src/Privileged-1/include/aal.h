/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * AAL (Architecture Adaptation Layer) - 服务侧共享库接口
 * 
 * AAL 作为共享库（.hiclib）实现，提供架构特定的优化接口。
 * 
 * 功能：
 * - 页表操作
 * - TLB 管理
 * - 上下文切换
 * - 特殊寄存器访问
 * 
 * 使用方式：通过 lib_manager 加载后直接调用
 */

#ifndef HIC_SERVICE_AAL_H
#define HIC_SERVICE_AAL_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "hiclib.h"

/* ==================== 库标识 ==================== */

/* AAL 库 UUID */
#define AAL_LIB_UUID \
    { 0xA1, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
      0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }

#define AAL_LIB_NAME    "libaal"
#define AAL_LIB_VERSION HICLIB_PACK_VERSION(1, 0, 0)

/* ==================== 类型定义 ==================== */

typedef uint64_t paddr_t;
typedef uint64_t vaddr_t;

/* 页表句柄 */
typedef uint64_t aal_pgtbl_t;

/* 上下文结构（不透明） */
typedef struct aal_context aal_context_t;

/* ==================== 页表权限标志 ==================== */

#define AAL_PAGE_PRESENT    (1U << 0)
#define AAL_PAGE_WRITABLE   (1U << 1)
#define AAL_PAGE_USER       (1U << 2)
#define AAL_PAGE_NO_EXEC    (1U << 3)
#define AAL_PAGE_ACCESSED   (1U << 4)
#define AAL_PAGE_DIRTY      (1U << 5)
#define AAL_PAGE_HUGE       (1U << 6)
#define AAL_PAGE_GLOBAL     (1U << 7)

/* ==================== 页表操作 ==================== */

/**
 * 创建页表
 * @return 页表句柄，失败返回 0
 */
aal_pgtbl_t aal_page_table_create(void);

/**
 * 销毁页表
 * @param pgtbl 页表句柄
 * @return 错误码
 */
int aal_page_table_destroy(aal_pgtbl_t pgtbl);

/**
 * 映射页面
 * @param pgtbl 页表句柄
 * @param vaddr 虚拟地址
 * @param paddr 物理地址
 * @param flags 权限标志
 * @return 错误码
 */
int aal_page_map(aal_pgtbl_t pgtbl, vaddr_t vaddr, paddr_t paddr, uint32_t flags);

/**
 * 取消映射页面
 */
int aal_page_unmap(aal_pgtbl_t pgtbl, vaddr_t vaddr);

/**
 * 获取物理地址
 */
paddr_t aal_page_get_phys(aal_pgtbl_t pgtbl, vaddr_t vaddr);

/**
 * 设置页面权限
 */
int aal_page_set_flags(aal_pgtbl_t pgtbl, vaddr_t vaddr, uint32_t flags);

/* ==================== TLB 操作 ==================== */

/**
 * 刷新全部 TLB
 */
void aal_tlb_flush_all(void);

/**
 * 刷新单个页面 TLB
 */
void aal_tlb_flush_page(vaddr_t vaddr);

/**
 * 刷新地址范围 TLB
 */
void aal_tlb_flush_range(vaddr_t vaddr, size_t size);

/* ==================== 上下文操作 ==================== */

/**
 * 保存上下文
 */
int aal_context_save(aal_context_t *ctx);

/**
 * 恢复上下文
 */
int aal_context_restore(aal_context_t *ctx);

/**
 * 切换上下文
 */
void aal_context_switch(aal_context_t *from, aal_context_t *to);

/**
 * 初始化上下文
 */
int aal_context_init(aal_context_t *ctx, void *entry, void *stack);

/* ==================== 特殊寄存器（x86_64） ==================== */

/**
 * 获取 CR3
 */
uint64_t aal_get_cr3(void);

/**
 * 设置 CR3
 */
void aal_set_cr3(uint64_t cr3);

/**
 * 获取 FS 基址
 */
uint64_t aal_get_fs_base(void);

/**
 * 设置 FS 基址
 */
void aal_set_fs_base(uint64_t base);

/**
 * 获取 GS 基址
 */
uint64_t aal_get_gs_base(void);

/**
 * 设置 GS 基址
 */
void aal_set_gs_base(uint64_t base);

/* ==================== CPU 特性 ==================== */

/**
 * CPUID 结果结构
 */
typedef struct {
    uint32_t leaf;
    uint32_t subleaf;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} aal_cpuid_t;

/**
 * 执行 CPUID
 */
int aal_cpuid(aal_cpuid_t *info);

/**
 * 读取 MSR
 */
uint64_t aal_read_msr(uint32_t msr);

/**
 * 写入 MSR
 */
void aal_write_msr(uint32_t msr, uint64_t value);

/* ==================== 缓存操作 ==================== */

/**
 * 刷新缓存
 */
void aal_cache_flush(void *addr, size_t size);

/**
 * 使缓存无效
 */
void aal_cache_invalidate(void *addr, size_t size);

/**
 * 预取
 */
void aal_cache_prefetch(const void *addr);

/* ==================== 错误码 ==================== */

typedef enum {
    AAL_OK = 0,
    AAL_ERR_INVALID_PARAM = 1,
    AAL_ERR_NOT_SUPPORTED = 2,
    AAL_ERR_NO_MEMORY = 3,
    AAL_ERR_PERMISSION = 4,
    AAL_ERR_ALIGNMENT = 5,
} aal_error_t;

/* ==================== 库加载辅助 ==================== */

typedef struct aal_lib_handle aal_lib_handle_t;

aal_lib_handle_t *aal_lib_load(void);
void aal_lib_unload(aal_lib_handle_t *handle);
uint32_t aal_lib_get_version(aal_lib_handle_t *handle);

#endif /* HIC_SERVICE_AAL_H */