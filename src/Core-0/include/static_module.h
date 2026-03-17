/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核静态模块系统
 * 
 * 静态模块特性：
 * - 固定地址段，在内核内存之外
 * - 代码在原位置执行（execute in place）
 * - 支持卸载，内存可重用
 * - init_launcher 用完后可释放，纳入动态池
 * 
 * 内存布局：
 * ┌─────────────────┐ 0x100000
 * │ 内核 (Core-0)    │
 * └─────────────────┘ _kernel_end
 * ┌─────────────────┐ 0x200000 (STATIC_MODULES_BASE)
 * │ 静态模块段       │
 * │ - verifier      │
 * │ - ide_driver    │
 * │ - fat32_service │
 * │ - memory_service│
 * │ - device_manager│
 * │ - security_mon  │
 * │ - init_launcher │ ← 最后加载，用完可卸载
 * └─────────────────┘ _static_modules_region_end
 *         ↓
 *   卸载后内存 → 动态池
 * 
 * 信任链：
 * 内核 → verifier → ide_driver → fat32_service → init_launcher
 *      → module_manager_service（动态）→ 其他动态模块
 */

#ifndef STATIC_MODULE_H
#define STATIC_MODULE_H

#include "../types.h"
#include "../capability.h"

/* ==================== 内存布局常量 ==================== */

/* 静态模块段基地址（在内核之外） */
#define STATIC_MODULES_BASE     0x200000

/* 静态模块优先级定义 */
#define STATIC_MODULE_PRIORITY_CRITICAL   0   /* 关键服务：最先启动 */
#define STATIC_MODULE_PRIORITY_HIGH       1   /* 高优先级：驱动服务 */
#define STATIC_MODULE_PRIORITY_NORMAL     2   /* 普通优先级：一般服务 */
#define STATIC_MODULE_PRIORITY_LOW        3   /* 低优先级：init_launcher */

/* 静态模块描述符 */
typedef struct static_module_desc {
    char     name[32];           /* 服务名称 */
    u32      type;               /* 服务类型 */
    u32      version;            /* 版本号 */
    u32      priority;           /* 启动优先级 (0=最高, 3=最低) */
    u32      _pad;               /* 填充，保持对齐 */
    void    *code_start;         /* 代码起始地址 */
    void    *code_end;           /* 代码结束地址 */
    void    *data_start;         /* 数据起始地址 */
    void    *data_end;           /* 数据结束地址 */
    u64      entry_offset;       /* 入口点偏移（相对于代码起始） */
    u64      capabilities[8];    /* 需要授予的初始能力 */
    u64      flags;              /* 标志位 */
} static_module_desc_t;

/* 模块类型 */
#define STATIC_MODULE_TYPE_DRIVER      1
#define STATIC_MODULE_TYPE_SERVICE     2
#define STATIC_MODULE_TYPE_SYSTEM      3
#define STATIC_MODULE_TYPE_USER        4

/* 模块标志 */
#define STATIC_MODULE_FLAG_CRITICAL    (1ULL << 0)  /* 关键服务，必须启动 */
#define STATIC_MODULE_FLAG_AUTO_START  (1ULL << 1)  /* 自动启动 */
#define STATIC_MODULE_FLAG_PRIVILEGED  (1ULL << 2)  /* 特权服务 */
#define STATIC_MODULE_FLAG_UNLOADABLE  (1ULL << 3)  /* 可卸载 */

/* 模块运行时状态 */
typedef enum {
    STATIC_MODULE_STATE_UNLOADED = 0,
    STATIC_MODULE_STATE_LOADING,
    STATIC_MODULE_STATE_RUNNING,
    STATIC_MODULE_STATE_UNLOADING,
    STATIC_MODULE_STATE_UNLOADED_MEM_FREED,  /* 已卸载，内存已释放 */
} static_module_state_t;

/* 外部符号声明（链接脚本定义） */
extern static_module_desc_t __static_modules_start;
extern static_module_desc_t __static_modules_end;

/* 静态模块内存区域（链接脚本定义） */
extern char _static_modules_region_start[];
extern char _static_modules_region_end[];

/* ==================== 初始化 ==================== */

/**
 * 初始化静态模块系统
 */
void static_module_system_init(void);

/* ==================== 加载流程 ==================== */

/**
 * 加载所有静态模块
 * 
 * @return 成功加载的模块数量
 */
int static_module_load_all(void);

/* ==================== 卸载接口 ==================== */

/**
 * 卸载静态模块
 * 
 * 停止模块线程，释放域和能力，
 * 内存可纳入动态池
 * 
 * @param name 模块名称
 * @return 状态码
 */
hic_status_t static_module_unload(const char *name);

/**
 * 卸载 init_launcher（引导完成后调用）
 * 
 * init_launcher 完成引导 module_manager 后，
 * 调用此函数释放其内存，纳入动态池
 * 
 * @return 状态码
 */
hic_status_t static_module_unload_init_launcher(void);

/**
 * 释放已卸载模块的内存到动态池
 * 
 * @return 释放的字节数
 */
u64 static_module_release_memory(void);

/**
 * 获取可释放的静态模块内存大小
 */
u64 static_module_get_reclaimable_memory(void);

/* ==================== 加载步骤 ==================== */

int static_module_create_sandbox_ex(static_module_desc_t *module, u32 runtime_idx);
int static_module_setup_capabilities(static_module_desc_t *module, u32 runtime_idx);
int static_module_register_service(static_module_desc_t *module, u32 runtime_idx);
int static_module_start_ex(static_module_desc_t *module, u32 runtime_idx);

/* ==================== 查询 ==================== */

static_module_desc_t* static_module_find(const char *name);
domain_id_t static_module_get_domain(const char *name);
bool static_module_is_running(const char *name);
static_module_state_t static_module_get_state(const char *name);
struct service_endpoint* static_module_get_service(const char *name);
hic_status_t static_module_get_endpoint_handle(const char *name, cap_handle_t *handle);

/* ==================== 内存信息 ==================== */

/**
 * 获取静态模块段起始地址
 */
static inline void* static_module_region_start(void) {
    return (void*)_static_modules_region_start;
}

/**
 * 获取静态模块段结束地址
 */
static inline void* static_module_region_end(void) {
    return (void*)_static_modules_region_end;
}

/**
 * 获取静态模块段大小
 */
static inline u64 static_module_region_size(void) {
    return (u64)(_static_modules_region_end - _static_modules_region_start);
}

#endif /* STATIC_MODULE_H */
