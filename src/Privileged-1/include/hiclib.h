/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC共享库格式定义
 * 
 * HIC共享库（.hiclib）是一种受能力保护的、可重定位的代码段集合，
 * 由专门的共享库服务进行管理。支持：
 * - 高效共享：多个服务可共用同一份库代码
 * - 安全隔离：库的代码和数据受能力系统控制
 * - 版本兼容：支持多版本共存
 * - 动态更新：支持库的热升级
 */

#ifndef HIC_HICLIB_H
#define HIC_HICLIB_H

#include "stdint.h"
#include "stddef.h"
/* bool类型由common.h定义 */

/* ========== 常量定义 ========== */

#define HICLIB_MAGIC        "HICLIB\0"   /* 文件魔数 (8字节) */
#define HICLIB_VERSION      1            /* 文件格式版本 */

/* 架构类型 */
#define HICLIB_ARCH_X86_64  1
#define HICLIB_ARCH_ARM64   2
#define HICLIB_ARCH_RISCV   3

/* 段类型 */
#define HICLIB_SEG_CODE     1   /* 代码段 (RX, 可共享) */
#define HICLIB_SEG_RODATA   2   /* 只读数据段 (R, 可共享) */
#define HICLIB_SEG_DATA     3   /* 读写数据段 (RW, 不共享，每服务私有副本) */

/* 段标志 */
#define HICLIB_SEG_EXEC     (1U << 0)   /* 可执行 */
#define HICLIB_SEG_READ     (1U << 1)   /* 可读 */
#define HICLIB_SEG_WRITE    (1U << 2)   /* 可写 */
#define HICLIB_SEG_SHARED   (1U << 3)   /* 可共享 */

/* 符号类型 */
#define HICLIB_SYM_FUNC     0   /* 函数 */
#define HICLIB_SYM_DATA     1   /* 数据 */
#define HICLIB_SYM_WEAK     2   /* 弱符号 */

/* 库状态 */
typedef enum hiclib_state {
    HICLIB_STATE_UNLOADED = 0,    /* 未加载 */
    HICLIB_STATE_LOADING,         /* 加载中 */
    HICLIB_STATE_LOADED,          /* 已加载 */
    HICLIB_STATE_ACTIVE,          /* 活跃（有引用） */
    HICLIB_STATE_DEPRECATED,      /* 已弃用（等待卸载） */
    HICLIB_STATE_ERROR            /* 错误 */
} hiclib_state_t;

/* ========== 文件格式结构 ========== */

/**
 * 共享库文件头 (128字节)
 */
typedef struct hiclib_header {
    char        magic[8];           /* 魔数 "HICLIB\0" */
    uint16_t    format_version;     /* 文件格式版本 */
    uint16_t    arch;               /* 架构类型 */
    uint8_t     uuid[16];           /* 全局唯一ID */
    
    /* 语义化版本号 */
    uint32_t    major;              /* 主版本号 */
    uint32_t    minor;              /* 次版本号 */
    uint32_t    patch;              /* 补丁版本号 */
    
    /* 段表 */
    uint64_t    segment_offset;     /* 段表偏移 */
    uint32_t    segment_count;      /* 段表项数 */
    
    /* 符号表 */
    uint64_t    symbol_offset;      /* 符号表偏移 */
    uint32_t    symbol_count;       /* 符号表项数 */
    
    /* 依赖表 */
    uint64_t    dep_offset;         /* 依赖表偏移 */
    uint32_t    dep_count;          /* 依赖项数 */
    
    /* 签名 */
    uint64_t    signature_offset;   /* 签名偏移 */
    uint32_t    signature_size;     /* 签名大小 */
    
    /* 元数据 */
    char        name[32];           /* 库名称 */
    char        display_name[64];   /* 显示名称 */
    
    /* 保留 */
    uint8_t     reserved[24];
} __attribute__((packed)) hiclib_header_t;

/**
 * 段表项 (40字节)
 */
