/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC FAT32服务
 * 提供FAT32文件系统访问功能
 * 
 * 支持：
 * - 文件读取
 * - 文件写入
 * - 目录列表
 * - 嵌入模块加载
 */

#include "service.h"
#include <string.h>
#include <stdlib.h>

/* ==================== 服务入口点（必须在代码段最前面） ==================== */

/**
 * 服务入口点 - 必须放在代码段最前面
 * 使用 section 属性确保此函数在链接时位于 .static_svc.fat32_service.text 的开头
 */
__attribute__((section(".static_svc.fat32_service.text"), used, noinline))
int _fat32_service_entry(void)
{
    /* 初始化 FAT32 服务 */
    fat32_service_init();
    
    /* 启动服务主循环 */
    fat32_service_start();
    
    /* 服务不应返回，如果返回则进入无限循环 */
    while (1) {
        __asm__ volatile("hlt");
    }
    
    return 0;
}

/* IDE 驱动函数（来自 ide_driver 服务） */
extern int ide_read_sector(uint32_t lba, void *buffer);
extern int ide_read_sectors(uint32_t lba, uint8_t count, void *buffer);

/* Boot信息结构 */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t flags;
    struct {
        void *magic_region_base;
        uint64_t magic_region_size;
        uint32_t embedded_module_count;
    } embedded_modules;
} hic_boot_info_t;

/* 外部引用boot_info */
extern hic_boot_info_t *g_boot_info;

/* 全局FAT32上下文 */
static fat32_service_ctx_t g_fat32_ctx = {0};

/* 文件句柄表 */
static fat32_file_handle_t g_file_handles[FAT32_MAX_OPEN_FILES];

/* 扇区缓冲区（用于 IDE 驱动读取） */
static uint8_t g_sector_buffer[512];
static uint8_t g_cluster_buffer[65536];  /* 最大簇大小 128 扇区 = 64KB */

/* FAT32签名 */
#define FAT32_SIGNATURE 0xAA55
#define FAT32_DIR_ENTRY_SIZE 32

/* 目录项属性 */
#define ATTR_READ_ONLY     0x01
#define ATTR_HIDDEN        0x02
#define ATTR_SYSTEM        0x04
#define ATTR_VOLUME_ID     0x08
#define ATTR_DIRECTORY     0x10
#define ATTR_ARCHIVE       0x20
#define ATTR_LONG_NAME     0x0F

/* FAT特殊值 */
#define FAT_FREE           0x00000000
#define FAT_RESERVED       0x0FFFFFF0
#define FAT_BAD_CLUSTER    0x0FFFFFF7
#define FAT_END_OF_CHAIN   0x0FFFFFF8

/* BPB结构 */
typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed)) fat32_bpb_t;

