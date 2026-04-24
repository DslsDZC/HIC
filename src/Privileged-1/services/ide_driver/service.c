/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * IDE/ATA 磁盘驱动服务
 * 
 * 运行在 Privileged-1 层，提供磁盘访问能力
 * 支持 LBA28 模式读取扇区
 */

#include "service.h"
#include <stdint.h>
#include <string.h>

/* ==================== 服务入口点（必须在代码段最前面） ==================== */

/**
 * 服务入口点 - 必须放在代码段最前面
 * 使用 section 属性确保此函数在链接时位于 .static_svc.ide_driver.text 的开头
 */
__attribute__((section(".static_svc.ide_driver.text"), used, noinline))
int _ide_driver_entry(void)
{
    /* 初始化 IDE 驱动 */
    ide_driver_init();
    
    /* 启动服务主循环 */
    ide_driver_start();
    
    /* 服务不应返回，如果返回则进入无限循环 */
    while (1) {
        __asm__ volatile("hlt");
    }
    
    return 0;
}

/* IDE 端口定义 */
#define IDE_PRIMARY_DATA        0x1F0
#define IDE_PRIMARY_ERROR       0x1F1
#define IDE_PRIMARY_FEATURES    0x1F1
#define IDE_PRIMARY_SECTOR_COUNT 0x1F2
#define IDE_PRIMARY_LBA_LOW     0x1F3
#define IDE_PRIMARY_LBA_MID     0x1F4
#define IDE_PRIMARY_LBA_HIGH    0x1F5
#define IDE_PRIMARY_DRIVE       0x1F6
#define IDE_PRIMARY_STATUS      0x1F7
#define IDE_PRIMARY_COMMAND     0x1F7
#define IDE_PRIMARY_CONTROL     0x3F6

/* IDE 命令 */
#define IDE_CMD_READ_SECTORS    0x20
#define IDE_CMD_IDENTIFY        0xEC

/* 状态位 */
#define IDE_STATUS_ERR          0x01
#define IDE_STATUS_DRQ          0x08
#define IDE_STATUS_RDY          0x40
#define IDE_STATUS_BSY          0x80

/* 服务状态 */
static struct {
    int initialized;
    int drive_present;
    uint32_t total_sectors;
} g_ide_state = {0};

/* 扇区缓冲区 */
static uint8_t g_sector_buffer[512];

/* 外部函数 - 来自内核的原语 */
extern void serial_print(const char *msg);
extern void thread_yield(void);

/* I/O 端口访问 - 通过内核 HAL */
extern uint8_t hal_inb(uint16_t port);
extern void hal_outb(uint16_t port, uint8_t value);
extern uint16_t hal_inw(uint16_t port);

/* 直接输出字符到串口 */
static void ide_putchar(char c) {
    extern void hal_outb(uint16_t port, uint8_t value);
    hal_outb(0x3F8, (uint8_t)c);
}

/* 日志输出 */
static void ide_log(const char *msg) {
    const char *prefix = "[IDE_DRV] ";
    while (*prefix) {
        ide_putchar(*prefix++);
    }
    while (*msg) {
        ide_putchar(*msg++);
    }
    ide_putchar('\n');
}

/* 延时 - 使用更多循环确保稳定 */
static void ide_delay(void) {
    for (volatile int i = 0; i < 1000; i++) {
        hal_inb(0x177);  /* 读取备用状态端口 */
    }
}

/* 等待磁盘就绪 */
static int ide_wait_ready(void) {
    uint8_t status;
    int timeout = 1000000;  /* 增加超时 */
    
    while (timeout-- > 0) {
        status = hal_inb(IDE_PRIMARY_STATUS);
        
        if (status & IDE_STATUS_ERR) {
            ide_log("Error in wait_ready");
            return -1;
        }
        
        if (!(status & IDE_STATUS_BSY)) {
            return 0;
        }
    }
    
    ide_log("Timeout waiting for ready");
    return -1;
}

