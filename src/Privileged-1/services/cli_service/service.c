/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "service.h"
#include <string.h>

/* 最大命令数量 */
#define MAX_COMMANDS 64
#define MAX_MODULES 32

/* 命令表 */
static command_t g_command_table[MAX_COMMANDS];
static int g_command_count = 0;

/* 版本信息 */
#define HIC_VERSION "0.1.0"
#define HIC_BUILD_DATE __DATE__

/* 外部服务函数声明（需要链接其他服务） */
extern hic_status_t module_load(const char *path, int verify_signature);
extern hic_status_t module_unload(const char *name);
extern hic_status_t module_list(module_info_t *modules, int *count);
extern hic_status_t module_info(const char *name, module_info_t *info);
extern hic_status_t module_verify(const char *path);
extern hic_status_t module_restart(const char *name);
extern hic_status_t module_rolling_update(const char *name, const char *new_path, int verify);

/* VGA 服务函数声明 */
extern void vga_putchar(char c);
extern void vga_puts(const char *str);
extern void vga_clear(void);

/* 串口服务函数声明 */
extern void serial_putc(char c);
extern void serial_puts(const char *str);

/* 输出函数 */
static void console_putc(char c) {
    vga_putchar(c);
    serial_putc(c);
}

static void console_puts(const char *str) {
    while (*str) {
        console_putc(*str++);
    }
}

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
    
    /* 注册模块管理命令 */
    cli_service_register_command("modload", "加载模块: modload <path> [verify]", cmd_modload);
    cli_service_register_command("modunload", "卸载模块: modunload <name>", cmd_modunload);
    cli_service_register_command("modlist", "列出已加载的模块", cmd_modlist);
    cli_service_register_command("modinfo", "显示模块信息: modinfo <name>", cmd_modinfo);
    cli_service_register_command("modverify", "验证模块: modverify <path>", cmd_modverify);
    cli_service_register_command("modrestart", "重启模块: modrestart <name>", cmd_modrestart);
    cli_service_register_command("modupdate", "滚动更新模块: modupdate <name> <new_path>", cmd_modupdate);
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
    console_puts("Unknown command: ");
    console_puts(cmd_name);
    console_puts("\n");
    console_puts("Type 'help' for available commands.\n");
}

/* 显示帮助信息 */
void cmd_help(const char *args) {
    int i;
    (void)args;  /* 未使用 */
    
    console_puts("Available commands:\n");
    for (i = 0; i < g_command_count; i++) {
        console_puts("  ");
        console_puts(g_command_table[i].name);
        
        /* 对齐 */
        int len = strlen(g_command_table[i].name);
        while (len < 16) {
            console_putc(' ');
            len++;
        }
        
        console_puts(" - ");
        console_puts(g_command_table[i].description);
        console_puts("\n");
    }
}

/* 回显文本 */
void cmd_echo(const char *args) {
    if (args && args[0] != '\0') {
        console_puts(args);
        console_puts("\n");
    }
}

/* 显示版本信息 */
void cmd_version(const char *args) {
    (void)args;  /* 未使用 */
    
    console_puts("HIC OS Version: ");
    console_puts(HIC_VERSION);
    console_puts("\n");
    console_puts("Build Date: ");
    console_puts(HIC_BUILD_DATE);
    console_puts("\n");
}

/* 显示内存信息 */
void cmd_mem(const char *args) {
    (void)args;  /* 未使用 */
    
    /* TODO: 获取并显示内存信息 */
    /* 需要通过系统调用从内核获取内存状态 */
    console_puts("Memory Information:\n");
    console_puts("  Total: 2048 MB\n");
    console_puts("  Used:  512 MB\n");
    console_puts("  Free:  1536 MB\n");
}

/* 清空屏幕 */
void cmd_clear(const char *args) {
    (void)args;  /* 未使用 */
    
    vga_clear();
}

/* 加载模块 */
void cmd_modload(const char *args) {
    char path[CMD_BUFFER_SIZE];
    int verify = 0;
    const char *p;
    int i;
    
    if (!args || args[0] == '\0') {
        console_puts("Usage: modload <path> [verify]\n");
        return;
    }
    
    /* 解析路径 */
    i = 0;
    p = args;
    while (*p != '\0' && *p != ' ' && i < CMD_BUFFER_SIZE - 1) {
        path[i++] = *p++;
    }
    path[i] = '\0';
    
    /* 跳过空格，检查是否需要验证 */
    while (*p == ' ') {
        p++;
    }
    if (strcmp(p, "verify") == 0 || strcmp(p, "yes") == 0) {
        verify = 1;
    }
    
    /* 调用模块加载函数 */
    hic_status_t status = module_load(path, verify);
    
    switch (status) {
        case HIC_SUCCESS:
            console_puts("Module loaded successfully: ");
            console_puts(path);
            console_puts("\n");
            break;
        case HIC_INVALID_PARAM:
            console_puts("Error: Invalid parameter\n");
            break;
        case HIC_NOT_FOUND:
            console_puts("Error: Module file not found: ");
            console_puts(path);
            console_puts("\n");
            break;
        case HIC_BUSY:
            console_puts("Error: Module already loaded\n");
            break;
        case HIC_OUT_OF_MEMORY:
            console_puts("Error: Out of memory\n");
            break;
        case HIC_PARSE_FAILED:
            console_puts("Error: Invalid module format\n");
            break;
        default:
            console_puts("Error: Failed to load module (status: ");
            /* TODO: 显示状态码 */
            console_puts(")\n");
            break;
    }
}

