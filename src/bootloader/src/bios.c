/**
 * HIC BIOS Bootloader
 * 第一引导层：BIOS引导加载程序
 * 
 * 负责：
 * 1. 初始化BIOS环境
 * 2. 加载并验证内核映像
 * 3. 构建启动信息结构(BIS)
 * 4. 切换到保护模式/长模式
 * 5. 跳转到内核入口点
 * 
 * 兼容Legacy BIOS系统（非UEFI）
 */

#include <stdint.h>
#include "boot_info.h"
#include "kernel_image.h"
#include "console.h"
#include "string.h"
#include "crypto.h"

/* BIOS中断调用 */
#define BIOS_INT_DISK          0x13
#define BIOS_INT_VIDEO         0x10
#define BIOS_INT_MEMORY        0x15
#define BIOS_INT_APM           0x15
#define BIOS_INT_KEYBOARD      0x16

/* 磁盘驱动器号 */
#define BIOS_DISK_HD0          0x80
#define BIOS_DISK_HD1          0x81

/* 视频模式 */
#define BIOS_VIDEO_TEXT_80x25  0x03
#define BIOS_VIDEO_GRAPHICS     0x12

/* 内存映射类型 */
#define BIOS_MEMORY_MAP        0xE820

/* 全局变量 */
static hic_boot_info_t *g_boot_info = NULL;
static uint8_t *g_kernel_data = NULL;
static uint64_t g_kernel_size = 0;

/* 函数前置声明 */
static void bios_init(void);
static void bios_init_video(void);
static void bios_init_memory(void);
static int bios_load_kernel(const char *path);
static void bios_prepare_boot_info(void);
static void bios_get_memory_map(hic_boot_info_t *boot_info);
static void bios_find_acpi_tables(hic_boot_info_t *boot_info);
static void bios_get_system_info(hic_boot_info_t *boot_info);
static void bios_switch_to_protected_mode(void);
static void bios_switch_to_long_mode(hic_boot_info_t *boot_info);
__attribute__((noreturn)) void bios_jump_to_kernel(hic_boot_info_t *boot_info);
static void bios_panic(const char *message);

/**
 * BIOS入口点
 * 由bootloader汇编代码调用
 */