/* 扩展BPB结构 */
typedef struct {
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_ver;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  ext_boot_signature;
    uint32_t volume_serial;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_ext_bpb_t;

/* 目录项结构 */
typedef struct {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

/* ========== 辅助函数 ========== */

static int str_len(const char *s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int str_cmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

static int str_ncmp(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static void to_upper(char *s)
{
    while (*s) {
        if (*s >= 'a' && *s <= 'z') {
            *s = *s - 'a' + 'A';
        }
        s++;
    }
}

/* ========== FAT32 核心操作 ========== */

/* 初始化FAT32服务 */
hic_status_t fat32_service_init(void) {
    /* 清零全局状态 */
    memset(&g_fat32_ctx, 0, sizeof(fat32_service_ctx_t));
    memset(g_file_handles, 0, sizeof(g_file_handles));
    
    return HIC_SUCCESS;
}

/* 启动FAT32服务 */
hic_status_t fat32_service_start(void) {
    /* 输出启动信息 */
    extern void serial_print(const char *msg);
    extern void thread_yield(void);
    
    serial_print("[FAT32] Service started\n");
    
    /* 初始化 FAT32 设备（使用 IDE 驱动） */
    serial_print("[FAT32] Initializing disk via IDE driver...\n");
    if (fat32_init_device() != HIC_SUCCESS) {
        serial_print("[FAT32] Failed to initialize disk\n");
        /* 继续运行，但磁盘不可用 */
    } else {
        serial_print("[FAT32] Disk initialized successfully\n");
    }
    
    /* 加载嵌入的文件系统驱动 */
    fat32_load_embedded_filesystem_drivers();
    
    /* 主服务循环 - 等待请求 */
    /* TODO: 实现 IPC 请求处理 */
    int count = 0;
    while (1) {
        /* 让出 CPU 给其他线程 */
        thread_yield();
        count++;
        
        /* 避免无限循环太快 */
        if (count > 1000000) {
            count = 0;
        }
    }
    
    return HIC_SUCCESS;
}

/* 停止FAT32服务 */
hic_status_t fat32_service_stop(void) {
    /* 关闭所有打开的文件句柄 */
    for (int i = 0; i < FAT32_MAX_OPEN_FILES; i++) {
        g_file_handles[i].active = 0;
    }
    
    return HIC_SUCCESS;
}

/* 清理FAT32服务 */
hic_status_t fat32_service_cleanup(void) {
    memset(&g_fat32_ctx, 0, sizeof(fat32_service_ctx_t));
    memset(g_file_handles, 0, sizeof(g_file_handles));
    
    return HIC_SUCCESS;
}

/* 初始化存储设备 - 使用 IDE 驱动 */
hic_status_t fat32_init_device(void) {
    fat32_bpb_t *bpb;
    fat32_ext_bpb_t *ext_bpb;
    uint16_t *signature;
    
    /* 使用 IDE 驱动读取第一个扇区 */
    if (ide_read_sector(0, g_sector_buffer) != 0) {
        extern void serial_print(const char *msg);
        serial_print("[FAT32] Failed to read boot sector via IDE\n");
        return HIC_ERROR;
    }
    
    /* 解析 BPB */
    bpb = (fat32_bpb_t *)g_sector_buffer;
    
    /* 验证签名 */
    signature = (uint16_t *)(g_sector_buffer + 510);
    if (*signature != FAT32_SIGNATURE) {
        extern void serial_print(const char *msg);
        serial_print("[FAT32] Invalid boot sector signature\n");
        return HIC_PARSE_FAILED;
    }
    
    /* 读取关键参数 */
    g_fat32_ctx.bytes_per_sector = bpb->bytes_per_sector;
    g_fat32_ctx.sectors_per_cluster = bpb->sectors_per_cluster;
    g_fat32_ctx.bytes_per_cluster = g_fat32_ctx.bytes_per_sector * g_fat32_ctx.sectors_per_cluster;
    
    /* 计算FAT表和数据区位置 */
    g_fat32_ctx.fat_start = bpb->reserved_sectors;
    
    uint32_t total_sectors = (bpb->total_sectors_16 != 0) ? 
                             bpb->total_sectors_16 : bpb->total_sectors_32;
    
    ext_bpb = (fat32_ext_bpb_t *)((uint8_t *)bpb + 36);
    uint32_t fat_size_32 = ext_bpb->fat_size_32;
    g_fat32_ctx.root_cluster = ext_bpb->root_cluster;
    g_fat32_ctx.total_clusters = (total_sectors - bpb->reserved_sectors - 
                                   (bpb->num_fats * fat_size_32)) / bpb->sectors_per_cluster;
    
    g_fat32_ctx.data_start = g_fat32_ctx.fat_start + (bpb->num_fats * fat_size_32);
    
    /* 初始化空闲簇链表（用于写入） */
    g_fat32_ctx.first_free_cluster = 0;
    
    /* 设置设备为已初始化状态 */
    g_fat32_ctx.device_base = (uint8_t *)1;  /* 非 NULL 表示已初始化 */
    g_fat32_ctx.device_size = total_sectors * g_fat32_ctx.bytes_per_sector;
    
    return HIC_SUCCESS;
}

/* 读取FAT表项 - 使用 IDE 驱动 */
static uint32_t get_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_fat32_ctx.fat_start + (fat_offset / g_fat32_ctx.bytes_per_sector);
    uint32_t entry_offset = fat_offset % g_fat32_ctx.bytes_per_sector;
    
    /* 使用 IDE 驱动读取 FAT 扇区 */
    if (ide_read_sector(fat_sector, g_sector_buffer) != 0) {
        return 0;  /* 返回 0 表示错误 */
    }
    
    return *(uint32_t *)(g_sector_buffer + entry_offset) & 0x0FFFFFFF;
}

/* 写入FAT表项 - 暂不支持 */
static void set_fat_entry(uint32_t cluster, uint32_t value) {
    /* 写入功能暂未实现，需要 IDE 写入支持 */
    (void)cluster;
    (void)value;
}

/* 查找空闲簇 */
static uint32_t find_free_cluster(void) {
    uint32_t cluster = g_fat32_ctx.first_free_cluster;
    if (cluster == 0) {
        cluster = 2;  /* FAT32 簇从2开始 */
    }
    
    for (uint32_t i = cluster; i < g_fat32_ctx.total_clusters + 2; i++) {
        if (get_fat_entry(i) == FAT_FREE) {
            g_fat32_ctx.first_free_cluster = i + 1;
            return i;
        }
    }
    
    /* 从头搜索 */
    for (uint32_t i = 2; i < cluster; i++) {
        if (get_fat_entry(i) == FAT_FREE) {
            g_fat32_ctx.first_free_cluster = i + 1;
            return i;
        }
    }
    
    return 0;  /* 没有空闲簇 */
}

/* 读取簇 - 使用 IDE 驱动 */
static int read_cluster(uint32_t cluster, void *buffer) {
    if (cluster < 2 || cluster >= g_fat32_ctx.total_clusters + 2) {
        return -1;
    }
    
    uint32_t sector = g_fat32_ctx.data_start + (cluster - 2) * g_fat32_ctx.sectors_per_cluster;
    
    /* 使用 IDE 驱动读取扇区 */
    if (ide_read_sectors(sector, g_fat32_ctx.sectors_per_cluster, buffer) != 0) {
        return -1;
    }
    
    return 0;
}

/* 写入簇 - 暂不支持 */
static int write_cluster(uint32_t cluster, const void *buffer) {
    /* 写入功能暂未实现，需要 IDE 写入支持 */
    (void)cluster;
    (void)buffer;
    return -1;
}

/* 路径解析 */
static const char *get_next_component(const char *path, char *component) {
    while (*path == '/') path++;
    
    int i = 0;
    while (*path && *path != '/' && i < 255) {
        component[i++] = *path++;
    }
    component[i] = '\0';
    
    while (*path == '/') path++;
    
    return path;
}

/* 转换为FAT短文件名格式 */
static void make_fat_name(const char *name, uint8_t *fat_name) {
    int i = 0;
    const char *p = name;
    
    /* 清空 */
    for (i = 0; i < 11; i++) {
        fat_name[i] = ' ';
    }
    
    /* 复制基本名称 */
    i = 0;
    while (*p && *p != '.' && i < 8) {
        char c = *p++;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        fat_name[i++] = (uint8_t)c;
    }
    
    /* 跳过点 */
    if (*p == '.') p++;
    
    /* 复制扩展名 */
    i = 8;
    while (*p && i < 11) {
        char c = *p++;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        fat_name[i++] = (uint8_t)c;
    }
}

/* 在目录中查找文件 */
static int find_in_dir(uint32_t dir_cluster, const char *name, 
                       fat32_dir_entry_t *out_entry) {
    uint8_t cluster_buffer[4096];
    uint32_t cluster = dir_cluster;
    
    /* 转换为FAT格式文件名 */
    uint8_t fat_name[11];
    make_fat_name(name, fat_name);
    
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        read_cluster(cluster, cluster_buffer);
        
        for (uint32_t i = 0; i < g_fat32_ctx.bytes_per_cluster; i += FAT32_DIR_ENTRY_SIZE) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t *)&cluster_buffer[i];
            
            /* 空条目或已删除 */
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5) {
                continue;
            }
            
            /* 跳过长文件名条目 */
            if (entry->attr == ATTR_LONG_NAME) {
                continue;
            }
            
            /* 跳过卷标和隐藏文件 */
            if (entry->attr & (ATTR_VOLUME_ID | ATTR_HIDDEN | ATTR_SYSTEM)) {
                continue;
            }
            
            /* 比较文件名 */
            int match = 1;
            for (int j = 0; j < 11; j++) {
                if (entry->name[j] != fat_name[j]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                if (out_entry) {
                    memcpy(out_entry, entry, sizeof(fat32_dir_entry_t));
                }
                return 0;
            }
        }
        
        cluster = get_fat_entry(cluster);
    }
    
    return -1;
}