/* 卸载模块 */
void cmd_modunload(const char *args) {
    char name[CMD_BUFFER_SIZE];
    const char *p;
    int i;
    
    if (!args || args[0] == '\0') {
        console_puts("Usage: modunload <name>\n");
        return;
    }
    
    /* 解析模块名称 */
    i = 0;
    p = args;
    while (*p != '\0' && *p != ' ' && i < CMD_BUFFER_SIZE - 1) {
        name[i++] = *p++;
    }
    name[i] = '\0';
    
    /* 调用模块卸载函数 */
    hic_status_t status = module_unload(name);
    
    switch (status) {
        case HIC_SUCCESS:
            console_puts("Module unloaded successfully: ");
            console_puts(name);
            console_puts("\n");
            break;
        case HIC_INVALID_PARAM:
            console_puts("Error: Invalid parameter\n");
            break;
        case HIC_NOT_FOUND:
            console_puts("Error: Module not found: ");
            console_puts(name);
            console_puts("\n");
            break;
        default:
            console_puts("Error: Failed to unload module (status: ");
            /* TODO: 显示状态码 */
            console_puts(")\n");
            break;
    }
}

/* 列出模块 */
void cmd_modlist(const char *args) {
    module_info_t modules[MAX_MODULES];
    int count;
    int i;
    (void)args;  /* 未使用 */
    
    count = MAX_MODULES;
    hic_status_t status = module_list(modules, &count);
    
    if (status != HIC_SUCCESS) {
        console_puts("Error: Failed to list modules\n");
        return;
    }
    
    if (count == 0) {
        console_puts("No modules loaded.\n");
        return;
    }
    
    console_puts("Loaded modules (");
    /* TODO: 显示数量 */
    console_puts("):\n");
    console_puts("  Name               Version  State\n");
    console_puts("  -----------------------------------\n");
    
    for (i = 0; i < count; i++) {
        console_puts("  ");
        console_puts(modules[i].name);
        
        /* 对齐 */
        int len = strlen(modules[i].name);
        while (len < 18) {
            console_putc(' ');
            len++;
        }
        
        /* 显示版本 */
        u32 version = modules[i].version;
        console_putc('0' + ((version >> 16) & 0xFF));
        console_putc('.');
        console_putc('0' + ((version >> 8) & 0xFF));
        console_putc('.');
        console_putc('0' + (version & 0xFF));
        
        console_putc(' ');
        console_putc(' ');
        
        /* 显示状态 */
        switch (modules[i].state) {
            case MODULE_STATE_unloaded:
                console_puts("unloaded");
                break;
            case MODULE_STATE_loading:
                console_puts("loading");
                break;
            case MODULE_STATE_loaded:
                console_puts("loaded");
                break;
            case MODULE_STATE_running:
                console_puts("running");
                break;
            case MODULE_STATE_error:
                console_puts("error");
                break;
            case MODULE_STATE_unloading:
                console_puts("unloading");
                break;
            case MODULE_STATE_suspended:
                console_puts("suspended");
                break;
        }
        
        console_puts("\n");
    }
}

/* 显示模块信息 */
void cmd_modinfo(const char *args) {
    char name[CMD_BUFFER_SIZE];
    const char *p;
    int i;
    module_info_t info;
    
    if (!args || args[0] == '\0') {
        console_puts("Usage: modinfo <name>\n");
        return;
    }
    
    /* 解析模块名称 */
    i = 0;
    p = args;
    while (*p != '\0' && *p != ' ' && i < CMD_BUFFER_SIZE - 1) {
        name[i++] = *p++;
    }
    name[i] = '\0';
    
    /* 获取模块信息 */
    hic_status_t status = module_info(name, &info);
    
    if (status != HIC_SUCCESS) {
        console_puts("Error: Module not found: ");
        console_puts(name);
        console_puts("\n");
        return;
    }
    
    /* 显示模块信息 */
    console_puts("Module Information:\n");
    console_puts("  Name:    ");
    console_puts(info.name);
    console_puts("\n");
    console_puts("  Version: ");
    u32 version = info.version;
    console_putc('0' + ((version >> 16) & 0xFF));
    console_putc('.');
    console_putc('0' + ((version >> 8) & 0xFF));
    console_putc('.');
    console_putc('0' + (version & 0xFF));
    console_puts("\n");
    console_puts("  State:   ");
    
    switch (info.state) {
        case MODULE_STATE_unloaded:
            console_puts("unloaded");
            break;
        case MODULE_STATE_loading:
            console_puts("loading");
            break;
        case MODULE_STATE_loaded:
            console_puts("loaded");
            break;
        case MODULE_STATE_running:
            console_puts("running");
            break;
        case MODULE_STATE_error:
            console_puts("error");
            break;
        case MODULE_STATE_unloading:
            console_puts("unloading");
            break;
        case MODULE_STATE_suspended:
            console_puts("suspended");
            break;
    }
    
    console_puts("\n");
    console_puts("  Size:    ");
    /* TODO: 显示大小 */
    console_puts(" bytes\n");
    console_puts("  Flags:   ");
    if (info.flags & 0x01) {
        console_puts("signed ");
    }
    console_puts("\n");
}