/* 等待数据就绪 */
static int ide_wait_data(void) {
    uint8_t status;
    int timeout = 1000000;  /* 增加超时 */

    while (timeout-- > 0) {
        status = hal_inb(IDE_PRIMARY_STATUS);

        if (status & IDE_STATUS_ERR) {
            /* 读取错误寄存器获取更多信息 */
            uint8_t error = hal_inb(0x1F1);  /* 错误寄存器 */
            ide_log("IDE error detected");
            /* 输出错误码 */
            char buf[32];
            buf[0] = 'E'; buf[1] = 'R'; buf[2] = 'R'; buf[3] = ':';
            buf[4] = '0'; buf[5] = 'x';
            const char hex[] = "0123456789ABCDEF";
            buf[6] = hex[(error >> 4) & 0xF];
            buf[7] = hex[error & 0xF];
            buf[8] = ' ';
            buf[9] = 'S'; buf[10] = ':';
            buf[11] = hex[(status >> 4) & 0xF];
            buf[12] = hex[status & 0xF];
            buf[13] = '\n';
            buf[14] = '\0';
            serial_print(buf);
            return -1;
        }

        if ((status & IDE_STATUS_DRQ) && !(status & IDE_STATUS_BSY)) {
            return 0;
        }

        /* If drive is idle (BSY=0, DRQ=0, ERR=0), command may not have been accepted */
        if (!(status & IDE_STATUS_BSY) && !(status & IDE_STATUS_DRQ) && !(status & IDE_STATUS_ERR)) {
            /* Give the controller more time by a small delay and retry */
            for (volatile int d = 0; d < 100; d++) {
                hal_inb(0x177);
            }
        }
    }

    ide_log("Timeout waiting for data");
    return -1;
}

/**
 * 初始化 IDE 驱动
 */
hic_status_t ide_driver_init(void) {
    uint8_t status;
    
    ide_log("Initializing IDE driver...");
    
    ide_log("Step 1: Reset and select drive...");
    /* 软复位 */
    hal_outb(IDE_PRIMARY_CONTROL, 0x04);  /* 设置 SRST */
    ide_delay();
    hal_outb(IDE_PRIMARY_CONTROL, 0x00);  /* 清除 SRST */
    ide_delay();
    
    /* 选择主驱动器，LBA 模式 */
    hal_outb(IDE_PRIMARY_DRIVE, 0xE0);  /* LBA 模式 */
    ide_delay();
    
    /* 等待驱动器就绪 */
    ide_log("Step 2: Wait for ready...");
    if (ide_wait_ready() < 0) {
        ide_log("Drive not ready after reset");
        /* 继续尝试 */
    }
    
    ide_log("Step 3: Write test pattern...");
    /* 测试驱动器是否存在 */
    hal_outb(IDE_PRIMARY_SECTOR_COUNT, 0x55);
    hal_outb(IDE_PRIMARY_LBA_LOW, 0xAA);
    
    ide_log("Step 4: Read back...");
    uint8_t count = hal_inb(IDE_PRIMARY_SECTOR_COUNT);
    uint8_t lba = hal_inb(IDE_PRIMARY_LBA_LOW);
    
    ide_log("Step 5: Check result...");
    if (count == 0x55 && lba == 0xAA) {
        ide_log("Primary master detected");

        /* ！！！调试：逐句测试崩溃点 */
        serial_print("[IDE_DRV] DBG1: before drive_present=1\n");
        g_ide_state.drive_present = 1;
        serial_print("[IDE_DRV] DBG2: after drive_present=1\n");
        g_ide_state.initialized = 1;
        serial_print("[IDE_DRV] DBG3: after initialized=1\n");

        /* 读取状态 */
        status = hal_inb(IDE_PRIMARY_STATUS);
        serial_print("[IDE_DRV] DBG4: after hal_inb(0x1F7)\n");

        serial_print("[IDE_DRV] DBG5: returning SUCCESS from init\n");
        return HIC_SUCCESS;
    }
    
    ide_log("No IDE drive found");
    return HIC_ERROR;
}

/**
 * 启动 IDE 驱动服务
 */