/* 获取目录条目列表 */
static int list_dir_entries(uint32_t dir_cluster, fat32_dir_entry_t *entries, 
                            int max_entries, int *count) {
    uint8_t cluster_buffer[4096];
    uint32_t cluster = dir_cluster;
    int entry_count = 0;
    
    while (cluster >= 2 && cluster < 0x0FFFFFF8 && entry_count < max_entries) {
        read_cluster(cluster, cluster_buffer);
        
        for (uint32_t i = 0; i < g_fat32_ctx.bytes_per_cluster && entry_count < max_entries; 
             i += FAT32_DIR_ENTRY_SIZE) {
            fat32_dir_entry_t *entry = (fat32_dir_entry_t *)&cluster_buffer[i];
            
            /* 空条目标志目录结束 */
            if (entry->name[0] == 0x00) {
                *count = entry_count;
                return 0;
            }
            
            /* 跳过已删除条目 */
            if (entry->name[0] == 0xE5) {
                continue;
            }
            
            /* 跳过长文件名条目 */
            if (entry->attr == ATTR_LONG_NAME) {
                continue;
            }
            
            /* 跳过卷标 */
            if (entry->attr & ATTR_VOLUME_ID) {
                continue;
            }
            
            /* 复制条目 */
            memcpy(&entries[entry_count], entry, sizeof(fat32_dir_entry_t));
            entry_count++;
        }
        
        cluster = get_fat_entry(cluster);
    }
    
    *count = entry_count;
    return 0;
}

