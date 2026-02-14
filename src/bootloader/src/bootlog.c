/**
 * HIK引导程序日志系统实现
 */

#include "bootlog.h"
#include "console.h"
#include "string.h"
#include "efi.h"
#include "stdlib.h"

/* 日志缓冲区 */
#define BOOTLOG_MAX_ENTRIES 64
static bootlog_entry_t g_bootlog_buffer[BOOTLOG_MAX_ENTRIES];
static uint32_t g_bootlog_index = 0;
static uint64_t g_boot_start_time = 0;

/* 获取时间戳（毫秒） */
static uint64_t get_timestamp(void)
{
    if (gST == NULL) {
        return 0;
    }
    
    EFI_TIME_CAPS time_caps;
    EFI_STATUS status = gST->RuntimeServices->GetTime(NULL, &time_caps);
    if (EFI_ERROR(status)) {
        return 0;
    }
    
    /* 简化实现：返回相对时间 */
    return (gST->BootServices->GetTimerValue) ? 
           gST->BootServices->GetTimerValue() : 0;
}

/* 初始化引导日志 */
void bootlog_init(void)
{
    g_boot_start_time = get_timestamp();
    g_bootlog_index = 0;
    
    console_puts("[BOOTLOG] Boot logging initialized\n");
}

/* 记录引导事件 */
void bootlog_event(bootlog_event_t type, const void* data, uint32_t len)
{
    if (g_bootlog_index >= BOOTLOG_MAX_ENTRIES) {
        /* 日志缓冲区已满，循环覆盖 */
        g_bootlog_index = 0;
    }
    
    bootlog_entry_t *entry = &g_bootlog_buffer[g_bootlog_index];
    
    entry->timestamp = get_timestamp() - g_boot_start_time;
    entry->type = type;
    entry->data_len = len < 32 ? len : 32;
    
    if (data && len > 0) {
        uint32_t copy_len = entry->data_len;
        for (uint32_t i = 0; i < copy_len; i++) {
            entry->data[i] = ((const uint8_t*)data)[i];
        }
    } else {
        for (uint32_t i = 0; i < 32; i++) {
            entry->data[i] = 0;
        }
    }
    
    g_bootlog_index++;
}

/* 记录错误 */
void bootlog_error(const char* msg)
{
    console_puts("[BOOTLOG ERROR] ");
    console_puts(msg);
    console_puts("\n");
    
    bootlog_event(BOOTLOG_ERROR, msg, strlen(msg));
}

/* 记录信息 */
void bootlog_info(const char* msg)
{
    console_puts("[BOOTLOG] ");
    console_puts(msg);
    console_puts("\n");
    
    bootlog_event(BOOTLOG_KERNEL_LOAD, msg, strlen(msg));
}
