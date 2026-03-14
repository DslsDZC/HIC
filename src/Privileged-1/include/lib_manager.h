/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC共享库管理服务接口
 * 
 * 共享库管理服务（libmanager.hicmod）运行在 Privileged-1 层，
 * 负责库的注册、查询、引用计数和内存管理。
 * 
 * 设计原则：
 * - 机制与策略分离：Core-0 提供底层原语，本服务负责策略
 * - 最小权限：服务只能访问其持有的共享库能力
 * - 物理共享：多个服务共享同一物理页框
 * - 版本化：支持多版本并行存在
 */

#ifndef HIC_LIB_MANAGER_H
#define HIC_LIB_MANAGER_H

#include "hiclib.h"
#include "stdint.h"
/* bool类型由common.h定义 */

/* ========== 服务端点定义 ========== */

/* 端点ID基址 */
#define LIB_ENDPOINT_BASE           0x6800

/* 端点定义 */
#define LIB_ENDPOINT_REGISTER       (LIB_ENDPOINT_BASE + 0)  /* 注册共享库 */
#define LIB_ENDPOINT_LOOKUP         (LIB_ENDPOINT_BASE + 1)  /* 查询库能力 */
#define LIB_ENDPOINT_REFERENCE      (LIB_ENDPOINT_BASE + 2)  /* 增加引用计数 */
#define LIB_ENDPOINT_RELEASE        (LIB_ENDPOINT_BASE + 3)  /* 减少引用计数 */
#define LIB_ENDPOINT_UPDATE         (LIB_ENDPOINT_BASE + 4)  /* 升级库版本 */
#define LIB_ENDPOINT_QUERY_SYMBOL   (LIB_ENDPOINT_BASE + 5)  /* 查询符号 */
#define LIB_ENDPOINT_LIST           (LIB_ENDPOINT_BASE + 6)  /* 列出库 */
#define LIB_ENDPOINT_GET_STATS      (LIB_ENDPOINT_BASE + 7)  /* 获取统计 */
#define LIB_ENDPOINT_UNLOAD         (LIB_ENDPOINT_BASE + 8)  /* 卸载库 */

/* ========== 请求/响应结构 ========== */

/**
 * 注册库请求
 */
typedef struct lib_register_request {
    const void     *lib_data;          /* 库文件数据指针 */
    uint32_t        lib_size;          /* 库文件大小 */
    uint32_t        flags;             /* 注册标志 */
    uint8_t         requester_domain;  /* 请求者域ID */
} lib_register_request_t;

/* 注册标志 */
#define LIB_REG_FLAG_VERIFY_SIG    (1U << 0)   /* 验证签名 */
#define LIB_REG_FLAG_CACHE         (1U << 1)   /* 缓存二进制 */
#define LIB_REG_FLAG_DEPRECATED    (1U << 2)   /* 标记为弃用 */

/**
 * 注册库响应
 */
typedef struct lib_register_response {
    int32_t         status;            /* 状态码: 0=成功, 负数=错误 */
    uint8_t         lib_uuid[16];      /* 库UUID */
    uint32_t        version;           /* 版本号（打包） */
    uint64_t        code_base;         /* 代码段物理基址 */
    uint64_t        code_size;         /* 代码段大小 */
    uint64_t        rodata_base;       /* 只读数据段基址 */
    uint64_t        rodata_size;       /* 只读数据段大小 */
} lib_register_response_t;

/**
 * 查询库请求
 */
typedef struct lib_lookup_request {
    uint8_t         uuid[16];          /* 库UUID (可选) */
    char            name[32];          /* 库名称 (可选) */
    uint32_t        min_version;       /* 最小版本（打包） */
    uint32_t        max_version;       /* 最大版本（打包） */
    uint8_t         requester_domain;  /* 请求者域ID */
    uint8_t         reserved[7];
} lib_lookup_request_t;

/**
 * 查询库响应
 */
