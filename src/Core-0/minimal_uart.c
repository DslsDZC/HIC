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
    return hal_inb((u16)(base + offset));
}

static inline void uart_write(phys_addr_t base, u16 offset, u8 value)
{
    hal_outb((u16)(base + offset), value);
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
    hal_outb((u16)(config->base_addr + UART_IER), 0x00);

    /* 启用DLAB，设置波特率 */
    hal_outb((u16)(config->base_addr + UART_LCR), UART_LCR_DLAB);
    hal_outb((u16)(config->base_addr + UART_DLL), (u8)(divisor & 0xFF));
    hal_outb((u16)(config->base_addr + UART_DLM), (u8)((divisor >> 8) & 0xFF));

    /* 设置线路控制（8N1或其他配置） */
    hal_outb((u16)(config->base_addr + UART_LCR), lcr);

    /* 禁用FIFO */
    hal_outb((u16)(config->base_addr + UART_FCR), 0x00);

    /* 禁用RTS和DTR */
    hal_outb((u16)(config->base_addr + UART_MCR), 0x00);

    /* 保存配置 */
    memcopy(&g_uart_config, config, sizeof(uart_config_t));
}

/* 使用配置初始化UART */
void minimal_uart_init_with_config(const uart_config_t *config)
{
    /* 调试输出：进入 minimal_uart_init_with_config */
    hal_outb(0x3F8, 'I');

    minimal_uart_configure(config);

    /* 保存配置 */
    memcopy(&g_uart_config, config, sizeof(uart_config_t));

    /* 调试输出：minimal_uart_init_with_config 完成 */
    hal_outb(0x3F8, 'J');
}

/* 使用默认配置初始化UART */
void minimal_uart_init(void)
{
    /* 静态标志位，防止重复初始化 */
    static bool initialized = false;

    /* 如果已经初始化过，直接返回 */
    if (initialized) {
        return;
    }

    /* 第一条指令：直接输出 'B' */
    __asm__ volatile("outb %%al, %%dx" : : "a"('B'), "d"(0x3F8));

    /* 计算波特率除数：115200 = 1843200 / (16 * 1) */
    const u16 divisor = 1;  /* 115200波特率 */

    /* 使用内联汇编直接输出 '1' */
    __asm__ volatile("outb %%al, %%dx" : : "a"('1'), "d"(0x3F8));

    /* 禁用中断 */
    __asm__ volatile("outb %%al, %%dx" : : "a"(0x00), "d"(0x3F8 + UART_IER));

    /* 使用内联汇编直接输出 '2' */
    __asm__ volatile("outb %%al, %%dx" : : "a"('2'), "d"(0x3F8));

    /* 启用DLAB，设置波特率 */
    __asm__ volatile("outb %%al, %%dx" : : "a"(UART_LCR_DLAB), "d"(0x3F8 + UART_LCR));
    __asm__ volatile("outb %%al, %%dx" : : "a"(divisor & 0xFF), "d"(0x3F8 + UART_DLL));
    __asm__ volatile("outb %%al, %%dx" : : "a"((divisor >> 8) & 0xFF), "d"(0x3F8 + UART_DLM));

    /* 8N1配置 - 这也会清除DLAB */
    __asm__ volatile("outb %%al, %%dx" : : "a"(0x03), "d"(0x3F8 + UART_LCR));

    /* 使用内联汇编直接输出 '3' */
    __asm__ volatile("outb %%al, %%dx" : : "a"('3'), "d"(0x3F8));

    /* 禁用FIFO */
    __asm__ volatile("outb %%al, %%dx" : : "a"(0x00), "d"(0x3F8 + UART_FCR));

    /* 使用内联汇编直接输出 '4' */
    __asm__ volatile("outb %%al, %%dx" : : "a"('4'), "d"(0x3F8));

    /* 使用内联汇编直接输出 '5' */
    __asm__ volatile("outb %%al, %%dx" : : "a"('5'), "d"(0x3F8));

    /* 禁用RTS/DTR */
    __asm__ volatile("outb %%al, %%dx" : : "a"(0x00), "d"(0x3F8 + UART_MCR));

    /* 使用内联汇编直接输出 '6' */
    __asm__ volatile("outb %%al, %%dx" : : "a"('6'), "d"(0x3F8));

    /* 标记为已初始化 */
    initialized = true;
}

/* 自动分配模式：引导层自动分配，写入文件，再从文件读取 */
void minimal_uart_init_apm_auto(const char *config_file_path)
{
    /* 从引导信息或平台配置读取串口配置 */
    extern hic_boot_info_t* g_boot_info;
    
    /* 优先从引导信息获取配置 */
    if (g_boot_info) {
        /* 检查是否有平台配置数据 */
        if (g_boot_info->platform.platform_data && 
            g_boot_info->platform.platform_size > 0) {
            /* 从平台YAML配置解析串口设置 */
            minimal_uart_init_from_yaml(
                (const char*)g_boot_info->platform.platform_data,
                g_boot_info->platform.platform_size
            );
            return;
        }
        
        /* 使用引导信息中的调试串口设置 */
        if (g_boot_info->debug.serial_port != 0) {
            uart_config_t config = {
                .base_addr = g_boot_info->debug.serial_port,
                .baud_rate = g_boot_info->debug.debug_flags ? 115200 : 9600,
                .data_bits = 8,
                .parity = 0,
                .stop_bits = 1
            };
            minimal_uart_init_with_config(&config);
            return;
        }
    }
    
    /* 如果指定了配置文件路径，尝试从文件读取 */
    if (config_file_path && config_file_path[0] != '\0') {
        /* 文件系统访问需要Privileged-1服务支持 */
        /* 这里由引导层负责将配置嵌入到内核映像中 */
        console_puts("[UART] Config file path specified: ");
        console_puts(config_file_path);
        console_puts("\n");
    }
    
    /* 默认初始化 */
    minimal_uart_init();
}