hic_status_t ide_driver_start(void) {
    extern void serial_print(const char *);

    /* 首先初始化驱动 */
    serial_print("[IDE_DRV] DEBUG: about to call ide_driver_init()\n");
    hic_status_t status = ide_driver_init();
    serial_print("[IDE_DRV] DEBUG: back from ide_driver_init(), status=");
    /* manual hex print of status */
    {
        char buf[20];
        buf[0] = '0'; buf[1] = 'x';
        const char hex[] = "0123456789ABCDEF";
        buf[2] = hex[((u32)status >> 28) & 0xF];
        buf[3] = hex[((u32)status >> 24) & 0xF];
        buf[4] = hex[((u32)status >> 20) & 0xF];
        buf[5] = hex[((u32)status >> 16) & 0xF];
        buf[6] = hex[((u32)status >> 12) & 0xF];
        buf[7] = hex[((u32)status >> 8) & 0xF];
        buf[8] = hex[((u32)status >> 4) & 0xF];
        buf[9] = hex[status & 0xF];
        buf[10] = '\n';
        buf[11] = '\0';
        serial_print(buf);
    }

    if (status != HIC_SUCCESS) {
        ide_log("Drive not initialized");
    } else {
        serial_print("[IDE_DRV] DEBUG: about to log success\n");
        ide_log("Drive initialized successfully");
        serial_print("[IDE_DRV] DEBUG: after log success\n");
    }

    serial_print("[IDE_DRV] DEBUG: about to log service started\n");
    ide_log("Service started");
    serial_print("[IDE_DRV] DEBUG: entering service loop\n");
    
    /* 主服务循环 */
    while (1) {
        thread_yield();
    }
    
    return HIC_SUCCESS;
}

/**
 * 读取扇区
 * @param lba: LBA 地址
 * @param buffer: 512字节缓冲区
 * @return: 0 成功，-1 失败
 */
int ide_read_sector(uint32_t lba, void *buffer) {
    uint16_t *data = (uint16_t *)buffer;
    
    if (!g_ide_state.initialized || !g_ide_state.drive_present) {
        ide_log("Drive not initialized");
        return -1;
    }
    
    /* 选择驱动器并设置 LBA 高4位 */
    uint8_t drive_select = 0xE0 | ((lba >> 24) & 0x0F);  /* LBA 模式，使用 0xE0 基址 */
    hal_outb(IDE_PRIMARY_DRIVE, drive_select);
    ide_delay();
    
    /* 等待驱动器就绪 */
    if (ide_wait_ready() < 0) {
        return -1;
    }
    
    /* 设置参数 */
    hal_outb(IDE_PRIMARY_SECTOR_COUNT, 1);
    hal_outb(IDE_PRIMARY_LBA_LOW, lba & 0xFF);
    hal_outb(IDE_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    hal_outb(IDE_PRIMARY_LBA_HIGH, (lba >> 16) & 0xFF);
    
    /* 发送读取命令 */
    hal_outb(IDE_PRIMARY_COMMAND, IDE_CMD_READ_SECTORS);
    
    /* 等待数据就绪 */
    if (ide_wait_data() < 0) {
        return -1;
    }
    
    /* 读取数据 */
    for (int i = 0; i < 256; i++) {
        data[i] = hal_inw(IDE_PRIMARY_DATA);
    }
    
    /* 400ns 延迟 */
    ide_delay();
    
    return 0;
}

/**
 * 读取多个扇区
 */
int ide_read_sectors(uint32_t lba, uint8_t count, void *buffer) {
    uint8_t *buf = (uint8_t *)buffer;
    
    for (int i = 0; i < count; i++) {
        if (ide_read_sector(lba + i, buf + i * 512) < 0) {
            return -1;
        }
    }
    
    return 0;
}

/* 服务接口 */
hic_status_t ide_driver_service_init(void) {
    return ide_driver_init();
}

hic_status_t ide_driver_service_start(void) {
    return ide_driver_start();
}

hic_status_t ide_driver_service_stop(void) {
    return HIC_SUCCESS;
}

hic_status_t ide_driver_service_cleanup(void) {
    return HIC_SUCCESS;
}
