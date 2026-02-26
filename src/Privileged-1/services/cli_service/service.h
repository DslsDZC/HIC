/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef CLI_SERVICE_H
#define CLI_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/* 命令缓冲区大小 */
#define CMD_BUFFER_SIZE 256

/* 命令处理函数类型 */
typedef void (*cmd_handler_t)(const char *args);

/* 命令结构 */
typedef struct {
    const char *name;        /* 命令名称 */
    const char *description; /* 命令描述 */
    cmd_handler_t handler;   /* 处理函数 */
} command_t;

/* 服务接口 */
void cli_service_init(void);
void cli_service_process(const char *input);
void cli_service_register_command(const char *name, const char *desc, cmd_handler_t handler);
void cli_service_print_help(void);

/* 内置命令处理函数 */
void cmd_help(const char *args);
void cmd_echo(const char *args);
void cmd_version(const char *args);
void cmd_mem(const char *args);
void cmd_clear(const char *args);

#endif /* CLI_SERVICE_H */