/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * IMAL (Isolation Mechanism Abstraction Layer) - 服务侧共享库接口
 * 
 * IMAL 作为共享库（.hiclib）实现，提供统一的隔离机制接口。
 * 
 * 功能：
 * - 隔离域创建/销毁
 * - 地址空间管理
 * - 内存映射
 * - TLB 操作
 * 
 * 支持 MMU/noMMU：
 * - 有 MMU：使用页表隔离
 * - 无 MMU：使用 MPU 或软件隔离
 */

#ifndef HIC_SERVICE_IMAL_H
#define HIC_SERVICE_IMAL_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "hiclib.h"

/* ==================== 库标识 ==================== */

/* IMAL 库 UUID */
#define IMAL_LIB_UUID \
    { 0x11, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
      0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }

#define IMAL_LIB_NAME    "libimal"
#define IMAL_LIB_VERSION HICLIB_PACK_VERSION(1, 0, 0)

/* ==================== 类型定义 ==================== */

typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;
typedef uint32_t domain_id_t;

/* 隔离域句柄 */
typedef uint64_t imal_domain_t;

/* 权限类型 */
typedef uint32_t imal_perm_t;

/* ==================== 权限定义 ==================== */

#define IMAL_PERM_NONE    (0U)
#define IMAL_PERM_READ    (1U << 0)
#define IMAL_PERM_WRITE   (1U << 1)
#define IMAL_PERM_EXEC    (1U << 2)
#define IMAL_PERM_RW      (IMAL_PERM_READ | IMAL_PERM_WRITE)
#define IMAL_PERM_RX      (IMAL_PERM_READ | IMAL_PERM_EXEC)
#define IMAL_PERM_RWX     (IMAL_PERM_READ | IMAL_PERM_WRITE | IMAL_PERM_EXEC)

/* ==================== 错误码 ==================== */

typedef enum {
    IMAL_OK = 0,
    IMAL_ERR_NOMEM = 1,
    IMAL_ERR_INVAL = 2,
    IMAL_ERR_PERM = 3,
    IMAL_ERR_RANGE = 4,
    IMAL_ERR_BUSY = 5,
    IMAL_ERR_NOT_FOUND = 6,
    IMAL_ERR_OVERFLOW = 7,
} imal_error_t;

/* ==================== 隔离域操作 ==================== */

/**
 * 创建隔离域
 * @return 隔离域句柄
 */
imal_domain_t imal_domain_create(void);

/**
 * 销毁隔离域
 */
imal_error_t imal_domain_destroy(imal_domain_t domain);

/**
 * 切换到目标域
 */
imal_error_t imal_domain_switch(imal_domain_t domain);

/**
 * 获取当前域
 */
imal_domain_t imal_domain_get_current(void);

/* ==================== 内存映射操作 ==================== */

/**
 * 映射内存
 * @param domain 隔离域
 * @param vaddr 虚拟地址
 * @param paddr 物理地址
 * @param size 大小
 * @param perm 权限
 * @return 错误码
 */
imal_error_t imal_map(imal_domain_t domain, vaddr_t vaddr,
                       paddr_t paddr, size_t size, imal_perm_t perm);

/**
 * 取消映射
 */
imal_error_t imal_unmap(imal_domain_t domain, vaddr_t vaddr, size_t size);

/**
 * 重新映射
 */
imal_error_t imal_remap(imal_domain_t domain, vaddr_t old_vaddr,
                         vaddr_t new_vaddr, size_t size);

/**
 * 修改权限
 */
imal_error_t imal_protect(imal_domain_t domain, vaddr_t vaddr,
                           size_t size, imal_perm_t perm);

/**
 * 查询映射
 */
imal_error_t imal_query(imal_domain_t domain, vaddr_t vaddr,
                         paddr_t *paddr, imal_perm_t *perm);

/* ==================== TLB 操作 ==================== */

/**
 * 刷新全部 TLB
 */
void imal_tlb_flush_all(void);

/**
 * 刷新域相关 TLB
 */
void imal_tlb_flush_domain(imal_domain_t domain);

/**
 * 刷新单个地址 TLB
 */
void imal_tlb_flush_va(vaddr_t vaddr);

/* ==================== 权限查询 ==================== */

/**
 * 检查权限
 */
imal_error_t imal_check_perm(imal_domain_t domain, vaddr_t vaddr,
                              size_t size, imal_perm_t perm);

/**
 * 获取物理地址
 */
paddr_t imal_get_phys(imal_domain_t domain, vaddr_t vaddr);

/* ==================== 域属性 ==================== */

/**
 * 设置域所有者
 */
void imal_set_owner(imal_domain_t domain, domain_id_t owner);

/**
 * 获取域所有者
 */
domain_id_t imal_get_owner(imal_domain_t domain);

/**
 * 域信息结构
 */
typedef struct {
    size_t total_mappings;
    size_t total_pages;
    size_t code_pages;
    size_t data_pages;
    domain_id_t owner;
    uint32_t flags;
} imal_domain_info_t;

/**
 * 获取域信息
 */
imal_error_t imal_get_info(imal_domain_t domain, imal_domain_info_t *info);

/* ==================== 错误检查辅助 ==================== */

static inline bool imal_ok(imal_error_t err) { return err == IMAL_OK; }
static inline bool imal_fail(imal_error_t err) { return err != IMAL_OK; }

/* ==================== 库加载辅助 ==================== */

typedef struct imal_lib_handle imal_lib_handle_t;

imal_lib_handle_t *imal_lib_load(void);
void imal_lib_unload(imal_lib_handle_t *handle);
uint32_t imal_lib_get_version(imal_lib_handle_t *handle);

#endif /* HIC_SERVICE_IMAL_H */