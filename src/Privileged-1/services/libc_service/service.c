/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * @file service.c
 * @brief libc 服务实现
 * 
 * libc服务为所有其他Privileged-1服务提供标准C库函数
 * 这是一个基础服务，需要在其他服务之前加载
 */

#include "service.h"
#include "../include/string.h"
#include "../include/stdio.h"
#include "../include/stdlib.h"
#include "../include/ctype.h"

/* 外部函数声明 - 需要内核提供的服务 */
extern void service_register(const char *name, const service_api_t *api);

/* 服务信息 */
#define LIBC_VERSION "1.0.0"
#define LIBC_BUILD_DATE __DATE__

/* libc API 实现 */
struct libc_api libc_api = {
    /* 字符串函数 */
    .strlen = strlen,
    .strcmp = strcmp,
    .strncmp = strncmp,
    .strcpy = strcpy,
    .strncpy = strncpy,
    .strcat = strcat,
    .strncat = strncat,
    .strchr = strchr,
    .strrchr = strrchr,
    .strstr = strstr,
    .strtok = strtok,
    
    /* 内存函数 */
    .memset = memset,
    .memcpy = memcpy,
    .memmove = memmove,
    .memcmp = memcmp,
    .memchr = memchr,
    
    /* 字符函数 */
    .isdigit = isdigit,
    .isalpha = isalpha,
    .isalnum = isalnum,
    .islower = islower,
    .isupper = isupper,
    .isspace = isspace,
    .tolower = tolower,
    .toupper = toupper,
    
    /* 转换函数 */
    .atoi = atoi,
    .atol = atol,
    .atoll = atoll,
    .strtol = strtol,
    .strtoul = strtoul,
    .strtoll = strtoll,
    .strtoull = strtoull,
    .atof = atof,
    .strtod = strtod,
    .strtof = strtof,
    
    /* 标准IO函数 */
    .printf = printf,
    .sprintf = sprintf,
    .snprintf = snprintf,
    .puts = puts,
    .putchar = putchar,
    
    /* 分配函数 */
    .malloc = malloc,
    .free = free,
    .calloc = calloc,
    .realloc = realloc,
};

/* 静态服务API */
static const service_api_t g_libc_service_api = {
    .init = libc_service_init,
    .start = libc_service_start,
    .stop = libc_service_stop,
    .cleanup = libc_service_cleanup,
    .get_info = NULL,
};

/**
 * @brief 初始化libc服务
 */
hic_status_t libc_service_init(void)
{
    /* libc服务不需要特殊初始化 */
    return HIC_SUCCESS;
}

/**
 * @brief 启动libc服务
 */
hic_status_t libc_service_start(void)
{
    /* 注册libc服务到服务管理器 */
    service_register("libc", &g_libc_service_api);
    return HIC_SUCCESS;
}

/**
 * @brief 停止libc服务
 */
hic_status_t libc_service_stop(void)
{
    /* libc服务不能停止，因为其他服务依赖它 */
    return HIC_ERROR;
}

/**
 * @brief 清理libc服务
 */
hic_status_t libc_service_cleanup(void)
{
    /* libc服务不需要清理 */
    return HIC_SUCCESS;
}