typedef struct lib_lookup_response {
    int32_t         status;            /* 状态码 */
    uint8_t         lib_uuid[16];      /* 库UUID */
    uint32_t        version;           /* 版本号 */
    uint64_t        code_cap;          /* 代码段能力句柄 */
    uint64_t        rodata_cap;        /* 只读数据段能力句柄 */
    uint32_t        symbol_count;      /* 符号数量 */
    uint32_t        ref_count;         /* 当前引用计数 */
} lib_lookup_response_t;

/**
 * 引用操作请求
 */
typedef struct lib_ref_request {
    uint8_t         uuid[16];          /* 库UUID */
    uint32_t        version;           /* 版本号（打包） */
    uint8_t         requester_domain;  /* 请求者域ID */
    uint8_t         operation;         /* 操作: 0=增加, 1=减少 */
    uint8_t         reserved[6];
} lib_ref_request_t;

/**
 * 引用操作响应
 */
typedef struct lib_ref_response {
    int32_t         status;            /* 状态码 */
    uint32_t        ref_count;         /* 更新后的引用计数 */
} lib_ref_response_t;

/**
 * 符号查询请求
 */
typedef struct lib_symbol_request {
    uint8_t         uuid[16];          /* 库UUID */
    uint32_t        version;           /* 版本号（打包） */
    char            symbol_name[48];   /* 符号名称 */
    uint8_t         reserved[4];
} lib_symbol_request_t;

/**
 * 符号查询响应
 */
typedef struct lib_symbol_response {
    int32_t         status;            /* 状态码 */
    uint64_t        offset;            /* 符号偏移（段内） */
    uint32_t        size;              /* 符号大小 */
    uint8_t         type;              /* 符号类型 */
    uint8_t         segment_index;     /* 所属段索引 */
} lib_symbol_response_t;

/**
 * 更新请求
 */
typedef struct lib_update_request {
    uint8_t         old_uuid[16];      /* 旧库UUID */
    uint32_t        old_version;       /* 旧版本 */
    uint8_t         new_uuid[16];      /* 新库UUID */
    uint32_t        new_version;       /* 新版本 */
    uint8_t         requester_domain;  /* 请求者域ID */
    uint8_t         reserved[7];
} lib_update_request_t;

/**
 * 更新响应
 */
typedef struct lib_update_response {
    int32_t         status;            /* 状态码 */
    uint64_t        new_code_cap;      /* 新代码段能力句柄 */
    uint64_t        new_rodata_cap;    /* 新只读数据段能力句柄 */
    uint32_t        migrated_refs;     /* 已迁移的引用数 */
} lib_update_response_t;

/**
 * 统计信息
 */
typedef struct lib_stats {
    uint32_t        total_libraries;   /* 总库数 */
    uint32_t        loaded_libraries;  /* 已加载数 */
    uint32_t        active_references; /* 活跃引用数 */
    uint64_t        total_code_size;   /* 总代码段大小 */
    uint64_t        total_rodata_size; /* 总只读数据大小 */
    uint32_t        cache_hits;        /* 缓存命中 */
    uint32_t        cache_misses;      /* 缓存未命中 */
} lib_stats_t;

/**
 * 库列表项
 */
typedef struct lib_list_item {
    uint8_t         uuid[16];          /* 库UUID */
    char            name[32];          /* 库名称 */
    uint32_t        version;           /* 版本号 */
    hiclib_state_t  state;             /* 状态 */
    uint32_t        ref_count;         /* 引用计数 */
    uint64_t        code_size;         /* 代码段大小 */
} lib_list_item_t;

/**
 * 库列表响应
 */
typedef struct lib_list_response {
    int32_t         status;            /* 状态码 */
    uint32_t        count;             /* 库数量 */
    lib_list_item_t libraries[16];     /* 库列表 */
} lib_list_response_t;

/* ========== 错误码定义 ========== */

