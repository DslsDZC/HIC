/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC极简串口驱动
 * 极简设计：无FIFO、无中断、轮询方式
 * 配置从 YAML 文件读取（通过引导层传递）
 */

#ifndef HIC_KERNEL_MINIMAL_UART_H
#define HIC_KERNEL_MINIMAL_UART_H

#include "types.h"

/* UART寄存器偏移 */
#define UART_RBR  0x00   /* 接收缓冲寄存器 */
#define UART_THR  0x00   /* 发送保持寄存器 */
#define UART_IER  0x01   /* 中断使能寄存器 */
#define UART_IIR  0x02   /* 中断标识寄存器 */
#define UART_FCR  0x02   /* FIFO控制寄存器 */
#define UART_LCR  0x03   /* 线路控制寄存器 */
#define UART_MCR  0x04   /* 调制解调控制寄存器 */
#define UART_LSR  0x05   /* 线路状态寄存器 */
#define UART_MSR  0x06   /* 调制解调状态寄存器 */
#define UART_SCR  0x07   /* 暂存寄存器 */
#define UART_DLL  0x00   /* 波特率低字节（DLAB=1） */
#define UART_DLM  0x01   /* 波特率高字节（DLAB=1） */

/* UART线路状态寄存器位 */
#define UART_LSR_DR      (1U << 0)  /* 数据就绪 */
#define UART_LSR_OE      (1U << 1)  /* 溢出错误 */
#define UART_LSR_PE      (1U << 2)  /* 奇偶错误 */
#define UART_LSR_FE      (1U << 3)  /* 帧错误 */
#define UART_LSR_BI      (1U << 4)  /* 中断指示 */
#define UART_LSR_THRE    (1U << 5)  /* 发送保持寄存器空 */
#define UART_LSR_TEMT    (1U << 6)  /* 发送器空 */
#define UART_LSR_RXFE    (1U << 7)  /* FIFO错误 */

/* UART线路控制寄存器位 */
#define UART_LCR_DLAB    (1U << 7)  /* 除数锁存访问位 */
#define UART_LCR_SBC     (1U << 6)  /* 设置断开 */
#define UART_LCR_SPAR    (1U << 5)  /* 强制奇偶 */
#define UART_LCR_EPAR    (1U << 4)  /* 偶校验 */
#define UART_LCR_PARITY  (1U << 3)  /* 奇偶校验使能 */
#define UART_LCR_STOP    (1U << 2)  /* 停止位 */
#define UART_LCR_WLEN8   0x03       /* 8数据位 */
#define UART_LCR_WLEN7   0x02       /* 7数据位 */
#define UART_LCR_WLEN6   0x01       /* 6数据位 */
#define UART_LCR_WLEN5   0x00       /* 5数据位 */

/* 串口数据位 */
typedef enum uart_data_bits {
    UART_DATA_BITS_5 = 5,
    UART_DATA_BITS_6 = 6,
    UART_DATA_BITS_7 = 7,
    UART_DATA_BITS_8 = 8,
} uart_data_bits_t;

/* 串口校验位 */
typedef enum uart_parity {
    UART_PARITY_NONE = 0,
    UART_PARITY_ODD,
    UART_PARITY_EVEN,
    UART_PARITY_MARK,
    UART_PARITY_SPACE,
} uart_parity_t;

/* 串口停止位 */
typedef enum uart_stop_bits {
    UART_STOP_BITS_1 = 1,
    UART_STOP_BITS_2 = 2,
} uart_stop_bits_t;

/* 串口配置 */
typedef struct uart_config {
    phys_addr_t base_addr;        /* 基地址 */
    u32        baud_rate;         /* 波特率 */
    uart_data_bits_t data_bits;   /* 数据位 */
    uart_parity_t    parity;      /* 校验位 */
    uart_stop_bits_t stop_bits;   /* 停止位 */
} uart_config_t;

/* 全局串口配置 */
extern uart_config_t g_uart_config;

/* 默认串口配置 */
#define UART_DEFAULT_BASE       0x3F8
#define UART_DEFAULT_BAUD       115200
#define UART_DEFAULT_DATA_BITS  UART_DATA_BITS_8
#define UART_DEFAULT_PARITY     UART_PARITY_NONE
#define UART_DEFAULT_STOP_BITS  UART_STOP_BITS_1

/* APM模式枚举 */
typedef enum apm_mode {
    APM_MODE_AUTO = 0,          /* 自动分配模式：引导层自动分配，写入文件，再从文件读取 */
    APM_MODE_CONFIG = 1,        /* 配置文件模式：直接在配置文件中写入，然后读取 */
} apm_mode_t;

/* 全局APM模式 */
extern apm_mode_t g_apm_mode;

/* 设置APM模式 */
void minimal_uart_set_apm_mode(apm_mode_t mode);

/* 获取APM模式 */
apm_mode_t minimal_uart_get_apm_mode(void);

/* 自动分配模式：引导层自动分配，写入文件，再从文件读取 */
void minimal_uart_init_apm_auto(const char *config_file_path);

/* 配置文件模式：直接在配置文件中写入，然后读取 */
void minimal_uart_init_apm_config(const char *yaml_data, size_t yaml_size);

/* 从引导信息获取YAML数据并初始化（配置文件模式） */
void minimal_uart_init_from_bootinfo(void);

/* 从YAML配置初始化UART（配置文件模式） */
void minimal_uart_init_from_yaml(const char *yaml_data, size_t yaml_size);

/* 使用配置初始化UART */
void minimal_uart_init_with_config(const uart_config_t *config);

/* 使用默认配置初始化UART */
void minimal_uart_init(void);

/* 发送单个字符 */
void minimal_uart_putc(char c);

/* 发送字符串 */
void minimal_uart_puts(const char *str);

/* 配置UART */
void minimal_uart_configure(const uart_config_t *config);

#endif /* HIC_KERNEL_MINIMAL_UART_H */