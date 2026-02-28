/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef FAT32_SERVICE_H
#define FAT32_SERVICE_H

#include <common.h>

/* FAT32服务最大支持的并发操作数 */
#define FAT32_MAX_OPEN_FILES 32
#define FAT32_BUFFER_SIZE 8192

/* FAT32上下文结构 */
typedef struct {
    uint8_t  *device_base;          /* 存储设备基地址 */
    uint32_t  device_size;          /* 存储设备大小 */
    uint8_t  bytes_per_sector;      /* 每扇区字节数 */
    uint8_t  sectors_per_cluster;   /* 每簇扇区数 */
    uint16_t bytes_per_cluster;     /* 每簇字节数 */
    uint32_t fat_start;             /* FAT表起始扇区 */
    uint32_t data_start;            /* 数据区起始扇区 */
    uint32_t root_cluster;          /* 根目录起始簇 */
} fat32_service_ctx_t;

/* 文件句柄 */
typedef struct {
    uint32_t  cluster;              /* 当前簇 */
    uint32_t  size;                 /* 文件大小 */
    uint32_t  offset;               /* 当前偏移 */
    uint8_t   active;               /* 是否活跃 */
} fat32_file_handle_t;

/* 服务接口 */
hic_status_t fat32_service_init(void);
hic_status_t fat32_service_start(void);
hic_status_t fat32_service_stop(void);
hic_status_t fat32_service_cleanup(void);

/* FAT32操作接口 */
hic_status_t fat32_init_device(void *device_base, uint32_t device_size);
hic_status_t fat32_read_file(const char *path, void *buffer, uint32_t buffer_size, uint32_t *bytes_read);
hic_status_t fat32_write_file(const char *path, const void *data, uint32_t size);
hic_status_t fat32_list_dir(const char *path, char *buffer, uint32_t buffer_size, uint32_t *count);
hic_status_t fat32_get_file_size(const char *path, uint32_t *size);

#endif /* FAT32_SERVICE_H */