/* 配置文件模式：直接在配置文件中写入，然后读取 */
void minimal_uart_init_apm_config(const char *yaml_data, size_t yaml_size)
{
    minimal_uart_init_from_yaml(yaml_data, yaml_size);
}

/* 从引导信息获取YAML数据并初始化（配置文件模式） */
void minimal_uart_init_from_bootinfo(void)
{
    /* 注意：串口已在 bootloader 中初始化，这里不重新初始化
       避免重复初始化导致输出混乱 */

    /* 如果需要从 YAML 重新配置串口（可选） */
    /*
    if (g_boot_info && g_boot_info->platform.platform_data && g_boot_info->platform.platform_size > 0) {
        minimal_uart_init_from_yaml(
            (const char*)g_boot_info->platform.platform_data,
            g_boot_info->platform.platform_size
        );
    }
    */
}

/* 从YAML配置初始化UART */
void minimal_uart_init_from_yaml(const char *yaml_data, size_t yaml_size)
{
    yaml_parser_t *parser = yaml_parser_create(yaml_data, yaml_size);
    if (!parser) {
        minimal_uart_init();
        return;
    }

    if (yaml_parse(parser) != 0) {
        minimal_uart_init();
        yaml_parser_destroy(parser);
        return;
    }

    yaml_node_t *root = yaml_get_root(parser);
    if (!root) {
        minimal_uart_init();
        yaml_parser_destroy(parser);
        return;
    }

    /* 查找串口配置节点 - 支持两种路径：debug.serial 和 uart */
    yaml_node_t *uart_node = yaml_find_node(root, "uart");
    
    /* 如果找不到uart节点，尝试从debug.serial获取 */
    if (!uart_node) {
        yaml_node_t *debug_node = yaml_find_node(root, "debug");
        if (debug_node && debug_node->type == YAML_TYPE_MAPPING) {
            uart_node = yaml_find_node(debug_node, "serial");
        }
    }
    
    /* 如果还是找不到，使用默认配置 */
    if (!uart_node) {
        minimal_uart_init();
        yaml_parser_destroy(parser);
        return;
    }

    uart_config_t config;
    memcopy(&config, &g_uart_config, sizeof(uart_config_t));

    /* 读取端口地址 */
    yaml_node_t *node = yaml_find_node(uart_node, "port");
    if (node && node->value) {
        config.base_addr = (phys_addr_t)yaml_get_u64(node, UART_DEFAULT_BASE);
    }

    /* 读取波特率 */
    node = yaml_find_node(uart_node, "baud_rate");
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
    /* 直接输出字符，不做任何转换 */
    __asm__ volatile("outb %%al, %%dx" : : "a"((u8)c), "d"(0x3F8 + UART_THR));
}

/* 发送字符串 */
void minimal_uart_puts(const char *str)
{
    while (*str) {
        minimal_uart_putc(*str);
        str++;
    }
}

/* 串口打印（兼容别名） */
void serial_print(const char *msg)
{
    minimal_uart_puts(msg);
}

/* 检查是否有数据可读 */
bool minimal_uart_readable(void)
{
    u8 lsr = hal_inb((u16)(g_uart_config.base_addr + UART_LSR));
    return (lsr & UART_LSR_DR) != 0;
}

/* 接收单个字符（阻塞） */
char minimal_uart_getc(void)
{
    /* 等待数据就绪 */
    while (!minimal_uart_readable()) {
        hal_halt();
    }
    return (char)hal_inb((u16)(g_uart_config.base_addr + UART_RBR));
}

/* 接收单个字符（非阻塞），返回是否成功 */
bool minimal_uart_try_getc(char *c)
{
    if (minimal_uart_readable()) {
        *c = (char)hal_inb((u16)(g_uart_config.base_addr + UART_RBR));
        return true;
    }
    return false;
}

/* 接收一行（带回显） */
int minimal_uart_getline(char *buf, int max_len)
{
    int i = 0;
    char c;
    
    while (i < max_len - 1) {
        c = minimal_uart_getc();
        
        if (c == '\r' || c == '\n') {
            /* 回车或换行：结束输入 */
            minimal_uart_putc('\r');
            minimal_uart_putc('\n');
            break;
        } else if (c == 0x7F || c == '\b') {
            /* 退格或删除：删除最后一个字符 */
            if (i > 0) {
                i--;
                minimal_uart_putc('\b');
                minimal_uart_putc(' ');
                minimal_uart_putc('\b');
            }
        } else if (c >= ' ' && c <= '~') {
            /* 可打印字符 */
            buf[i++] = c;
            minimal_uart_putc(c);
        }
    }
    
    buf[i] = '\0';
    return i;
}

/* 从APM配置初始化串口 */
struct uart_config_for_minimal {
    phys_addr_t base_addr;
    u32 baud_rate;
    u32 data_bits;
    u32 parity;
    u32 stop_bits;
};

void minimal_uart_init_from_apm(struct uart_config_for_minimal *cfg)
{
    if (!cfg) {
        return;
    }
    
    uart_config_t config;
    config.base_addr = cfg->base_addr;
    config.baud_rate = cfg->baud_rate;
    config.data_bits = (uart_data_bits_t)cfg->data_bits;
    config.parity = (uart_parity_t)cfg->parity;
    config.stop_bits = (uart_stop_bits_t)cfg->stop_bits;
    
    minimal_uart_init_with_config(&config);
}