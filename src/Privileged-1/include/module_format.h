/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC模块格式定义
 * 用于.hicmod二进制模块
 */

#ifndef HIC_MODULE_FORMAT_H
#define HIC_MODULE_FORMAT_H

#include "../Core-0/types.h"

/* HIC模块魔数 */
#define HICMOD_MAGIC 0x48494B4D  /* "HKMD" */
#define HICMOD_VERSION 1

/* 模块头部 */
typedef struct hicmod_header {
    u32 magic;                  /* 魔数 */
    u32 version;                /* 格式版本 */
    u8 uuid[16];                /* 模块UUID */
    u32 semantic_version;       /* 语义化版本 */
    u32 api_version;            /* API版本 */
    u32 header_size;            /* 头部大小 */
    u32 metadata_offset;        /* 元数据偏移 */
    u32 metadata_size;          /* 元数据大小 */
    u32 code_offset;            /* 代码段偏移 */
    u32 code_size;              /* 代码段大小 */
    u32 data_offset;            /* 数据段偏移 */
    u32 data_size;              /* 数据段大小 */
    u32 bss_offset;             /* BSS段偏移 */
    u32 bss_size;               /* BSS段大小 */
    u32 rodata_offset;          /* 只读数据偏移 */
    u32 rodata_size;            /* 只读数据大小 */
    u32 symbol_offset;          /* 符号表偏移 */
    u32 symbol_size;            /* 符号表大小 */
    u32 signature_offset;       /* 签名偏移 */
    u32 signature_size;         /* 签名大小 */
    u32 reserved[8];            /* 保留 */
} hicmod_header_t;

/* 元数据头部 */
typedef struct hicmod_metadata {
    char name[64];              /* 模块名称 */
    char display_name[128];     /* 显示名称 */
    char version[16];           /* 版本 */
    char api_version[16];       /* API版本 */
    u8 uuid[16];                /* UUID */
    
    /* 作者信息 */
    char author_name[64];
    char author_email[128];
    char license[128];
    
    /* 描述 */
    char short_desc[256];
    char long_desc[1024];
    
    /* 依赖 */
    u32 dependency_count;
    
    /* 资源 */
    u64 max_memory;
    u32 max_threads;
    u32 max_capabilities;
    u32 cpu_quota_percent;
    
    /* 端点 */
    u32 endpoint_count;
    
    /* 权限 */
    u32 permission_count;
    
    /* 安全 */
    bool critical;
    bool privileged;
    bool signature_required;
    
    /* 编译 */
    bool static_build;
    u32 priority;
    bool autostart;
} hicmod_metadata_t;

/* 依赖项 */
typedef struct hicmod_dependency {
    char service_name[64];
    u32 min_version;
    u32 max_version;
} hicmod_dependency_t;

/* 端点描述符 */
typedef struct hicmod_endpoint {
    char name[64];
    u32 version;
    u64 entry_point;
} hicmod_endpoint_t;

/* 权限描述符 */
typedef struct hicmod_permission {
    char name[64];
    bool required;
} hicmod_permission_t;

/* 符号表项 */
typedef struct hicmod_symbol {
    char name[128];
    u64 address;
    u32 size;
    u8 type;  /* 0=代码, 1=数据, 2=BSS */
} hicmod_symbol_t;

#endif /* HIC_MODULE_FORMAT_H */