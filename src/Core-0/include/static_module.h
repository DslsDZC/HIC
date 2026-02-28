/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核静态模块系统
 * 静态链接到内核中的服务模块
 */

#ifndef STATIC_MODULE_H
#define STATIC_MODULE_H

#include "types.h"

/* 静态模块描述符 */
typedef struct static_module_desc {
    char     name[32];           /* 服务名称 */
    u32      type;               /* 服务类型：1=驱动, 2=系统服务, 3=用户服务 */
    u32      version;            /* 版本号 */
    void    *code_start;         /* 代码起始地址 */
    void    *code_end;           /* 代码结束地址 */
    void    *data_start;         /* 数据起始地址 */
    void    *data_end;           /* data结束地址 */
    u64      entry_offset;       /* 入口点偏移（相对于代码起始） */
    u64      capabilities[8];     /* 需要授予的初始能力 */
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

/* 外部符号声明 */
extern static_module_desc_t __static_modules_start;
extern static_module_desc_t __static_modules_end;

/* 函数声明 */

/**
 * 初始化静态模块系统
 */
void static_module_system_init(void);

/**
 * 加载所有静态模块
 */
int static_module_load_all(void);

/**
 * 创建静态模块的沙箱
 */
int static_module_create_sandbox(static_module_desc_t *module);

/**
 * 启动静态模块
 */
int static_module_start(static_module_desc_t *module);

/**
 * 查找静态模块
 */
static_module_desc_t* static_module_find(const char *name);

#endif /* STATIC_MODULE_H */