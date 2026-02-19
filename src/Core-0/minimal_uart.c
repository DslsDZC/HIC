/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC极简串口驱动实现
 * 极简设计：无FIFO、无中断、轮询方式
 * 支持两种 APM 模式：
 * 1. 自动分配模式：引导层自动分配，写入文件，再从文件读取
 * 2. 配置文件模式：直接在配置文件中写入，然后读取
 */

#include "minimal_uart.h"
#include "yaml.h"
#include "boot_info.h"
#include "hal.h"
#include "lib/console.h"
#include "lib/mem.h"
#include "lib/string.h"

/* 引用外部引导信息 */
extern hic_boot_info_t *g_boot_info;

/* 全局串口配置 */
uart_config_t g_uart_config = {
    .base_addr = UART_DEFAULT_BASE,
    .baud_rate = UART_DEFAULT_BAUD,
    .data_bits = UART_DEFAULT_DATA_BITS,
    .parity = UART_DEFAULT_PARITY,
    .stop_bits = UART_DEFAULT_STOP_BITS,
};

/* 全局APM模式 */
apm_mode_t g_apm_mode = APM_MODE_AUTO;

/* 设置APM模式 */
void minimal_uart_set_apm_mode(apm_mode_t mode)
{
    g_apm_mode = mode;
}

/* 获取APM模式 */
apm_mode_t minimal_uart_get_apm_mode(void)
{
    return g_apm_mode;
}

/* UART寄存器读写 */
static inline u8 uart_read(phys_addr_t base, u16 offset)
{
    return *(volatile u8 *)(base + offset);
}

static inline void uart_write(phys_addr_t base, u16 offset, u8 value)
{
    *(volatile u8 *)(base + offset) = value;
}

/* 计算波特率除数 */
static u16 calculate_baud_divisor(u32 baud_rate)
{
    /* UART时钟频率为1.8432MHz */
    const u32 uart_clock = 1843200;
    return (u16)(uart_clock / (16 * baud_rate));
}

/* 配置UART */
void minimal_uart_configure(const uart_config_t *config)
{
    u16 divisor = calculate_baud_divisor(config->baud_rate);
    u8 lcr = 0;

    /* 设置数据位 */
    switch (config->data_bits) {
        case UART_DATA_BITS_5:
            lcr |= UART_LCR_WLEN5;
            break;
        case UART_DATA_BITS_6:
            lcr |= UART_LCR_WLEN6;
            break;
        case UART_DATA_BITS_7:
            lcr |= UART_LCR_WLEN7;
            break;
        case UART_DATA_BITS_8:
        default:
            lcr |= UART_LCR_WLEN8;
            break;
    }

    /* 设置校验位 */
    switch (config->parity) {
        case UART_PARITY_ODD:
            lcr |= UART_LCR_PARITY;
            break;
        case UART_PARITY_EVEN:
            lcr |= UART_LCR_PARITY | UART_LCR_EPAR;
            break;
        case UART_PARITY_MARK:
            lcr |= UART_LCR_PARITY | UART_LCR_SPAR;
            break;
        case UART_PARITY_SPACE:
            lcr |= UART_LCR_PARITY | UART_LCR_SPAR | UART_LCR_EPAR;
            break;
        case UART_PARITY_NONE:
        default:
            /* 无校验，不设置相关位 */
            break;
    }

    /* 设置停止位 */
    if (config->stop_bits == UART_STOP_BITS_2) {
        lcr |= UART_LCR_STOP;
    }

    /* 禁用中断 */
    uart_write(config->base_addr, UART_IER, 0x00);

    /* 启用DLAB，设置波特率 */
    uart_write(config->base_addr, UART_LCR, UART_LCR_DLAB);
    uart_write(config->base_addr, UART_DLL, (u8)(divisor & 0xFF));
    uart_write(config->base_addr, UART_DLM, (u8)((divisor >> 8) & 0xFF));

    /* 设置线路控制（8N1或其他配置） */
    uart_write(config->base_addr, UART_LCR, lcr);

    /* 禁用FIFO */
    uart_write(config->base_addr, UART_FCR, 0x00);

    /* 启用RTS和DTR */
    uart_write(config->base_addr, UART_MCR, 0x03);

    /* 保存配置 */
    memcopy(&g_uart_config, config, sizeof(uart_config_t));
}

/* 使用配置初始化UART */
void minimal_uart_init_with_config(const uart_config_t *config)
{
    minimal_uart_configure(config);

    console_puts("[UART] Initialized (");
    console_putu32(config->baud_rate);
    console_puts(" ");
    console_putu32(config->data_bits);
    console_puts("N");
    console_putu32(config->stop_bits);
    console_puts(")\n");
}

/* 使用默认配置初始化UART */
void minimal_uart_init(void)
{
    minimal_uart_init_with_config(&g_uart_config);
}