/* 从目录项获取首簇 */
static uint32_t get_first_cluster(const fat32_dir_entry_t *entry) {
    return ((uint32_t)entry->fst_clus_hi << 16) | entry->fst_clus_lo;
}

/* ========== 公开接口 ========== */

/* 读取文件 */
hic_status_t fat32_read_file(const char *path, void *buffer, uint32_t buffer_size, uint32_t *bytes_read) {
    uint32_t cluster, size, bytes_read_val;
    uint8_t cluster_buffer[4096];
    char component[256];
    const char *current_path = path;
    fat32_dir_entry_t entry;
    uint32_t dir_cluster;
    
    if (!path || !buffer || !bytes_read) {
        return HIC_INVALID_PARAM;
    }
    
    if (g_fat32_ctx.device_base == 0) {
        return HIC_NOT_INITIALIZED;
    }
    
    *bytes_read = 0;
    
    /* 从根目录开始 */
    dir_cluster = g_fat32_ctx.root_cluster;
    
    /* 解析路径 */
    while (*current_path) {
        current_path = get_next_component(current_path, component);
        
        if (component[0] == '\0') {
            continue;
        }
        
        if (*current_path) {
            /* 还有子目录，查找目录 */
            if (find_in_dir(dir_cluster, component, &entry) != 0) {
                return HIC_NOT_FOUND;
            }
            
            if (!(entry.attr & ATTR_DIRECTORY)) {
                return HIC_NOT_FOUND;
            }
            
            dir_cluster = get_first_cluster(&entry);
        } else {
            /* 最后一个组件，是文件 */
            if (find_in_dir(dir_cluster, component, &entry) != 0) {
                return HIC_NOT_FOUND;
            }
            
            cluster = get_first_cluster(&entry);
            size = entry.file_size;
            
            /* 读取文件内容 */
            bytes_read_val = 0;
            while (cluster >= 2 && cluster < 0x0FFFFFF8 && bytes_read_val < buffer_size) {
                read_cluster(cluster, cluster_buffer);
                
                uint32_t copy_size = g_fat32_ctx.bytes_per_cluster;
                if (bytes_read_val + copy_size > size) {
                    copy_size = size - bytes_read_val;
                }
                if (bytes_read_val + copy_size > buffer_size) {
                    copy_size = buffer_size - bytes_read_val;
                }
                
                memcpy((uint8_t *)buffer + bytes_read_val, cluster_buffer, copy_size);
                bytes_read_val += copy_size;
                
                cluster = get_fat_entry(cluster);
            }
            
            *bytes_read = bytes_read_val;
            return HIC_SUCCESS;
        }
    }
    
    return HIC_NOT_FOUND;
}

