/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核FAT32文件系统支持
 * 简化的FAT32实现，用于读取磁盘上的模块
 */

#ifndef FAT32_H
#define FAT32_H

#include "types.h"

/* FAT32 BPB (BIOS Parameter Block) 结构 */
typedef struct {
    u8  jmp_boot[3];
    u8  oem_name[8];
    u16 bytes_per_sector;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  num_fats;
    u16 root_entry_count;
    u16 total_sectors_16;
    u8  media;
    u16 fat_size_16;
    u16 sectors_per_track;
    u16 num_heads;
    u32 hidden_sectors;
    u32 total_sectors_32;
} fat32_bpb_t;

/* FAT32 扩展 BPB */
typedef struct {
    u32 fat_size_32;
    u16 ext_flags;
    u16 fs_ver;
    u32 root_cluster;
    u16 fs_info;
    u16 backup_boot_sector;
    u8  reserved[12];
    u8  drive_number;
    u8  reserved1;
    u8  ext_boot_signature;
    u32 volume_serial;
    u8  volume_label[11];
    u8  fs_type[8];
} fat32_ext_bpb_t;

/* FAT32 文件信息 */
typedef struct {
    char name[256];
    u32 size;
    u32 first_cluster;
    u8  is_directory;
    u8  is_hidden;
} fat32_file_info_t;

/* FAT32 上下文 */
typedef struct {
    u8  *image_base;          /* 磁盘镜像基址 */
    u32 image_size;           /* 镜像大小 */
    u32 bytes_per_sector;     /* 每扇区字节数 */
    u32 sectors_per_cluster;  /* 每簇扇区数 */
    u32 bytes_per_cluster;    /* 每簇字节数 */
    u32 fat_start;            /* FAT表起始扇区 */
    u32 data_start;           /* 数据区起始扇区 */
    u32 root_cluster;         /* 根目录起始簇 */
    u32 sectors_per_fat;      /* 每个FAT表的扇区数 */
} fat32_ctx_t;

/* 初始化FAT32 */
int fat32_init(fat32_ctx_t *ctx, void *image_base, u32 image_size);

/* 读取文件 */
int fat32_read_file(fat32_ctx_t *ctx, const char *path, void *buffer, u32 buffer_size);

/* 列出目录 */
int fat32_list_dir(fat32_ctx_t *ctx, const char *path, fat32_file_info_t *files, u32 max_files);

/* 获取文件大小 */
int fat32_get_file_size(fat32_ctx_t *ctx, const char *path, u32 *size);

#endif /* FAT32_H */