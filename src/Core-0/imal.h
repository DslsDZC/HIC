/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 隔离机制抽象层 (Isolation Mechanism Abstraction Layer)
 * 
 * IMAL 是 HIC 实现 MMU/MPU/noMMU 统一接口的关键层次。
 * 所有 IMAL 原语在调用点静态绑定到具体变体实现，
 * 通过编译时 #if 选择，上层能力系统完全复用，不感知底层机制差异。
 *
 * 设计原则：
 * 1. 编译时多态：零间接层，零运行时开销
 * 2. 统一语义：上层使用相同接口，底层实现不同机制
 * 3. 安全降级：从 MMU -> MPU -> 软件能力，逐级降级
 */

#ifndef HIC_KERNEL_IMAL_H
#define HIC_KERNEL_IMAL_H

#include "types.h"

/* NULL 定义 */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* 类型别名（统一接口使用） */
typedef virt_addr_t vaddr_t;
typedef phys_addr_t paddr_t;

/* ===== 权限定义（跨架构统一语义） ===== */

/**
 * 权限位掩码
 * 注意：具体硬件映射由各变体实现处理
 */
typedef u32 imal_perm_t;

#define IMAL_PERM_NONE    (0U)
#define IMAL_PERM_READ    (1U << 0)    /* 读权限 */
#define IMAL_PERM_WRITE   (1U << 1)    /* 写权限 */
#define IMAL_PERM_EXEC    (1U << 2)    /* 执行权限 */
#define IMAL_PERM_RW      (IMAL_PERM_READ | IMAL_PERM_WRITE)
#define IMAL_PERM_RX      (IMAL_PERM_READ | IMAL_PERM_EXEC)
#define IMAL_PERM_RWX     (IMAL_PERM_READ | IMAL_PERM_WRITE | IMAL_PERM_EXEC)

/* ===== 隔离域句柄（不透明类型） ===== */

/**
 * imal_domain_t - 隔离域句柄
 * 
 * 具体内容由变体实现定义：
 * - MMU 变体：指向根页表 (page_table_t*)
 * - MPU 变体：指向 MPU 区域配置
 * - noMMU 变体：NULL 或软件能力记录
 */
typedef struct imal_domain imal_domain_t;

/* ===== 错误码 ===== */

typedef enum {
    IMAL_OK = 0,
    IMAL_ERR_NOMEM,       /* 内存不足 */
    IMAL_ERR_INVAL,       /* 无效参数 */
    IMAL_ERR_PERM,        /* 权限冲突 */
    IMAL_ERR_RANGE,       /* 地址范围错误 */
    IMAL_ERR_BUSY,        /* 资源忙 */
} imal_error_t;

/* ===== 编译时变体选择 ===== */

#if CONFIG_MMU == 1

/*
 * ============================================================
 * MMU 变体实现
 * 通过页表实现隔离，domain_t 指向根页表 PA
 * ============================================================
 */

#include "pagetable.h"

/* MMU 变体：隔离域结构 */
struct imal_domain {
    page_table_t   *root;        /* 根页表 */
    domain_id_t     owner;       /* 所属域 ID */
    u32             ref_count;   /* 引用计数 */
    u32             flags;       /* 标志位 */
};

/* 权限映射：IMAL -> x86_64 页表标志 */
static inline u64 imal_perm_to_page_flags(imal_perm_t perm)
{
    u64 flags = PAGE_FLAG_PRESENT;
    
    if (perm & IMAL_PERM_WRITE) {
        flags |= PAGE_FLAG_WRITE;
    }
    if (!(perm & IMAL_PERM_EXEC)) {
        flags |= PAGE_FLAG_NX;  /* 默认不可执行，显式设置才可执行 */
    }
    /* 用户态可访问 */
    flags |= PAGE_FLAG_USER;
    
    return flags;
}

/**
 * imal_domain_create - 创建隔离域
 * @return: 隔离域句柄，失败返回 NULL
 */
