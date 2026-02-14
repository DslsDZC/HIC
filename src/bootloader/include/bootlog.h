/**
 * HIK引导程序日志系统
 * 从引导开始记录所有关键操作
 */

#ifndef BOOTLOADER_BOOTLOG_H
#define BOOTLOADER_BOOTLOG_H

#include <stdint.h>

/* 引导日志事件类型 */
typedef enum {
    BOOTLOG_UEFI_INIT,
    BOOTLOG_CONFIG_LOAD,
    BOOTLOG_KERNEL_LOAD,
    BOOTLOG_KERNEL_VERIFY,
    BOOTLOG_MEMORY_MAP,
    BOOTLOG_ACPI_FIND,
    BOOTLOG_EXIT_BOOT_SERVICES,
    BOOTLOG_JUMP_TO_KERNEL,
    BOOTLOG_ERROR,
} bootlog_event_t;

/* 引导日志条目 */
typedef struct {
    uint64_t timestamp;     /* 时间戳（ms） */
    bootlog_event_t type;   /* 事件类型 */
    uint32_t data_len;      /* 数据长度 */
    uint8_t data[32];       /* 事件数据 */
} bootlog_entry_t;

/* 日志接口 */
void bootlog_init(void);
void bootlog_event(bootlog_event_t type, const void* data, uint32_t len);
void bootlog_error(const char* msg);
void bootlog_info(const char* msg);

#endif /* BOOTLOADER_BOOTLOG_H */