/* 写入文件 */
hic_status_t fat32_write_file(const char *path, const void *data, uint32_t size) {
    uint8_t cluster_buffer[4096];
    char component[256];
    const char *current_path = path;
    fat32_dir_entry_t entry;
    uint32_t dir_cluster, file_cluster;
    uint32_t bytes_written;
    int is_new_file = 0;
    
    if (!path || (!data && size > 0)) {
        return HIC_INVALID_PARAM;
    }
    
    if (g_fat32_ctx.device_base == 0) {
        return HIC_NOT_INITIALIZED;
    }
    
    /* 从根目录开始 */
    dir_cluster = g_fat32_ctx.root_cluster;
    
    /* 解析路径到父目录 */
    while (*current_path) {
        const char *next = get_next_component(current_path, component);
        
        if (*next) {
            /* 还有子目录 */
            if (find_in_dir(dir_cluster, component, &entry) != 0) {
                return HIC_NOT_FOUND;
            }
            dir_cluster = get_first_cluster(&entry);
            current_path = next;
        } else {
            /* 最后一个组件是文件名 */
            break;
        }
    }
    
    /* 检查文件是否存在 */
    if (find_in_dir(dir_cluster, component, &entry) != 0) {
        /* 文件不存在，创建新文件 */
        is_new_file = 1;
        file_cluster = find_free_cluster();
        if (file_cluster == 0) {
            return HIC_OUT_OF_MEMORY;
        }
        set_fat_entry(file_cluster, FAT_END_OF_CHAIN);
    } else {
        file_cluster = get_first_cluster(&entry);
    }
    
    /* 写入数据 */
    bytes_written = 0;
    
    while (bytes_written < size) {
        /* 清空簇缓冲区 */
        memset(cluster_buffer, 0, sizeof(cluster_buffer));
        
        /* 计算本次写入量 */
        uint32_t write_size = g_fat32_ctx.bytes_per_cluster;
        if (bytes_written + write_size > size) {
            write_size = size - bytes_written;
        }
        
        /* 复制数据 */
        memcpy(cluster_buffer, (const uint8_t *)data + bytes_written, write_size);
        
        /* 写入簇 */
        if (write_cluster(file_cluster, cluster_buffer) != 0) {
            return HIC_ERROR;
        }
        
        bytes_written += write_size;
        
        /* 如果还有数据，分配下一个簇 */
        if (bytes_written < size) {
            uint32_t next_cluster = get_fat_entry(file_cluster);
            if (next_cluster >= 0x0FFFFFF8) {
                next_cluster = find_free_cluster();
                if (next_cluster == 0) {
                    return HIC_OUT_OF_MEMORY;
                }
                set_fat_entry(file_cluster, next_cluster);
                set_fat_entry(next_cluster, FAT_END_OF_CHAIN);
            }
            file_cluster = next_cluster;
        }
    }
    
    /* 如果是新文件，需要创建目录项 */
    if (is_new_file) {
        /* 简化：假设目录有空间 */
        /* 完整实现需要在目录中找到空闲条目并写入 */
    }
    
    return HIC_SUCCESS;
}

/* 列出目录 */
hic_status_t fat32_list_dir(const char *path, char *buffer, uint32_t buffer_size, uint32_t *count) {
    char component[256];
    const char *current_path = path;
    fat32_dir_entry_t entries[64];
    int entry_count = 0;
    uint32_t dir_cluster;
    
    if (!buffer || !count) {
        return HIC_INVALID_PARAM;
    }
    
    if (g_fat32_ctx.device_base == 0) {
        return HIC_NOT_INITIALIZED;
    }
    
    *count = 0;
    
    /* 从根目录开始 */
    dir_cluster = g_fat32_ctx.root_cluster;
    
    /* 解析路径 */
    while (*current_path) {
        current_path = get_next_component(current_path, component);
        
        if (component[0] == '\0') {
            continue;
        }
        
        fat32_dir_entry_t entry;
        if (find_in_dir(dir_cluster, component, &entry) != 0) {
            return HIC_NOT_FOUND;
        }
        
        if (!(entry.attr & ATTR_DIRECTORY)) {
            return HIC_NOT_FOUND;
        }
        
        dir_cluster = get_first_cluster(&entry);
    }
    
    /* 获取目录条目 */
    if (list_dir_entries(dir_cluster, entries, 64, &entry_count) != 0) {
        return HIC_ERROR;
    }
    
    /* 格式化输出 */
    uint32_t offset = 0;
    for (int i = 0; i < entry_count && offset < buffer_size - 32; i++) {
        fat32_dir_entry_t *e = &entries[i];
        
        /* 提取文件名 */
        char filename[13];
        int j = 0;
        for (int k = 0; k < 8 && e->name[k] != ' '; k++) {
            filename[j++] = (char)e->name[k];
        }
        if (e->name[8] != ' ') {
            filename[j++] = '.';
            for (int k = 8; k < 11 && e->name[k] != ' '; k++) {
                filename[j++] = (char)e->name[k];
            }
        }
        filename[j] = '\0';
        
        /* 写入缓冲区 */
        uint32_t len = (uint32_t)str_len(filename);
        if (offset + len + 2 < buffer_size) {
            for (uint32_t k = 0; k < len; k++) {
                buffer[offset++] = filename[k];
            }
            buffer[offset++] = '\n';
        }
    }
    
    buffer[offset] = '\0';
    *count = (uint32_t)entry_count;
    
    return HIC_SUCCESS;
}