static inline imal_domain_t* imal_domain_create(void)
{
    imal_domain_t *dom = (imal_domain_t*)0;  /* TODO: 从 PMM 分配 */
    if (!dom) return NULL;
    
    dom->root = pagetable_create();
    if (!dom->root) {
        return NULL;
    }
    dom->owner = HIC_DOMAIN_CORE;
    dom->ref_count = 1;
    dom->flags = 0;
    
    return dom;
}

/**
 * imal_domain_destroy - 销毁隔离域
 * @dom: 隔离域句柄
 */
static inline void imal_domain_destroy(imal_domain_t *dom)
{
    if (dom && dom->root) {
        pagetable_destroy(dom->root);
        dom->root = (page_table_t*)0;
    }
}

/**
 * imal_map - 在域内映射物理内存
 * @dom: 隔离域句柄
 * @va: 虚拟地址
 * @pa: 物理地址
 * @size: 映射大小（字节）
 * @perm: 权限
 * @return: IMAL_OK 成功，其他失败
 */
static inline int imal_map(imal_domain_t *dom, vaddr_t va, paddr_t pa,
                           size_t size, imal_perm_t perm)
{
    if (!dom || !dom->root) return IMAL_ERR_INVAL;
    
    u64 flags = imal_perm_to_page_flags(perm);
    hic_status_t status = pagetable_map(dom->root, va, pa, size,
                                        (page_perm_t)flags, MAP_TYPE_USER);
    return (status == HIC_SUCCESS) ? IMAL_OK : IMAL_ERR_INVAL;
}

/**
 * imal_unmap - 撤销映射
 * @dom: 隔离域句柄
 * @va: 虚拟地址
 * @size: 映射大小
 * @return: IMAL_OK 成功，其他失败
 */
static inline int imal_unmap(imal_domain_t *dom, vaddr_t va, size_t size)
{
    if (!dom || !dom->root) return IMAL_ERR_INVAL;
    
    hic_status_t status = pagetable_unmap(dom->root, va, size);
    return (status == HIC_SUCCESS) ? IMAL_OK : IMAL_ERR_INVAL;
}

/**
 * imal_switch_to - 切换到目标隔离域
 * @dom: 目标隔离域
 * 
 * 切换当前执行流的地址空间
 */
static inline void imal_switch_to(imal_domain_t *dom)
{
    if (dom && dom->root) {
        pagetable_switch(dom->root);
    }
}

/**
 * imal_tlb_flush_all - 刷新全部 TLB
 */
static inline void imal_tlb_flush_all(void)
{
    pagetable_flush_tlb_all();
}

/**
 * imal_tlb_flush_domain - 刷新域相关 TLB
 * @dom: 隔离域句柄
 * 
 * x86_64 不支持 ASID，需要全量刷新
 */
static inline void imal_tlb_flush_domain(imal_domain_t *dom)
{
    (void)dom;
    pagetable_flush_tlb_all();
}

/**
 * imal_tlb_flush_va - 刷新单个虚拟地址的 TLB 条目
 * @va: 虚拟地址
 */
static inline void imal_tlb_flush_va(vaddr_t va)
{
    pagetable_flush_tlb(va);
}

#elif CONFIG_MPU == 1

/*
 * ============================================================
 * MPU 变体实现
 * 通过 MPU 区域实现隔离，domain_t 包含区域配置
 * ============================================================
 */

#include "nommu.h"

/* MPU 变体：隔离域结构 */
struct imal_domain {
    nommu_mpu_region_t regions[16];  /* MPU 区域配置 */
    u32                region_count; /* 区域数量 */
    domain_id_t        owner;        /* 所属域 ID */
    u32                flags;
};

/**
 * imal_domain_create - 创建隔离域（MPU 变体）
 */
static inline imal_domain_t* imal_domain_create(void)
{
    imal_domain_t *dom = (imal_domain_t*)0;  /* TODO: 从 PMM 分配 */
    if (!dom) return NULL;
    
    dom->region_count = 0;
    dom->owner = HIC_DOMAIN_CORE;
    dom->flags = 0;
    
    return dom;
}

