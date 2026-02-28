/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC FAT32服务
 * 提供FAT32文件系统访问功能
 */

#include "service.h"
#include <string.h>
#include <stdlib.h>

/* 全局FAT32上下文 */
static fat32_service_ctx_t g_fat32_ctx = {0};

/* 文件句柄表 */
static fat32_file_handle_t g_file_handles[FAT32_MAX_OPEN_FILES];

/* FAT32签名 */
#define FAT32_SIGNATURE 0xAA55
#define FAT32_DIR_ENTRY_SIZE 32

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

/* 初始化FAT32服务 */
hic_status_t fat32_service_init(void) {
    /* 清零全局状态 */
    memset(&g_fat32_ctx, 0, sizeof(fat32_service_ctx_t));
    memset(g_file_handles, 0, sizeof(g_file_handles));
    
    return HIC_SUCCESS;
}

/* 启动FAT32服务 */
hic_status_t fat32_service_start(void) {
    /* FAT32服务启动成功 */
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

/* 初始化存储设备 */
hic_status_t fat32_init_device(void *device_base, uint32_t device_size) {
    fat32_bpb_t *bpb;
    fat32_ext_bpb_t *ext_bpb;
    uint16_t *signature;
    
    if (!device_base || device_size < 512) {
        return HIC_INVALID_PARAM;
    }
    
    /* 初始化上下文 */
    g_fat32_ctx.device_base = (uint8_t *)device_base;
    g_fat32_ctx.device_size = device_size;
    
    /* 解析BPB */
    bpb = (fat32_bpb_t *)g_fat32_ctx.device_base;
    
    /* 验证签名 */
    signature = (uint16_t *)(g_fat32_ctx.device_base + 510);
    if (*signature != FAT32_SIGNATURE) {
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
    
    g_fat32_ctx.data_start = g_fat32_ctx.fat_start + (bpb->num_fats * fat_size_32);
    
    return HIC_SUCCESS;
}

/* 读取FAT表项 */
static uint32_t get_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_fat32_ctx.fat_start + (fat_offset / g_fat32_ctx.bytes_per_sector);
    uint32_t entry_offset = fat_offset % g_fat32_ctx.bytes_per_sector;
    
    uint8_t *fat_sector_ptr = g_fat32_ctx.device_base + (fat_sector * g_fat32_ctx.bytes_per_sector);
    return *(uint32_t *)(fat_sector_ptr + entry_offset) & 0x0FFFFFFF;
}

/* 读取簇 */
static int read_cluster(uint32_t cluster, void *buffer) {
    uint32_t sector = g_fat32_ctx.data_start + (cluster - 2) * g_fat32_ctx.sectors_per_cluster;
    uint8_t *src = g_fat32_ctx.device_base + (sector * g_fat32_ctx.bytes_per_sector);
    memcpy(buffer, src, g_fat32_ctx.bytes_per_cluster);
    return 0;
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

/* 在目录中查找文件 */
static int find_in_dir(uint32_t dir_cluster, const char *name, 
                       uint32_t *out_cluster, uint32_t *out_size) {
    uint8_t cluster_buffer[4096];
    uint32_t cluster = dir_cluster;
    
    while (cluster < 0x0FFFFFF8) {
        read_cluster(cluster, cluster_buffer);
        
        for (uint32_t i = 0; i < g_fat32_ctx.bytes_per_cluster; i += FAT32_DIR_ENTRY_SIZE) {
            uint8_t *entry = &cluster_buffer[i];
            
            if (entry[0] == 0x00 || entry[0] == 0xE5) {
                continue;
            }
            
            if (entry[11] & 0x18) {
                continue;
            }
            
            /* 提取文件名（8.3格式） */
            char filename[12];
            int j = 0;
            for (int k = 0; k < 8 && entry[k] != ' '; k++) {
                filename[j++] = (char)entry[k];
            }
            if (entry[8] != ' ') {
                filename[j++] = '.';
                for (int k = 8; k < 11 && entry[k] != ' '; k++) {
                    filename[j++] = (char)entry[k];
                }
            }
            filename[j] = '\0';
            
            /* 比较文件名 */
            if (strcmp(filename, name) == 0) {
                uint32_t first_cluster = entry[20] | (entry[21] << 8) | 
                                         (entry[26] << 16) | (entry[27] << 24);
                uint32_t size = entry[28] | (entry[29] << 8) | 
                               (entry[30] << 16) | (entry[31] << 24);
                
                if (out_cluster) *out_cluster = first_cluster;
                if (out_size) *out_size = size;
                
                return 0;
            }
        }
        
        cluster = get_fat_entry(cluster);
    }
    
    return -1;
}

/* 读取文件 */
hic_status_t fat32_read_file(const char *path, void *buffer, uint32_t buffer_size, uint32_t *bytes_read) {
    uint32_t cluster, size, bytes_read_val;
    uint8_t cluster_buffer[4096];
    char component[256];
    const char *current_path = path;
    
    if (!path || !buffer || !bytes_read) {
        return HIC_INVALID_PARAM;
    }
    
    if (g_fat32_ctx.device_base == 0) {
        return HIC_NOT_INITIALIZED;
    }
    
    /* 从根目录开始 */
    cluster = g_fat32_ctx.root_cluster;
    
    /* 解析路径 */
    while (*current_path) {
        current_path = get_next_component(current_path, component);
        
        if (*current_path) {
            /* 还有子目录，查找目录 */
            if (find_in_dir(cluster, component, &cluster, NULL) != 0) {
                return HIC_NOT_FOUND;
            }
        } else {
            /* 最后一个组件，是文件 */
            if (find_in_dir(cluster, component, &cluster, &size) != 0) {
                return HIC_NOT_FOUND;
            }
            
            /* 读取文件内容 */
            bytes_read_val = 0;
            while (cluster < 0x0FFFFFF8 && bytes_read_val < buffer_size) {
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

/* 写入文件（简化版：暂不支持） */
hic_status_t fat32_write_file(const char *path, const void *data, uint32_t size) {
    (void)path;
    (void)data;
    (void)size;
    return HIC_NOT_IMPLEMENTED;
}

/* 列出目录 */
hic_status_t fat32_list_dir(const char *path, char *buffer, uint32_t buffer_size, uint32_t *count) {
    (void)path;
    (void)buffer;
    (void)buffer_size;
    (void)count;
    return HIC_NOT_IMPLEMENTED;
}

/* 获取文件大小 */
hic_status_t fat32_get_file_size(const char *path, uint32_t *size) {
    uint32_t cluster;
    
    if (!path || !size) {
        return HIC_INVALID_PARAM;
    }
    
    if (g_fat32_ctx.device_base == 0) {
        return HIC_NOT_INITIALIZED;
    }
    
    if (find_in_dir(g_fat32_ctx.root_cluster, path, &cluster, size) == 0) {
        return HIC_SUCCESS;
    }
    
    return HIC_NOT_FOUND;
}