/* 验证模块 */
void cmd_modverify(const char *args) {
    char path[CMD_BUFFER_SIZE];
    const char *p;
    int i;
    
    if (!args || args[0] == '\0') {
        console_puts("Usage: modverify <path>\n");
        return;
    }
    
    /* 解析路径 */
    i = 0;
    p = args;
    while (*p != '\0' && *p != ' ' && i < CMD_BUFFER_SIZE - 1) {
        path[i++] = *p++;
    }
    path[i] = '\0';
    
    /* 调用模块验证函数 */
    hic_status_t status = module_verify(path);
    
    switch (status) {
        case HIC_SUCCESS:
            console_puts("Module verified successfully: ");
            console_puts(path);
            console_puts("\n");
            break;
        case HIC_INVALID_PARAM:
            console_puts("Error: Invalid parameter\n");
            break;
        case HIC_NOT_FOUND:
            console_puts("Error: Module file not found: ");
            console_puts(path);
            console_puts("\n");
            break;
        case HIC_PARSE_FAILED:
            console_puts("Error: Invalid module format or corrupted\n");
            break;
        default:
            console_puts("Error: Failed to verify module (status: ");
            /* TODO: 显示状态码 */
            console_puts(")\n");
            break;
    }
}

/* 重启模块 */
void cmd_modrestart(const char *args) {
    char name[CMD_BUFFER_SIZE];
    const char *p;
    int i;
    
    if (!args || args[0] == '\0') {
        console_puts("Usage: modrestart <name>\n");
        return;
    }
    
    /* 解析模块名称 */
    i = 0;
    p = args;
    while (*p != '\0' && *p != ' ' && i < CMD_BUFFER_SIZE - 1) {
        name[i++] = *p++;
    }
    name[i] = '\0';
    
    /* 调用模块重启函数 */
    hic_status_t status = module_restart(name);
    
    switch (status) {
        case HIC_SUCCESS:
            console_puts("Module restarted successfully: ");
            console_puts(name);
            console_puts("\n");
            break;
        case HIC_INVALID_PARAM:
            console_puts("Error: Invalid parameter\n");
            break;
        case HIC_NOT_FOUND:
            console_puts("Error: Module not found: ");
            console_puts(name);
            console_puts("\n");
            break;
        default:
            console_puts("Error: Failed to restart module (status: ");
            /* TODO: 显示状态码 */
            console_puts(")\n");
            break;
    }
}

/* 滚动更新模块 */
void cmd_modupdate(const char *args) {
    char name[CMD_BUFFER_SIZE];
    char new_path[CMD_BUFFER_SIZE];
    const char *p;
    int i;
    int verify = 0;
    
    if (!args || args[0] == '\0') {
        console_puts("Usage: modupdate <name> <new_path> [verify]\n");
        return;
    }
    
    /* 解析模块名称 */
    i = 0;
    p = args;
    while (*p != '\0' && *p != ' ' && i < CMD_BUFFER_SIZE - 1) {
        name[i++] = *p++;
    }
    name[i] = '\0';
    
    /* 跳过空格 */
    while (*p == ' ') {
        p++;
    }
    
    /* 解析新路径 */
    i = 0;
    while (*p != '\0' && *p != ' ' && i < CMD_BUFFER_SIZE - 1) {
        new_path[i++] = *p++;
    }
    new_path[i] = '\0';
    
    /* 跳过空格，检查是否需要验证 */
    while (*p == ' ') {
        p++;
    }
    if (strcmp(p, "verify") == 0 || strcmp(p, "yes") == 0) {
        verify = 1;
    }
    
    /* 调用滚动更新函数 */
    hic_status_t status = module_rolling_update(name, new_path, verify);
    
    switch (status) {
        case HIC_SUCCESS:
            console_puts("Module rolling update completed: ");
            console_puts(name);
            console_puts("\n");
            break;
        case HIC_INVALID_PARAM:
            console_puts("Error: Invalid parameter\n");
            break;
        case HIC_NOT_FOUND:
            console_puts("Error: Module not found: ");
            console_puts(name);
            console_puts("\n");
            break;
        case HIC_NOT_IMPLEMENTED:
            console_puts("Error: Rolling update not implemented yet\n");
            break;
        default:
            console_puts("Error: Failed to update module (status: ");
            /* TODO: 显示状态码 */
            console_puts(")\n");
            break;
    }
}