#define LIB_SUCCESS             0
#define LIB_ERR_INVALID_PARAM   (-1)
#define LIB_ERR_NOT_FOUND       (-2)
#define LIB_ERR_VERSION_MISMATCH (-3)
#define LIB_ERR_SIGNATURE_INVALID (-4)
#define LIB_ERR_NO_MEMORY       (-5)
#define LIB_ERR_ALREADY_EXISTS  (-6)
#define LIB_ERR_IN_USE          (-7)
#define LIB_ERR_PERMISSION      (-8)
#define LIB_ERR_DEPENDENCY      (-9)

/* ========== 库实例结构（内部使用） ========== */

#define LIB_MAX_INSTANCES   64
#define LIB_MAX_NAME_LEN    32
#define LIB_MAX_SYMBOLS     256

/**
 * 已加载库实例
 */
typedef struct lib_instance {
    uint8_t         uuid[16];          /* 库UUID */
    char            name[LIB_MAX_NAME_LEN]; /* 库名称 */
    uint32_t        major;             /* 主版本号 */
    uint32_t        minor;             /* 次版本号 */
    uint32_t        patch;             /* 补丁版本号 */
    
    /* 内存布局 */
    uint64_t        code_base;         /* 代码段物理基址 */
    size_t          code_size;         /* 代码段大小 */
    uint64_t        rodata_base;       /* 只读数据段物理基址 */
    size_t          rodata_size;       /* 只读数据段大小 */
    size_t          data_size;         /* 读写数据段大小（不共享） */
    
    /* 符号表 */
    const hiclib_symbol_t *symbols;    /* 符号表指针 */
    uint32_t        symbol_count;      /* 符号数量 */
    
    /* 引用管理 */
    uint32_t        ref_count;         /* 引用计数 */
    uint8_t         ref_domains[32];   /* 引用此库的域ID列表 */
    
    /* 状态 */
    hiclib_state_t  state;             /* 库状态 */
    uint64_t        load_time;         /* 加载时间戳 */
    
    /* 能力句柄（用于传递给服务） */
    uint64_t        code_cap_handle;   /* 代码段能力句柄 */
    uint64_t        rodata_cap_handle; /* 只读数据段能力句柄 */
    
    /* 依赖 */
    uint8_t         dependencies[8][16]; /* 依赖库UUID列表 */
    uint32_t        dep_count;
    
    /* 缓存 */
    const void     *cached_data;       /* 缓存的二进制数据 */
    size_t          cached_size;       /* 缓存大小 */
    
    /* 链表 */
    struct lib_instance *next;
} lib_instance_t;

/* ========== 全局管理器结构 ========== */

typedef struct lib_manager {
    lib_instance_t  instances[LIB_MAX_INSTANCES];  /* 库实例数组 */
    uint32_t        instance_count;                 /* 实例数量 */
    lib_stats_t     stats;                          /* 统计信息 */
    bool            initialized;                    /* 已初始化 */
} lib_manager_t;

/* ========== 服务接口函数 ========== */

/**
 * 初始化库管理器
 */
void lib_manager_init(void);

/**
 * 注册共享库
 */
int lib_register(const lib_register_request_t *req, lib_register_response_t *resp);

/**
 * 查询库
 */
int lib_lookup(const lib_lookup_request_t *req, lib_lookup_response_t *resp);

/**
 * 增加引用
 */
int lib_reference(const lib_ref_request_t *req, lib_ref_response_t *resp);

/**
 * 减少引用
 */
int lib_release(const lib_ref_request_t *req, lib_ref_response_t *resp);

/**
 * 查询符号
 */
int lib_query_symbol(const lib_symbol_request_t *req, lib_symbol_response_t *resp);

/**
 * 列出库
 */
int lib_list(lib_list_response_t *resp);

/**
 * 获取统计
 */
int lib_get_stats(lib_stats_t *stats);

/**
 * 卸载库
 */
int lib_unload(const uint8_t uuid[16], uint32_t version);

/**
 * 滚动更新库
 */
int lib_update(const lib_update_request_t *req, lib_update_response_t *resp);

#endif /* HIC_LIB_MANAGER_H */
