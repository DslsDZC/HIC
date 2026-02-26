/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "service.h"
#include <string.h>

/* 最大命令数量 */
#define MAX_COMMANDS 32

/* 命令表 */
static command_t g_command_table[MAX_COMMANDS];
static int g_command_count = 0;

/* 版本信息 */
#define HIC_VERSION "0.1.0"
#define HIC_BUILD_DATE __DATE__

/* 注册命令到命令表 */
void cli_service_register_command(const char *name, const char *desc, cmd_handler_t handler) {
    if (g_command_count >= MAX_COMMANDS) {
        return;  /* 命令表已满 */
    }
    
    g_command_table[g_command_count].name = name;
    g_command_table[g_command_count].description = desc;
    g_command_table[g_command_count].handler = handler;
    g_command_count++;
}

/* 初始化 CLI 服务 */
void cli_service_init(void) {
    g_command_count = 0;
    
    /* 注册内置命令 */
    cli_service_register_command("help", "显示帮助信息", cmd_help);
    cli_service_register_command("echo", "回显文本", cmd_echo);
    cli_service_register_command("version", "显示版本信息", cmd_version);
    cli_service_register_command("mem", "显示内存信息", cmd_mem);
    cli_service_register_command("clear", "清空屏幕", cmd_clear);
}

/* 处理输入命令 */
void cli_service_process(const char *input) {
    char cmd_name[CMD_BUFFER_SIZE];
    const char *args = NULL;
    int i;
    
    if (!input || input[0] == '\0') {
        return;
    }
    
    /* 跳过前导空格 */
    while (*input == ' ' || *input == '\t') {
        input++;
    }
    
    /* 解析命令名称 */
    i = 0;
    while (*input != '\0' && *input != ' ' && *input != '\t' && *input != '\n' && i < CMD_BUFFER_SIZE - 1) {
        cmd_name[i++] = *input++;
    }
    cmd_name[i] = '\0';
    
    /* 跳过空格，获取参数 */
    while (*input == ' ' || *input == '\t') {
        input++;
    }
    args = (*input != '\0') ? input : "";
    
    /* 查找并执行命令 */
    for (i = 0; i < g_command_count; i++) {
        if (strcmp(cmd_name, g_command_table[i].name) == 0) {
            g_command_table[i].handler(args);
            return;
        }
    }
    
    /* 未找到命令 */
    /* TODO: 输出错误信息到某个地方 */
}

/* 显示帮助信息 */
void cmd_help(const char *args) {
    int i;
    (void)args;  /* 未使用 */
    
    /* TODO: 输出帮助信息 */
    for (i = 0; i < g_command_count; i++) {
        /* TODO: 输出命令名称和描述 */
    }
}

/* 回显文本 */
void cmd_echo(const char *args) {
    if (args && args[0] != '\0') {
        /* TODO: 输出 args */
    }
}

/* 显示版本信息 */
void cmd_version(const char *args) {
    (void)args;  /* 未使用 */
    
    /* TODO: 输出版本信息 */
    /*
    printf("HIC Kernel Version: %s\n", HIC_VERSION);
    printf("Build Date: %s\n", HIC_BUILD_DATE);
    */
}

/* 显示内存信息 */
void cmd_mem(const char *args) {
    (void)args;  /* 未使用 */
    
    /* TODO: 获取并显示内存信息 */
    /* 需要通过系统调用从内核获取内存状态 */
}

/* 清空屏幕 */
void cmd_clear(const char *args) {
    (void)args;  /* 未使用 */
    
    /* TODO: 清空屏幕 */
    /* 可能需要与 GUI 交互 */
}