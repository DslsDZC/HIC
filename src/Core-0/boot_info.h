/*
 * HIK内核启动信息处理
 * 接收并处理Bootloader传递的启动信息
 */

#ifndef HIK_KERNEL_BOOT_INFO_H
#define HIK_KERNEL_BOOT_INFO_H

#include <stdint.h>
#include "types.h"
#include "kernel.h"
#include "hardware_probe.h"

/* ACPI类型定义 */
#define ACPI_SIG_RSDP  "RSD PTR "
#define ACPI_SIG_RSDT  "RSDT"
#define ACPI_SIG_XSDT  "XSDT"

/* ACPI RSDP结构 */
typedef struct {
    u8 signature[8];        /* "RSD PTR " */
    u8 checksum;
    u8 oem_id[6];
    u8 revision;
    u32 rsdt_address;       /* RSDT物理地址 */
    u32 length;
    u64 xsdt_address;       /* XSDT物理地址 (ACPI 2.0+) */
    u8 extended_checksum;
    u8 reserved[3];
} acpi_rsdp_t;

/* ACPI SDT表头 */
typedef struct {
    char signature[4];      /* 表签名 */
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    char oem_revision[4];
    char creator_id[4];
    char creator_revision[4];
} acpi_sdt_header_t;

/* ACPI RSDT结构 */
typedef struct {
    acpi_sdt_header_t header;
    u32 entry_pointers[];    /* 指向其他SDT的指针数组 */
} acpi_rsdt_t;

/* ACPI XSDT结构 */
typedef struct {
    acpi_sdt_header_t header;
    u64 entry_pointers[];    /* 指向其他SDT的指针数组 (64位) */
} acpi_xsdt_t;

/* 引导信息魔数 */
#define HIK_BOOT_INFO_MAGIC  0x48494B21  // "HIK!"
#define HIK_BOOT_INFO_VERSION 1

/* 内存映射条目类型 */
#define HIK_MEM_TYPE_USABLE      1
#define HIK_MEM_TYPE_RESERVED    2
#define HIK_MEM_TYPE_ACPI        3
#define HIK_MEM_TYPE_NVS         4
#define HIK_MEM_TYPE_UNUSABLE    5
#define HIK_MEM_TYPE_BOOTLOADER  6
#define HIK_MEM_TYPE_KERNEL      7
#define HIK_MEM_TYPE_MODULE      8

/* 内存映射条目 */
typedef struct {
    u64 base;
    u64 length;
    u32 type;
    u32 flags;
} hik_mem_entry_t;

/* 视频信息 */
typedef struct {
    u32 framebuffer_base;
    u32 framebuffer_size;
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
} video_info_t;

/* 前向声明（避免循环依赖） */
struct hardware_probe_result;
typedef struct hardware_probe_result hardware_probe_result_t;

/* HIK引导信息结构 */
typedef struct {
    u32 magic;                    // 魔数 "HIK!"
    u32 version;                  // 结构版本
    u64 flags;                    // 特性标志位
    
    // 内存信息
    hik_mem_entry_t* mem_map;
    u64 mem_map_size;
    u64 mem_map_desc_size;
    u64 mem_map_entry_count;
    
    // ACPI信息
    void* rsdp;                   // ACPI RSDP指针
    void* xsdp;                   // ACPI XSDP指针 (UEFI)
    bool acpi_valid;             // ACPI是否有效
    
    // 固件信息
    firmware_info_t firmware;
    
    // 内核映像信息
    void* kernel_base;
    u64 kernel_size;
    u64 entry_point;
    
    /* 命令行 */
    char cmdline[256];
    
    /* 设备树（架构无关） */
    void* device_tree;
    u64 device_tree_size;
    
    /* 模块信息 */
    struct {
        void* base;
        u64 size;
        char name[64];
    } modules[16];
    u64 module_count;
    
    /* 引导日志信息 */
    struct {
        void* log_buffer;       /* 引导日志缓冲区地址 */
        u64 log_size;           /* 日志大小 */
        u32 log_entry_count;     /* 日志条目数 */
        u64 boot_time;           /* 引导开始时间戳 */
    } boot_log;
    
    /* 系统信息 */
    system_info_t system;
    
    /* 配置数据（platform.yaml等） */
    void* config_data;
    u64 config_size;
    
    /* 固件类型 */
    u8 firmware_type;
    u8 reserved[7];
    
    /* 栈信息 */
    u64 stack_top;
    u64 stack_size;
    
    /* 视频信息 */
    video_info_t video;
    
    /* 调试信息 */
    debug_info_t debug;
    
} hik_boot_info_t;

