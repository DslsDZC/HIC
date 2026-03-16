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
#define HICMOD_MAGIC 0x48494B4D  /* "HICM" */
#define HICMOD_VERSION 2         /* 版本2支持多架构 */

/* 架构标识符 */
#define HICMOD_ARCH_X86_64    0x01  /* x86-64 (AMD64) */
#define HICMOD_ARCH_AARCH64   0x02  /* ARM64 (AArch64) */
#define HICMOD_ARCH_RISCV64   0x03  /* RISC-V 64-bit */
#define HICMOD_ARCH_ARM32     0x04  /* ARM 32-bit */
#define HICMOD_ARCH_RISCV32   0x05  /* RISC-V 32-bit */
#define HICMOD_ARCH_MAX       8     /* 最大架构数量 */

/* 架构段描述符 - 每个架构一份 */
typedef struct hicmod_arch_section {
    u32 arch_id;                /* 架构标识符 (HICMOD_ARCH_*) */
    u32 flags;                  /* 架构特定标志 */
    u32 code_offset;            /* 代码段偏移（相对于文件起始） */
    u32 code_size;              /* 代码段大小 */
    u32 data_offset;            /* 数据段偏移 */
    u32 data_size;              /* 数据段大小 */
    u32 bss_size;               /* BSS段大小（运行时分配） */
    u32 rodata_offset;          /* 只读数据偏移 */
    u32 rodata_size;            /* 只读数据大小 */
    u32 entry_offset;           /* 入口点偏移（相对于代码段） */
    u32 reloc_offset;           /* 重定位表偏移 */
    u32 reloc_count;            /* 重定位项数量 */
    u32 reserved[4];            /* 保留 */
} hicmod_arch_section_t;

/* 模块头部 - 多架构布局 */
typedef struct hicmod_header {
    u32 magic;                  /* 魔数 HICMOD_MAGIC */
    u32 version;                /* 格式版本 */
    u8 uuid[16];                /* 模块UUID */
    u32 semantic_version;       /* 语义化版本 */
    u32 api_version;            /* API版本 */
    u32 header_size;            /* 头部大小 */
    
    /* 多架构支持 */
    u32 arch_count;             /* 包含的架构数量 */
    u32 arch_table_offset;      /* 架构表偏移 */
    u32 arch_table_size;        /* 架构表大小 */
    
    /* 元数据（架构中立） */
    u32 metadata_offset;        /* 元数据偏移 */
    u32 metadata_size;          /* 元数据大小 */
    
    /* 符号表（架构中立，符号名映射） */
    u32 symbol_offset;          /* 符号表偏移 */
    u32 symbol_size;            /* 符号表大小 */
    
    /* 安全签名 */
    u32 signature_offset;       /* 签名偏移 */
    u32 signature_size;         /* 签名大小 */
    
    /* 兼容性：单一架构快捷访问（arch_count==1时使用） */
    u32 legacy_code_offset;
    u32 legacy_code_size;
    u32 legacy_data_offset;
    u32 legacy_data_size;
    
    u32 reserved[4];            /* 保留 */
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