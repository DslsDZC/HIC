#ifndef HIK_BOOTLOADER_BOOT_INFO_H
#define HIK_BOOTLOADER_BOOT_INFO_H

#include <stdint.h>

// HIK引导信息结构魔数
#define HIK_BOOT_INFO_MAGIC  0x48494B21  // "HIK!"

// 引导信息结构版本
#define HIK_BOOT_INFO_VERSION 1

// 内存映射条目类型
#define HIK_MEM_TYPE_USABLE      1
#define HIK_MEM_TYPE_RESERVED    2
#define HIK_MEM_TYPE_ACPI        3
#define HIK_MEM_TYPE_NVS         4
#define HIK_MEM_TYPE_UNUSABLE    5
#define HIK_MEM_TYPE_BOOTLOADER  6
#define HIK_MEM_TYPE_KERNEL      7
#define HIK_MEM_TYPE_MODULE      8

// 内存映射条目
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t flags;
} hik_mem_entry_t;

// HIK引导信息结构
typedef struct {
    uint32_t magic;                    // 魔数 "HIK!"
    uint32_t version;                  // 结构版本
    uint64_t flags;                    // 特性标志位
    
    // 内存信息
    hik_mem_entry_t *mem_map;
    uint64_t mem_map_size;
    uint64_t mem_map_desc_size;
    uint64_t mem_map_entry_count;
    
    // ACPI信息
    void *rsdp;                        // ACPI RSDP指针
    void *xsdp;                        // ACPI XSDP指针 (UEFI)
    
    // 固件信息
    union {
        struct {
            void *system_table;        // UEFI系统表
            void *image_handle;        // UEFI映像句柄
        } uefi;
        struct {
            void *bios_data_area;      // BIOS数据区指针
            uint32_t vbe_info;         // VESA信息块
        } bios;
    } firmware;
    
    // 内核映像信息
    void *kernel_base;
    uint64_t kernel_size;
    uint64_t entry_point;
    
    // 命令行
    char cmdline[256];
    
    // 设备树（x86通常为空）
    void *device_tree;
    uint64_t device_tree_size;
    
    // 模块信息（用于动态模块加载）
    struct {
        void *base;
        uint64_t size;
        char name[64];
    } modules[16];
    uint64_t module_count;
    
    // 系统信息
    struct {
        uint32_t cpu_count;
        uint32_t memory_size_mb;
        uint32_t architecture;         // 1=x86_64, 2=ARM64
        uint32_t platform_type;        // 1=UEFI, 2=BIOS
    } system;
    
    // 固件类型
    uint8_t firmware_type;             // 0=UEFI, 1=BIOS
    uint8_t reserved[7];
    
    // 栈信息
    uint64_t stack_top;
    uint64_t stack_size;
    
    // 视频信息
    struct {
        uint32_t framebuffer_base;
        uint32_t framebuffer_size;
        uint32_t width;
        uint32_t height;
        uint32_t pitch;
        uint32_t bpp;
    } video;
    
    // 调试信息
    struct {
        uint16_t serial_port;          // 串口端口 (如0x3F8)
        uint16_t debug_flags;
        void *log_buffer;              // 日志缓冲区
        uint64_t log_size;
    } debug;
    
} hik_boot_info_t;

// 标志位定义
#define HIK_BOOT_FLAG_SECURE_BOOT   (1ULL << 0)
#define HIK_BOOT_FLAG_ACPI_ENABLED  (1ULL << 1)
#define HIK_BOOT_FLAG_VIDEO_ENABLED (1ULL << 2)
#define HIK_BOOT_FLAG_DEBUG_ENABLED (1ULL << 3)
#define HIK_BOOT_FLAG_RECOVERY_MODE (1ULL << 4)

// 引导信息总大小
#define HIK_BOOT_INFO_SIZE  4096

#endif // HIK_BOOTLOADER_BOOT_INFO_H