/* 标志位定义 */
#define HIK_BOOT_FLAG_SECURE_BOOT   (1ULL << 0)
#define HIK_BOOT_FLAG_ACPI_ENABLED  (1ULL << 1)
#define HIK_BOOT_FLAG_VIDEO_ENABLED (1ULL << 2)
#define HIK_BOOT_FLAG_DEBUG_ENABLED (1ULL << 3)
#define HIK_BOOT_FLAG_RECOVERY_MODE (1ULL << 4)

/* 启动信息处理状态 */
typedef struct boot_state {
    hik_boot_info_t* boot_info;    // Bootloader传递的信息
    hardware_probe_result_t hw;    // 静态硬件探测结果
    u8 valid;                      // 信息是否有效
    
    /* 扩展字段 */
    bool recovery_mode;            // 恢复模式
    u16 serial_port;               // 串口端口
    u32 serial_baud;               // 串口波特率
    bool debug_enabled;            // 调试模式
    bool quiet_mode;               // 静默模式
} boot_state_t;

/* 全局启动状态 */
extern boot_state_t g_boot_state;

/* 外部API声明 */

/**
 * 内核入口点
 * 接收Bootloader传递的启动信息
 * 
 * 参数：
 *   boot_info - Bootloader传递的启动信息结构
 */
void kernel_entry(hik_boot_info_t* boot_info);

/**
 * 验证启动信息
 * 
 * 参数：
 *   boot_info - 启动信息
 * 
 * 返回值：验证通过返回true
 */
bool boot_info_validate(hik_boot_info_t* boot_info);

/**
 * 处理启动信息
 * 解析Bootloader传递的信息并与静态探测结果整合
 * 
 * 参数：
 *   boot_info - Bootloader传递的启动信息
 */
void boot_info_process(hik_boot_info_t* boot_info);

/**
 * 初始化内存管理器
 * 使用Bootloader传递的内存映射
 * 
 * 参数：
 *   boot_info - 启动信息
 */
void boot_info_init_memory(hik_boot_info_t* boot_info);

/**
 * 初始化ACPI
 * 使用Bootloader传递的ACPI表
 * 
 * 参数：
 *   boot_info - 启动信息
 */
void boot_info_init_acpi(hik_boot_info_t* boot_info);

/**
 * 解析命令行参数
 * 
 * 参数：
 *   cmdline - 命令行字符串
 */
void boot_info_parse_cmdline(const char* cmdline);

/**
 * 获取启动状态
 * 
 * 返回值：启动状态指针
 */
boot_state_t* get_boot_state(void);

/**
 * 打印启动信息摘要
 */
void boot_info_print_summary(void);

/* 内核主循环 */
void kernel_main_loop(void);

/* 中断处理 */
bool interrupts_pending(void);
void handle_pending_interrupts(void);

/* 系统调用处理 */
bool syscalls_pending(void);
void handle_pending_syscalls(void);

/* 定时器 */
void timer_update(void);

/* 内核维护任务 */
void kernel_maintenance_tasks(void);

/* 硬件探测 */
void probe_all_hardware(hardware_probe_result_t *result);

/* 内存管理 */
void pmm_mark_used(u64 base, u64 size);

/* ACPI */
void boot_info_parse_acpi_tables(void *sdt, const char *signature);

#endif /* BOOT_INFO_H */