typedef struct hiclib_segment {
    uint32_t    type;               /* 段类型 */
    uint32_t    flags;              /* 段标志 */
    uint64_t    file_offset;        /* 在文件中的偏移 */
    uint64_t    file_size;          /* 文件中的大小 */
    uint64_t    mem_size;           /* 内存大小（页对齐） */
    uint64_t    align;              /* 对齐要求 */
} __attribute__((packed)) hiclib_segment_t;

/**
 * 符号表项 (64字节)
 */
typedef struct hiclib_symbol {
    char        name[48];           /* 符号名称 */
    uint64_t    offset;             /* 段内偏移 */
    uint32_t    size;               /* 大小 */
    uint8_t     type;               /* 符号类型 */
    uint8_t     segment_index;      /* 所属段索引 */
    uint16_t    version;            /* 符号版本 */
    uint8_t     reserved[6];
} __attribute__((packed)) hiclib_symbol_t;

/**
 * 依赖项 (48字节)
 */
typedef struct hiclib_dependency {
    uint8_t     uuid[16];           /* 依赖库UUID */
    uint32_t    min_major;          /* 最小主版本 */
    uint32_t    min_minor;          /* 最小次版本 */
    uint32_t    max_major;          /* 最大主版本 */
    uint32_t    max_minor;          /* 最大次版本 */
    char        name[16];           /* 依赖库名称（可选） */
    uint8_t     reserved[4];
} __attribute__((packed)) hiclib_dependency_t;

/* ========== 版本工具 ========== */

/**
 * 打包版本号为uint32_t
 * 格式: [31:24] major, [23:12] minor, [11:0] patch
 */
static inline uint32_t hiclib_pack_version(uint32_t major, uint32_t minor, uint32_t patch)
{
    return ((major & 0xFF) << 24) | ((minor & 0xFFF) << 12) | (patch & 0xFFF);
}

/**
 * 解包版本号
 */
static inline void hiclib_unpack_version(uint32_t packed, uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (major) *major = (packed >> 24) & 0xFF;
    if (minor) *minor = (packed >> 12) & 0xFFF;
    if (patch) *patch = packed & 0xFFF;
}

/**
 * 版本比较
 * @return 负数 if a < b, 0 if a == b, 正数 if a > b
 */
static inline int hiclib_compare_version(uint32_t major_a, uint32_t minor_a, uint32_t patch_a,
                                         uint32_t major_b, uint32_t minor_b, uint32_t patch_b)
{
    if (major_a != major_b) return (int)major_a - (int)major_b;
    if (minor_a != minor_b) return (int)minor_a - (int)minor_b;
    return (int)patch_a - (int)patch_b;
}

/**
 * 检查版本是否满足约束
 */
static inline bool hiclib_version_satisfies(uint32_t major, uint32_t minor, uint32_t patch,
                                            uint32_t min_major, uint32_t min_minor,
                                            uint32_t max_major, uint32_t max_minor)
{
    /* 检查最小版本 */
    if (min_major > 0 || min_minor > 0) {
        if (hiclib_compare_version(major, minor, patch, min_major, min_minor, 0) < 0) {
            return false;
        }
    }
    
    /* 检查最大版本 */
    if (max_major > 0 || max_minor > 0) {
        if (hiclib_compare_version(major, minor, patch, max_major, max_minor, 0) >= 0) {
            return false;
        }
    }
    
    return true;
}

/**
 * 验证文件头
 */
static inline bool hiclib_header_valid(const hiclib_header_t *header)
{
    if (!header) return false;
    
    /* 检查魔数 */
    for (int i = 0; i < 7; i++) {
        if (header->magic[i] != HICLIB_MAGIC[i]) {
            return false;
        }
    }
    
    /* 检查版本 */
    if (header->format_version > HICLIB_VERSION) {
        return false;
    }
    
    /* 检查架构 */
    if (header->arch != HICLIB_ARCH_X86_64 &&
        header->arch != HICLIB_ARCH_ARM64 &&
        header->arch != HICLIB_ARCH_RISCV) {
        return false;
    }
    
    return true;
}

#endif /* HIC_HICLIB_H */