void bios_main(void)
{
    // 初始化BIOS环境
    bios_init();
    
    log_info("HIC BIOS Bootloader v1.0\n");
    log_info("hello world\n");
    
    // 加载内核
    if (bios_load_kernel("\\HIC\\KERNEL.HIC") != 0) {
        bios_panic("Failed to load kernel");
    }
    
    // 准备启动信息
    bios_prepare_boot_info();
    
    // 切换到长模式并跳转到内核
    bios_switch_to_long_mode(g_boot_info);
    
    // 永远不会到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * 初始化BIOS环境
 */
static void bios_init(void)
{
    // 初始化视频
    bios_init_video();
    
    // 初始化控制台
    console_init_bios();
    
    log_info("BIOS Bootloader initializing...\n");
    
    // 初始化内存管理
    bios_init_memory();
}

/**
 * 初始化视频
 */
static void bios_init_video(void)
{
    // 设置文本模式 80x25
    __asm__ volatile (
        "mov $0x03, %%ah\n"
        "int $0x10\n"
        : : : "ax"
    );
    
    // 清屏
    __asm__ volatile (
        "mov $0x0600, %%ax\n"   // AH=0x06 (滚动), AL=0x00 (清屏)
        "mov $0, %%cx\n"        // CH=0, CL=0 (左上角)
        "mov $0x184f, %%dx\n"   // DH=0x18, DL=0x4f (右下角 80x25)
        "mov $0x07, %%bh\n"     // BH=0x07 (属性)
        "int $0x10\n"
        : : : "ax", "bx", "cx", "dx"
    );
}

/**
 * 初始化内存管理
 */
static void bios_init_memory(void)
{
    // 获取EBDA (Extended BIOS Data Area) 地址
    uint16_t ebda_seg;
    __asm__ volatile (
        "mov $0x40, %%ax\n"
        "mov %%ax, %%es\n"
        "movw $0x0E, %%ax\n"
        : "=a"(ebda_seg)
        :
    );
    
    log_info("Memory management initialized (EBDA at 0x%04x:0000)\n", ebda_seg);
}

/**
 * 加载内核
 */
static int bios_load_kernel(const char *path)
{
    // 使用BIOS INT 0x13从磁盘加载内核
    // 假设内核文件在第一个硬盘的第一个分区的 /HIC/KERNEL.HIC
    
    uint8_t *kernel_base = (uint8_t *)0x100000;  // 1MB处
    uint8_t *load_addr = kernel_base;
    uint32_t sectors_read = 0;
    uint32_t total_sectors = 0;
    
    log_info("Loading kernel from: %s\n", path);
    
    // 读取多个扇区到临时缓冲区
    uint8_t *temp_buffer = (uint8_t *)0x80000;  // 512KB临时缓冲区
    
    // 读取前100个扇区（足够读取内核头部）
    uint8_t disk_drive = BIOS_DISK_HD0;
    uint16_t cylinders = 0;
    uint16_t sectors = 0;
    uint16_t heads = 0;
    
    // 获取磁盘参数
    __asm__ volatile (
        "mov $0x08, %%ah\n"
        "mov %1, %%dl\n"
        "int $0x13\n"
        "jc disk_error\n"
        : "=c"(cylinders), "=d"(sectors)
        : "a"((disk_drive << 8) | 0x08)
        : "bx", "si"
    );
    
    heads = (sectors >> 8) & 0xFF;
    sectors = sectors & 0x3F;
    cylinders = (cylinders >> 2) & 0x3FF;
    
    log_info("Disk: C=%u, H=%u, S=%u\n", cylinders, heads, sectors);
    
    /* 读取内核文件（完整实现：解析FAT32文件系统） */
    /* 1. 读取MBR，查找活动分区 */
    /* 2. 读取分区引导记录（PBR），解析FAT32 BPB */
    /* 3. 读取根目录，查找内核文件 */
    /* 4. 读取内核文件数据 */
    
    /* 完整实现：解析FAT32文件系统 */
    uint8_t mbr[512];
    if (bios_read_lba(0, 1, (uint16_t*)mbr) != 0) {
        log_error("Failed to read MBR\n");
        return;
    }
    
    /* 查找活动分区 */
    uint8_t partition_offset = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t* pte = &mbr[446 + i * 16];
        if (pte[0] == 0x80) {  /* 活动分区 */
            partition_offset = *(uint32_t*)&pte[8];
            log_info("Found active partition at LBA %u\n", partition_offset);
            break;
        }
    }
    
    if (partition_offset == 0) {
        log_error("No active partition found\n");
        return;
    }
    
    /* 读取分区引导记录（PBR） */
    uint8_t pbr[512];
    if (bios_read_lba(partition_offset, 1, (uint16_t*)pbr) != 0) {
        log_error("Failed to read PBR\n");
        return;
    }
    
    /* 解析FAT32 BPB */
    uint16_t bytes_per_sector = *(uint16_t*)&pbr[11];
    uint8_t sectors_per_cluster = pbr[13];
    uint16_t reserved_sectors = *(uint16_t*)&pbr[14];
    uint8_t num_fats = pbr[16];
    uint32_t sectors_per_fat = *(uint32_t*)&pbr[36];
    uint32_t root_cluster = *(uint32_t*)&pbr[44];
    
    uint32_t fat_start = partition_offset + reserved_sectors;
    uint32_t data_start = fat_start + num_fats * sectors_per_fat;
    
    log_info("FAT32: FAT start=%u, data start=%u, root cluster=%u\n",
            fat_start, data_start, root_cluster);
    
    /* 读取根目录，查找内核文件 */
    uint8_t* kernel_data = (uint8_t*)KERNEL_LOAD_ADDR;
    uint32_t kernel_size = 0;
    
    if (fat32_find_and_read_file(partition_offset, fat_start, data_start,
                                 sectors_per_cluster, bytes_per_sector,
                                 root_cluster, "HICKERNL", 
                                 kernel_data, &kernel_size) != 0) {
        log_error("Failed to find kernel file\n");
        return;
    }
    
    log_info("Kernel loaded: %u bytes at 0x%x\n", kernel_size, KERNEL_LOAD_ADDR);
    
    // 读取扇区
    for (uint32_t i = 0; i < sector_count; i++) {
        uint32_t current_lba = lba + i;
        
        // LBA转CHS
        uint32_t c = current_lba / (heads * sectors);
        uint32_t h = (current_lba / sectors) % heads;
        uint32_t s = (current_lba % sectors) + 1;
        
        __asm__ volatile (
            "mov $0x02, %%ah\n"      // 读扇区
            "mov $1, %%al\n"         // 读取1个扇区
            "mov %2, %%ch\n"         // 柱号（低8位）
            "mov %3, %%cl\n"         // 扇号（低6位）+ 柱号（高2位）
            "mov %4, %%dh\n"         // 磁头号
            "mov %5, %%dl\n"         // 驱动器号
            "mov %6, %%bx\n"         // 目标偏移
            "mov $0x8000, %%es\n"    // 目标段
            "int $0x13\n"
            "jc read_error\n"
            :
            : "a"((disk_drive << 8) | 0x02),
              "c"(((c >> 2) & 0xFF) | ((c & 0x03) << 6) | s),
              "r"(c & 0xFF),
              "r"(s | ((c >> 8) & 0x03) << 6),
              "r"(h),
              "r"(disk_drive),
              "r"((uint16_t)(i * 512) & 0xFFFF)
            : "bx", "dx", "si"
        );
        
        // 复制到内核加载地址
        uint8_t *src = (uint8_t *)(0x80000 + (i * 512));
        uint8_t *dst = load_addr + (i * 512);
        
        for (int j = 0; j < 512; j++) {
            dst[j] = src[j];
        }
        
        sectors_read++;
    }
    
    // 验证内核映像
    hic_image_header_t *header = (hic_image_header_t *)kernel_base;
    
    if (header->magic[0] != 'H' || header->magic[1] != 'I' || 
        header->magic[2] != 'K' || header->magic[3] != '_' ||
        header->magic[4] != 'I' || header->magic[5] != 'M' ||
        header->magic[6] != 'G' || header->magic[7] != '\0') {
        log_error("Invalid kernel magic: 0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                 header->magic[0], header->magic[1], header->magic[2], header->magic[3],
                 header->magic[4], header->magic[5], header->magic[6], header->magic[7]);
        return 0;
    }
    
    if ((header->version >> 8) != 0 || (header->version & 0xFF) != 1) {
    
    // 计算需要的总扇区数
    total_sectors = (header->image_size + 511) / 512;
    
    // 如果需要读取更多扇区
    if (total_sectors > sectors_read) {
        log_info("Need to read more sectors: %u total, %u already read\n",
                total_sectors, sectors_read);
        
        // 继续读取剩余扇区
        for (uint32_t i = sectors_read; i < total_sectors; i++) {
            uint32_t current_lba = lba + i;
            
            uint32_t c = current_lba / (heads * sectors);
            uint32_t h = (current_lba / sectors) % heads;
            uint32_t s = (current_lba % sectors) + 1;
            
            __asm__ volatile (
                "mov $0x02, %%ah\n"
                "mov $1, %%al\n"
                "int $0x13\n"
                "jc read_error\n"
                :
                : "a"((disk_drive << 8) | 0x02),
                  "c"(((c >> 2) & 0xFF) | ((c & 0x03) << 6) | s),
                  "d"((h << 8) | disk_drive),
                  "b"((uint16_t)((i * 512) & 0xFFFF))
                : "si"
            );
            
            // 复制到内核加载地址
            uint8_t *src = (uint8_t *)(0x80000 + ((i - sectors_read) * 512));
            uint8_t *dst = load_addr + (i * 512);
            
            for (int j = 0; j < 512; j++) {
                dst[j] = src[j];
            }
        }
        
        sectors_read = total_sectors;
    }
    
    // 验证签名
    if (!hic_image_verify_kernel(kernel_base, header->image_size)) {
        log_error("Kernel signature verification failed\n");
        return -1;
    }
    
    g_kernel_data = kernel_base;
    g_kernel_size = header->image_size;
    
    log_info("Kernel loaded at 0x%08x, size: %lu bytes (%u sectors)\n", 
            (uint32_t)kernel_base, g_kernel_size, sectors_read);
    
    return 0;

read_error:
    log_error("Disk read error at sector %u\n", sectors_read);
    return -1;
}

/**
 * 准备启动信息
 */
static void bios_prepare_boot_info(void)
{
    // 分配启动信息结构
    g_boot_info = (hic_boot_info_t *)0x8000;  // 低内存区域
    
    // 清零结构
    memset(g_boot_info, 0, sizeof(hic_boot_info_t));
    
    // 填充基本信息
    g_boot_info->magic = HIC_BOOT_INFO_MAGIC;
    g_boot_info->version = HIC_BOOT_INFO_VERSION;
    g_boot_info->flags = 0;
    g_boot_info->firmware_type = 1;  // BIOS
    
    // 保存固件信息
    g_boot_info->firmware.bios.bios_data_area = (void *)0x400;
    
    // 内核信息
    hic_image_header_t *header = (hic_image_header_t *)g_kernel_data;
    g_boot_info->kernel_base = g_kernel_data;
    g_boot_info->kernel_size = g_kernel_size;
    g_boot_info->entry_point = header->entry_point;
    
    // 获取内存映射
    bios_get_memory_map(g_boot_info);
    
    // 查找ACPI表
    bios_find_acpi_tables(g_boot_info);
    
    // 获取系统信息
    bios_get_system_info(g_boot_info);
    
    // 设置栈
    g_boot_info->stack_size = 0x10000;  // 64KB栈
    g_boot_info->stack_top = 0x100000;  // 栈顶在1MB处
    
    // 调试信息
    g_boot_info->flags |= HIC_BOOT_FLAG_DEBUG_ENABLED;
    g_boot_info->debug.serial_port = 0x3F8;  // COM1
}

/**
 * 获取内存映射
 */
static void bios_get_memory_map(hic_boot_info_t *boot_info)
{
    // 使用BIOS INT 0x15, EAX=0xE820获取内存映射
    
    struct bios_memory_map_entry {
        uint64_t base;
        uint64_t length;
        uint32_t type;
        uint32_t acpi_attrs;
    } __attribute__((packed));
    
    bios_memory_map_entry_t *bios_map = 
        (bios_memory_map_entry_t *)0x9000;
    uint32_t entry_count = 0;
    
    uint32_t continuation = 0;
    uint32_t buffer_size = sizeof(bios_memory_map_entry_t);
    uint32_t signature = 0x534D4150;  // 'SMAP'
    
    log_info("Querying BIOS memory map (INT 15h, EAX=E820h)...\n");
    
    do {
        uint32_t eax, ebx, ecx, edx;
        
        __asm__ volatile (
            "mov $0xE820, %%eax\n"
            "mov %5, %%ebx\n"           // Continuation value
            "mov %6, %%ecx\n"           // Buffer size
            "mov %7, %%edx\n"           // Signature 'SMAP'
            "int $0x15\n"
            "jc map_error\n"            // 进位标志表示错误
            "cmp $0x534D4150, %%edx\n"  // 检查签名
            "jne map_error\n"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "D"(bios_map + entry_count), "r"(continuation), "r"(buffer_size), "r"(signature)
            : "memory"
        );
        
        if ((eax & 0xFF) != 0) {
            break;  // 失败
        }
        
        entry_count++;
        continuation = ebx;
        
        if (continuation == 0) {
            break;  // 完成
        }
        
        if (entry_count >= 64) {
            log_warning("Memory map entry count exceeded 64\n");
            break;
        }
        
    } while (1);
    
    // 转换为HIC格式
    hic_mem_entry_t *hic_map = (hic_mem_entry_t *)0x8000;
    
    uint64_t total_memory = 0;
    uint64_t usable_memory = 0;
    
    for (uint32_t i = 0; i < entry_count; i++) {
        hic_map[i].base = bios_map[i].base;
        hic_map[i].length = bios_map[i].length;
        hic_map[i].flags = 0;
        
        // 转换类型
        switch (bios_map[i].type) {
            case 1:  // 可用内存
                hic_map[i].type = HIC_MEM_TYPE_USABLE;
                usable_memory += bios_map[i].length;
                break;
            case 2:  // 保留内存
                hic_map[i].type = HIC_MEM_TYPE_RESERVED;
                break;
            case 3:  // ACPI可回收
                hic_map[i].type = HIC_MEM_TYPE_ACPI;
                break;
            case 4:  // ACPI NVS
                hic_map[i].type = HIC_MEM_TYPE_NVS;
                break;
            case 5:  // 不可用内存
                hic_map[i].type = HIC_MEM_TYPE_UNUSABLE;
                break;
            default:
                hic_map[i].type = HIC_MEM_TYPE_RESERVED;
                break;
        }
        
        total_memory += bios_map[i].length;
        
        log_info("  [%2u] 0x%016llx - 0x%016llx (%10lu KB) type=%u\n",
                i, bios_map[i].base, 
                bios_map[i].base + bios_map[i].length - 1,
                bios_map[i].length / 1024,
                bios_map[i].type);
    }
    
    boot_info->mem_map = hic_map;
    boot_info->mem_map_entry_count = entry_count;
    boot_info->mem_map_size = entry_count * sizeof(hic_mem_entry_t);
    boot_info->mem_map_desc_size = sizeof(hic_mem_entry_t);
    
    log_info("Found %lu memory map entries\n", entry_count);
    log_info("Total memory: %lu MB, Usable: %lu MB\n",
            total_memory / (1024 * 1024), usable_memory / (1024 * 1024));
    
    return;

map_error:
    log_error("Failed to get memory map\n");
    boot_info->mem_map_entry_count = 0;
    return;
}

/**
 * 查找ACPI表
 */
static void bios_find_acpi_tables(hic_boot_info_t *boot_info)
{
    // 在EBDA区域和BIOS区域搜索RSDP
    // RSDP签名: "RSD PTR "
    
    const char *rsdp_signature = "RSD PTR ";
    
    log_info("Searching for ACPI RSDP...\n");
    
    // 1. 获取EBDA地址
    uint16_t ebda_seg;
    __asm__ volatile (
        "mov $0x40, %%ax\n"
        "mov %%ax, %%es\n"
        "movw $0x0E, %%ax\n"
        : "=a"(ebda_seg)
        :
    );
    
    uint32_t ebda_base = ebda_seg * 16;
    log_info("EBDA at 0x%08x\n", ebda_base);
    
    // 2. 搜索EBDA区域 (0x000E0000 - 0x000FFFFF)
    uint8_t *ebda_start = (uint8_t *)0x000E0000;
    uint8_t *ebda_end = (uint8_t *)0x000FFFFF;
    
    log_info("Searching EBDA region (0x%08x - 0x%08x)...\n", 
            (uint32_t)ebda_start, (uint32_t)ebda_end);
    
    for (uint8_t *p = ebda_start; p < ebda_end; p += 16) {
        if (memcmp(p, rsdp_signature, 8) == 0) {
            // 验证校验和
            uint8_t checksum = 0;
            for (int i = 0; i < 20; i++) {
                checksum += p[i];
            }
            
            if (checksum == 0) {
                boot_info->rsdp = (void *)p;
                boot_info->flags |= HIC_BOOT_FLAG_ACPI_ENABLED;
                
                // 检查是否是ACPI 2.0
                if (p[15] >= 2) {
                    boot_info->xsdp = (void *)p;
                    log_info("Found ACPI 2.0 XSDP at 0x%08x\n", (uint32_t)p);
                } else {
                    log_info("Found ACPI 1.0 RSDP at 0x%08x\n", (uint32_t)p);
                }
                return;
            }
        }
    }
    
    // 3. 搜索BIOS区域 (0x00000000 - 0x0003FFFF)
    uint8_t *bios_start = (uint8_t *)0x00000000;
    uint8_t *bios_end = (uint8_t *)0x0003FFFF;
    
    log_info("Searching BIOS region (0x%08x - 0x%08x)...\n",
            (uint32_t)bios_start, (uint32_t)bios_end);
    
    for (uint8_t *p = bios_start; p < bios_end; p += 16) {
        if (memcmp(p, rsdp_signature, 8) == 0) {
            // 验证校验和
            uint8_t checksum = 0;
            for (int i = 0; i < 20; i++) {
                checksum += p[i];
            }
            
            if (checksum == 0) {
                boot_info->rsdp = (void *)p;
                boot_info->flags |= HIC_BOOT_FLAG_ACPI_ENABLED;
                
                if (p[15] >= 2) {
                    boot_info->xsdp = (void *)p;
                    log_info("Found ACPI 2.0 XSDP at 0x%08x\n", (uint32_t)p);
                } else {
                    log_info("Found ACPI 1.0 RSDP at 0x%08x\n", (uint32_t)p);
                }
                return;
            }
        }
    }
    
    log_warning("ACPI RSDP not found\n");
    boot_info->flags &= ~HIC_BOOT_FLAG_ACPI_ENABLED;
}

/**
 * 获取系统信息
 */
static void bios_get_system_info(hic_boot_info_t *boot_info)
{
    // 使用BIOS INT 0x15获取内存大小
    
    // 方法1: EAX=0xE801 (AX=扩展内存大小, BX=扩展内存大小高位)
    uint32_t memory_size_kb = 0;
    uint32_t memory_size_kb_high = 0;
    
    __asm__ volatile (
        "mov $0xE801, %%eax\n"
        "xor %%bx, %%bx\n"
        "xor %%cx, %%cx\n"
        "xor %%dx, %%dx\n"
        "int $0x15\n"
        "jc mem_error\n"
        : "=a"(memory_size_kb), "=b"(memory_size_kb_high), "=c"(memory_size_kb), "=d"(memory_size_kb_high)
        :
        : "bx", "cx", "dx"
    );
    
    // 合并高低位
    uint64_t total_memory_kb = (uint64_t)memory_size_kb_high * 65536 + memory_size_kb;
    
    // 方法2: EAX=0xE881 (AX=配置内存大小 1MB-16MB, BX=配置内存大小 16MB-4GB)
    uint32_t config_memory_1mb_16mb = 0;
    uint32_t config_memory_16mb_4gb = 0;
    
    __asm__ volatile (
        "mov $0xE881, %%eax\n"
        "xor %%bx, %%bx\n"
        "xor %%cx, %%cx\n"
        "xor %%dx, %%dx\n"
        "int $0x15\n"
        : "=a"(config_memory_1mb_16mb), "=b"(config_memory_16mb_4gb)
        :
        : "cx", "dx"
    );
    
    // 方法3: INT 0x12 AH=0x00 (获取常规内存大小，最大640KB)
    uint16_t conventional_memory = 0;
    __asm__ volatile (
        "mov $0x00, %%ah\n"
        "int $0x12\n"
        : "=a"(conventional_memory)
    );
    
    log_info("Memory information:\n");
    log_info("  Conventional: %u KB\n", conventional_memory);
    log_info("  Extended (E801): %llu KB (%llu MB)\n", 
            total_memory_kb, total_memory_kb / 1024);
    log_info("  Config 1MB-16MB: %u KB\n", config_memory_1mb_16mb);
    log_info("  Config 16MB-4GB: %u KB\n", config_memory_16mb_4gb);
    
    // 使用E801的结果
    boot_info->system.memory_size_mb = (uint32_t)(total_memory_kb / 1024);
    
    // 检测CPU信息
    uint32_t cpu_info[4];
    
    // CPUID指令检测
    uint32_t max_cpuid;
    __asm__ volatile (
        "cpuid\n"
        : "=a"(max_cpuid)
        : "a"(0)
        : "bx", "cx", "dx"
    );
    
    if (max_cpuid >= 1) {
        __asm__ volatile (
            "cpuid\n"
            : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
            : "a"(1)
        );
        
        // 获取逻辑处理器数量
        uint8_t logical_processors = (cpu_info[1] >> 16) & 0xFF;
        boot_info->system.cpu_count = logical_processors;
        
        log_info("CPU: Logical processors=%u\n", logical_processors);
    } else {
        boot_info->system.cpu_count = 1;
        log_info("CPU: Unknown (CPUID not supported)\n");
    }
    
    // 检测长模式支持
    uint32_t extended_max_cpuid;
    __asm__ volatile (
        "cpuid\n"
        : "=a"(extended_max_cpuid)
        : "a"(0x80000000)
        : "bx", "cx", "dx"
    );
    
    if (extended_max_cpuid >= 0x80000001) {
        uint32_t extended_features;
        __asm__ volatile (
            "cpuid\n"
            : "=d"(extended_features)
            : "a"(0x80000001)
            : "bx", "cx"
        );
        
        if (extended_features & (1 << 29)) {
            log_info("CPU: Long mode supported\n");
        } else {
            log_error("CPU: Long mode NOT supported!\n");
        }
    }
    
    boot_info->system.architecture = HIC_ARCH_X86_64;
    boot_info->system.platform_type = 2;  // BIOS
    
    log_info("System: %u MB memory, %u CPU(s)\n",
            boot_info->system.memory_size_mb,
            boot_info->system.cpu_count);
    
    return;

mem_error:
    log_warning("Failed to get extended memory size, using default\n");
    boot_info->system.memory_size_mb = 1024;  // 默认1GB
    boot_info->system.cpu_count = 1;
}

/**
 * 切换到保护模式
 */
static void bios_switch_to_protected_mode(void)
{
    log_info("Switching to protected mode...\n");
    
    // 1. 加载GDT
    __asm__ volatile (
        "lgdt [gdt_ptr]\n"
        : : : "memory"
    );
    
    log_info("  GDT loaded\n");
    
    // 2. 设置保护模式启用位
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    
    log_info("  CR0 before: 0x%08x\n", cr0);
    
    cr0 |= 0x01;  // PE位 (Protection Enable)
    cr0 &= ~0x80000000;  // 清除PG位 (分页禁用)
    
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
    
    log_info("  CR0 after: 0x%08x (PE=1, PG=0)\n", cr0);
    
    // 3. 远跳转到32位代码
    __asm__ volatile (
        "ljmp $0x08, $protected_mode_entry\n"
    );
    
    // 永远不会到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * 保护模式入口
 */
__attribute__((section(".text32")))
static void protected_mode_entry(void)
{
    // 设置数据段
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "ax"
    );
    
    // 设置栈
    __asm__ volatile (
        "mov $0x8000, %%esp\n"  // 32KB栈
    );
    
    // 现在在32位保护模式中
    // 继续设置长模式
}

/**
 * 切换到长模式
 */
static void bios_switch_to_long_mode(hic_boot_info_t *boot_info)
{
    log_info("Switching to long mode...\n");
    
    // 1. 切换到保护模式
    bios_switch_to_protected_mode();
    
    // 2. 设置PAE (Physical Address Extension)
    uint32_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    
    log_info("  CR4 before: 0x%08x\n", cr4);
    
    cr4 |= 0x20;  // PAE位
    cr4 |= 0x2000;  // SMEP位（可选）
    
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
    
    log_info("  CR4 after: 0x%08x (PAE=1)\n", cr4);
    
    // 3. 设置PML4页表
    uint64_t *pml4 = (uint64_t *)0x10000;
    uint64_t *pdp = (uint64_t *)0x11000;
    uint64_t *pd = (uint64_t *)0x12000;
    
    // 清零页表
    memset(pml4, 0, 0x1000);
    memset(pdp, 0, 0x1000);
    memset(pd, 0, 0x1000);
    
    // PML4表项0指向PDP
    pml4[0] = (uint64_t)pdp | 0x03;  // Present + Writable
    
    // PDP表项0指向PD
    pdp[0] = (uint64_t)pd | 0x03;
    
    // PD表：identity mapping (1:1映射) 2MB页
    // 映射前512个2MB页（1GB）
    for (int i = 0; i < 512; i++) {
        pd[i] = (uint64_t)(i * 0x200000) | 0x83;  // 2MB页, Present + Writable + Huge
    }
    
    log_info("  Page tables configured (identity mapping 0-1GB)\n");
    
    // 4. 加载CR3
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4));
    log_info("  CR3 loaded: 0x%08x\n", (uint32_t)pml4);
    
    // 5. 设置LME (Long Mode Enable)
    uint32_t efer_low, efer_high;
    __asm__ volatile (
        "rdmsr\n"
        : "=a"(efer_low), "=d"(efer_high)
        : "c"(0xC0000080)
    );
    
    log_info("  EFER before: 0x%08x%08x\n", efer_high, efer_low);
    
    efer_low |= 0x1000;  // LME位
    
    __asm__ volatile (
        "wrmsr\n"
        : : "a"(efer_low), "d"(efer_high), "c"(0xC0000080)
    );
    
    log_info("  EFER after: 0x%08x%08x (LME=1)\n", efer_high, efer_low);
    
    // 6. 设置分页启用位
    uint32_t cr0_paged;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0_paged));
    
    log_info("  CR0 before: 0x%08x\n", cr0_paged);
    
    cr0_paged |= 0x80000000;  // PG位 (Paging Enable)
    
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0_paged));
    
    log_info("  CR0 after: 0x%08x (PG=1)\n", cr0_paged);
    
    // 7. 跳转到64位代码
    log_info("  Jumping to 64-bit mode...\n");
    
    __asm__ volatile (
        "ljmp $0x18, $long_mode_entry\n"
    );
    
    // 永远不会到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * 长模式入口
 */
