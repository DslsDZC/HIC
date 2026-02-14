/**
 * HIK内核启动信息处理
 * 接收并处理Bootloader传递的启动信息
 */

#include <stdint.h>
#include "types.h"
#include "kernel.h"
#include "hardware_probe.h"

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

/* 调试信息 */
typedef struct {
    u16 serial_port;
    u16 debug_flags;
    void* log_buffer;
    u64 log_size;
} debug_info_t;

/* 系统信息 */
typedef struct {
    u32 cpu_count;
    u32 memory_size_mb;
    u32 architecture;
    u32 platform_type;
} system_info_t;

/* 固件信息 */
typedef struct {
    void* system_table;
    void* image_handle;
} firmware_info_t;

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
    hardware_probe_result_t hw;    // 运行时硬件探测结果
    u8 valid;                      // 信息是否有效
} boot_state_t;

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
 * 解析Bootloader传递的信息并与运行时探测结果整合
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

#endif /* BOOT_INFO_H */