/**
 * imal_domain_destroy - 销毁隔离域（MPU 变体）
 */
static inline void imal_domain_destroy(imal_domain_t *dom)
{
    if (dom) {
        dom->region_count = 0;
    }
}

/**
 * imal_map - 配置 MPU 区域（MPU 变体）
 * 
 * MPU 变体下，"映射"实际上是配置 MPU 区域
 */
static inline int imal_map(imal_domain_t *dom, vaddr_t va, paddr_t pa,
                           size_t size, imal_perm_t perm)
{
    if (!dom) return IMAL_ERR_INVAL;
    if (dom->region_count >= 16) return IMAL_ERR_NOMEM;
    
    /* MPU 区域配置 */
    u32 mpu_perm = 0;
    if (perm & IMAL_PERM_READ)    mpu_perm |= MPU_PERM_READ;
    if (perm & IMAL_PERM_WRITE)   mpu_perm |= MPU_PERM_WRITE;
    if (perm & IMAL_PERM_EXEC)    mpu_perm |= MPU_PERM_EXECUTE;
    
    hic_status_t status = nommu_mpu_config_region(dom->region_count, pa,
                                                   size, mpu_perm);
    if (status != HIC_SUCCESS) {
        return IMAL_ERR_INVAL;
    }
    
    dom->regions[dom->region_count].base = pa;
    dom->regions[dom->region_count].size = size;
    dom->regions[dom->region_count].permissions = mpu_perm;
    dom->regions[dom->region_count].enabled = true;
    dom->region_count++;
    
    return IMAL_OK;
}

/**
 * imal_unmap - 禁用 MPU 区域（MPU 变体）
 */
static inline int imal_unmap(imal_domain_t *dom, vaddr_t va, size_t size)
{
    if (!dom) return IMAL_ERR_INVAL;
    
    /* 查找并禁用匹配的区域 */
    for (u32 i = 0; i < dom->region_count; i++) {
        if (dom->regions[i].base == va && dom->regions[i].size == size) {
            dom->regions[i].enabled = false;
            return IMAL_OK;
        }
    }
    
    return IMAL_ERR_RANGE;
}

/**
 * imal_switch_to - 重载 MPU 寄存器（MPU 变体）
 */
static inline void imal_switch_to(imal_domain_t *dom)
{
    if (dom) {
        /* 重载 MPU 区域寄存器 */
        for (u32 i = 0; i < dom->region_count; i++) {
            if (dom->regions[i].enabled) {
                nommu_mpu_config_region(i, dom->regions[i].base,
                                        dom->regions[i].size,
                                        dom->regions[i].permissions);
            }
        }
    }
}

/**
 * imal_tlb_flush_all - MPU 无 TLB（空操作）
 */
static inline void imal_tlb_flush_all(void)
{
    /* MPU 没有 TLB，无需操作 */
}

/**
 * imal_tlb_flush_domain - MPU 无 TLB（空操作）
 */
static inline void imal_tlb_flush_domain(imal_domain_t *dom)
{
    (void)dom;
    /* MPU 没有 TLB，无需操作 */
}

/**
 * imal_tlb_flush_va - MPU 无 TLB（空操作）
 */
static inline void imal_tlb_flush_va(vaddr_t va)
{
    (void)va;
    /* MPU 没有 TLB，无需操作 */
}

#else

/*
 * ============================================================
 * noMMU/noMPU 变体实现（Safe 变体）
 * 无硬件隔离，仅通过软件能力系统保护
 * ============================================================
 */

#include "nommu.h"

/* Safe 变体：隔离域结构（仅记录权限，无硬件隔离） */
struct imal_domain {
    domain_id_t owner;        /* 所属域 ID */
    u32         flags;        /* 标志位 */
    /* 不需要实际映射记录，因为虚拟地址 = 物理地址 */
};

/**
 * imal_domain_create - 创建隔离域（Safe 变体）
 * 
 * 返回一个标记域，无实际硬件隔离
 */