/* 获取文件大小 */
hic_status_t fat32_get_file_size(const char *path, uint32_t *size) {
    char component[256];
    const char *current_path = path;
    fat32_dir_entry_t entry;
    uint32_t dir_cluster;
    
    if (!path || !size) {
        return HIC_INVALID_PARAM;
    }
    
    if (g_fat32_ctx.device_base == 0) {
        return HIC_NOT_INITIALIZED;
    }
    
    /* 从根目录开始 */
    dir_cluster = g_fat32_ctx.root_cluster;
    
    /* 解析路径 */
    while (*current_path) {
        current_path = get_next_component(current_path, component);
        
        if (component[0] == '\0') {
            continue;
        }
        
        if (*current_path) {
            if (find_in_dir(dir_cluster, component, &entry) != 0) {
                return HIC_NOT_FOUND;
            }
            dir_cluster = get_first_cluster(&entry);
        } else {
            if (find_in_dir(dir_cluster, component, &entry) != 0) {
                return HIC_NOT_FOUND;
            }
            *size = entry.file_size;
            return HIC_SUCCESS;
        }
    }
    
    return HIC_NOT_FOUND;
}

/* HIC模块魔数和驱动类型 */
#define HICMOD_MAGIC 0x48494B4D  /* "HICM" */
#define HICMOD_DRIVER_TYPE_FILESYSTEM 0x46535953  /* "FSYS" */

/* 加载嵌入的文件系统驱动 */
hic_status_t fat32_load_embedded_filesystem_drivers(void) {
    extern hic_boot_info_t *g_boot_info;
    
    if (!g_boot_info) {
        return HIC_NOT_INITIALIZED;
    }
    
    void *magic_region = g_boot_info->embedded_modules.magic_region_base;
    uint32_t region_size = (uint32_t)g_boot_info->embedded_modules.magic_region_size;
    
    if (!magic_region || region_size == 0) {
        return HIC_SUCCESS;  /* 没有嵌入模块，不是错误 */
    }
    
    uint8_t *region_ptr = (uint8_t *)magic_region;
    uint32_t offset = 0;
    int loaded_count = 0;
    
    /* 遍历嵌入模块区域 */
    while (offset + 52 < region_size) {
        uint32_t magic = *(uint32_t *)(region_ptr + offset);
        uint32_t driver_type = *(uint32_t *)(region_ptr + offset + 4);
        
        if (magic != HICMOD_MAGIC) {
            break;
        }
        
        if (driver_type == HICMOD_DRIVER_TYPE_FILESYSTEM) {
            /* 找到文件系统驱动 */
            /* 获取驱动入口点 */
            uint32_t code_offset = *(uint32_t *)(region_ptr + offset + 44);
            uint64_t entry_point = (uint64_t)(region_ptr + offset + code_offset);
            
            /* 初始化驱动 */
            typedef void (*driver_init_t)(void);
            driver_init_t driver_init = (driver_init_t)entry_point;
            
            if (driver_init) {
                driver_init();
                loaded_count++;
            }
        }
        
        /* 读取模块大小以跳到下一个模块 */
        uint32_t code_size = *(uint32_t *)(region_ptr + offset + 36);
        uint32_t data_size = *(uint32_t *)(region_ptr + offset + 40);
        uint32_t header_size = *(uint32_t *)(region_ptr + offset + 48);
        
        /* 跳到下一个模块 */
        uint32_t total_size = header_size + code_size + data_size;
        offset += total_size;
        offset = (offset + 3) & ~3U;  /* 对齐到4字节 */
    }
    
    if (loaded_count == 0) {
        return HIC_NOT_FOUND;
    }
    
    return HIC_SUCCESS;
}