/* 自动分配模式：引导层自动分配，写入文件，再从文件读取 */
void minimal_uart_init_apm_auto(const char *config_file_path)
{
    console_puts("[UART] APM Auto Mode: Loading config from file\n");
    console_puts("[UART] Config file: ");
    console_puts(config_file_path);
    console_puts("\n");

    /* TODO: 从配置文件读取串口配置 */
    /* 这里需要实现文件读取功能 */
    /* 由于内核早期阶段可能没有文件系统，这部分由引导层处理 */
    /* 引导层自动分配串口配置，写入文件，内核启动时读取 */

    console_puts("[UART] Using default config (auto allocation mode)\n");
    minimal_uart_init();
}

/* 配置文件模式：直接在配置文件中写入，然后读取 */
void minimal_uart_init_apm_config(const char *yaml_data, size_t yaml_size)
{
    console_puts("[UART] APM Config Mode: Loading config from YAML\n");
    minimal_uart_init_from_yaml(yaml_data, yaml_size);
}

/* 从引导信息获取YAML数据并初始化（配置文件模式） */
void minimal_uart_init_from_bootinfo(void)
{
    console_puts("[UART] Initializing from boot info\n");

    if (!g_boot_info) {
        console_puts("[UART] No boot info, using default config\n");
        minimal_uart_init();
        return;
    }

    const char *yaml_data = (const char *)g_boot_info->config_data;
    size_t yaml_size = g_boot_info->config_size;

    if (!yaml_data || yaml_size == 0) {
        console_puts("[UART] No YAML data in boot info, using default config\n");
        minimal_uart_init();
        return;
    }

    console_puts("[UART] YAML data size: ");
    console_putu64(yaml_size);
    console_puts(" bytes\n");

    minimal_uart_init_from_yaml(yaml_data, yaml_size);
}

/* 从YAML配置初始化UART */
void minimal_uart_init_from_yaml(const char *yaml_data, size_t yaml_size)
{
    yaml_parser_t *parser = yaml_parser_create(yaml_data, yaml_size);
    if (!parser) {
        console_puts("[UART] Failed to create YAML parser, using default config\n");
        minimal_uart_init();
        return;
    }

    if (yaml_parse(parser) != 0) {
        console_puts("[UART] Failed to parse YAML, using default config\n");
        minimal_uart_init();
        yaml_parser_destroy(parser);
        return;
    }

    yaml_node_t *root = yaml_get_root(parser);
    if (!root) {
        console_puts("[UART] No YAML root, using default config\n");
        minimal_uart_init();
        yaml_parser_destroy(parser);
        return;
    }

    /* 查找串口配置节点 */
    yaml_node_t *uart_node = yaml_find_node(root, "uart");
    if (!uart_node) {
        console_puts("[UART] No UART config in YAML, using default config\n");
        minimal_uart_init();
        yaml_parser_destroy(parser);
        return;
    }

    uart_config_t config;
    memcopy(&config, &g_uart_config, sizeof(uart_config_t));

    /* 读取基地址 */
    yaml_node_t *node = yaml_find_node(uart_node, "base");
    if (node && node->value) {
        config.base_addr = (phys_addr_t)yaml_get_u64(node, UART_DEFAULT_BASE);
    }

    /* 读取波特率 */
    node = yaml_find_node(uart_node, "baud");
    if (node && node->value) {
        config.baud_rate = (u32)yaml_get_u64(node, UART_DEFAULT_BAUD);
    }

    /* 读取数据位 */
    node = yaml_find_node(uart_node, "data_bits");
    if (node && node->value) {
        u32 data_bits = (u32)yaml_get_u64(node, 8);
        if (data_bits >= 5 && data_bits <= 8) {
            config.data_bits = (uart_data_bits_t)data_bits;
        }
    }

    /* 读取校验位 */
    node = yaml_find_node(uart_node, "parity");
    if (node && node->value) {
        const char *parity_str = node->value;
        if (strcmp(parity_str, "none") == 0) {
            config.parity = UART_PARITY_NONE;
        } else if (strcmp(parity_str, "odd") == 0) {
            config.parity = UART_PARITY_ODD;
        } else if (strcmp(parity_str, "even") == 0) {
            config.parity = UART_PARITY_EVEN;
        } else if (strcmp(parity_str, "mark") == 0) {
            config.parity = UART_PARITY_MARK;
        } else if (strcmp(parity_str, "space") == 0) {
            config.parity = UART_PARITY_SPACE;
        }
    }

    /* 读取停止位 */
    node = yaml_find_node(uart_node, "stop_bits");
    if (node && node->value) {
        u32 stop_bits = (u32)yaml_get_u64(node, 1);
        if (stop_bits >= 1 && stop_bits <= 2) {
            config.stop_bits = (uart_stop_bits_t)stop_bits;
        }
    }

    /* 应用配置 */
    minimal_uart_init_with_config(&config);

    yaml_parser_destroy(parser);
}

/* 发送单个字符 */
void minimal_uart_putc(char c)
{
    /* 等待发送保持寄存器就绪 */
    while (!(uart_read(g_uart_config.base_addr, UART_LSR) & UART_LSR_THRE)) {
        /* 等待 */
    }

    /* 发送字符 */
    uart_write(g_uart_config.base_addr, UART_THR, (u8)c);
}

/* 发送字符串 */
void minimal_uart_puts(const char *str)
{
    while (*str) {
        minimal_uart_putc(*str);
        str++;
    }
}