static inline imal_domain_t* imal_domain_create(void)
{
    imal_domain_t *dom = (imal_domain_t*)0;  /* TODO: 从 PMM 分配 */
    if (!dom) return NULL;
    
    dom->owner = HIC_DOMAIN_CORE;
    dom->flags = 0;
    
    return dom;
}

/**
 * imal_domain_destroy - 销毁隔离域（Safe 变体）
 */
static inline void imal_domain_destroy(imal_domain_t *dom)
{
    /* 无操作 */
    (void)dom;
}

/**
 * imal_map - 记录权限（Safe 变体）
 * 
 * 无实际映射操作，恒等映射始终存在
 * 仅通过软件能力系统检查访问权限
 */
static inline int imal_map(imal_domain_t *dom, vaddr_t va, paddr_t pa,
                           size_t size, imal_perm_t perm)
{
    (void)dom;
    (void)va;
    (void)pa;
    (void)size;
    (void)perm;
    
    /* 恒等映射，无需操作 */
    /* 权限检查由能力系统负责 */
    return IMAL_OK;
}

/**
 * imal_unmap - 空操作（Safe 变体）
 */
static inline int imal_unmap(imal_domain_t *dom, vaddr_t va, size_t size)
{
    (void)dom;
    (void)va;
    (void)size;
    
    return IMAL_OK;
}

/**
 * imal_switch_to - 空操作（Safe 变体）
 * 
 * 无地址空间切换
 */
static inline void imal_switch_to(imal_domain_t *dom)
{
    /* 无操作：恒等映射，无需切换 */
    (void)dom;
}

/**
 * imal_tlb_flush_all - 空操作（Safe 变体）
 */
static inline void imal_tlb_flush_all(void)
{
    /* 无 TLB，无需刷新 */
}

/**
 * imal_tlb_flush_domain - 空操作（Safe 变体）
 */
static inline void imal_tlb_flush_domain(imal_domain_t *dom)
{
    (void)dom;
}

/**
 * imal_tlb_flush_va - 空操作（Safe 变体）
 */
static inline void imal_tlb_flush_va(vaddr_t va)
{
    (void)va;
}

#endif /* CONFIG_MMU / CONFIG_MPU / noMMU */

/* ===== 跨变体统一接口 ===== */

/**
 * imal_get_phys - 获取虚拟地址对应的物理地址
 * @dom: 隔离域句柄
 * @va: 虚拟地址
 * @return: 物理地址，失败返回 0
 */
static inline paddr_t imal_get_phys(imal_domain_t *dom, vaddr_t va)
{
#if CONFIG_MMU == 1
    if (dom && dom->root) {
        return pagetable_get_phys(dom->root, va);
    }
    return 0;
#else
    /* noMMU/MPU：恒等映射 */
    (void)dom;
    return (paddr_t)va;
#endif
}

/**
 * imal_set_owner - 设置隔离域的所有者
 * @dom: 隔离域句柄
 * @owner: 所有者域 ID
 */
static inline void imal_set_owner(imal_domain_t *dom, domain_id_t owner)
{
    if (dom) {
        dom->owner = owner;
    }
}

/**
 * imal_get_owner - 获取隔离域的所有者
 * @dom: 隔离域句柄
 * @return: 所有者域 ID
 */
static inline domain_id_t imal_get_owner(imal_domain_t *dom)
{
    return dom ? dom->owner : HIC_DOMAIN_CORE;
}

/* ===== 初始化接口 ===== */

/**
 * imal_init - 初始化 IMAL 层
 * 
 * 根据编译配置初始化对应的隔离机制
 */
void imal_init(void);

/**
 * imal_get_variant_name - 获取当前变体名称
 * @return: 变体名称字符串
 */
static inline const char* imal_get_variant_name(void)
{
#if CONFIG_MMU == 1
    return "MMU";
#elif CONFIG_MPU == 1
    return "MPU";
#else
    return "Safe(noMMU/noMPU)";
#endif
}

#endif /* HIC_KERNEL_IMAL_H */