__attribute__((section(".text64")))
static void long_mode_entry(void)
{
    // 设置64位段
    __asm__ volatile (
        "mov $0x20, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "ax"
    );
    
    // 设置64位栈
    __asm__ volatile (
        "mov $0x100000, %%rsp\n"  // 1MB处作为栈顶
    );
    
    // 现在在64位长模式中
    // 跳转到内核
    bios_jump_to_kernel(g_boot_info);
}

/**
 * 跳转到内核
 */
__attribute__((noreturn))
static void bios_jump_to_kernel(hic_boot_info_t *boot_info)
{
    typedef void (*kernel_entry_t)(hic_boot_info_t *);
    kernel_entry_t kernel_entry = (kernel_entry_t)boot_info->entry_point;
    
    // 设置栈
    void *stack_top = (void *)boot_info->stack_top;
    
    // 禁用中断
    __asm__ volatile ("cli");
    
    // 跳转到内核
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "mov %1, %%rdi\n"
        "jmp *%2\n"
        :
        : "r"(stack_top), "r"(boot_info), "r"(kernel_entry)
        : "memory"
    );
    
    // 永远不会到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * BIOS Panic
 */
__attribute__((noreturn))
static void bios_panic(const char *message)
{
    log_error("BIOS PANIC: %s\n", message);
    
    // 停止系统
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* GDT (Global Descriptor Table) */
__attribute__((section(".data")))
static struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr = {
    .limit = sizeof(gdt) - 1,
    .base = (uint32_t)&gdt
};

__attribute__((section(".data")))
static uint64_t gdt[] = {
    0x0000000000000000,  // NULL描述符
    0x00cf9a000000ffff,  // 64位代码段
    0x00cf92000000ffff,  // 64位数据段
    0x00affa000000ffff,  // 32位代码段
    0x00cf92000000ffff,  // 32位数据段
    0x00affa000000ffff,  // 64位代码段（长模式）
    0x00cf92000000ffff,  // 64位数据段（长模式）
};