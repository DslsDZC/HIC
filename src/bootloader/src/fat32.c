/**
 * FAT32文件系统解析器实现
 * 
 * 直接使用EFI_BLOCK_IO_PROTOCOL读取扇区，解析FAT32元数据
 * 绕过UEFI FAT32驱动的bug
 */

#include <efi.h>
#include <string.h>
#include "fat32.h"
#include "console.h"

/* 全局boot services指针 */
extern EFI_BOOT_SERVICES *gBS;

/* FAT32特殊值 */
#define FAT32_END_OF_CHAIN  0x0FFFFFF8  // 簇链结束标记
#define FAT32_BAD_CLUSTER   0x0FFFFFF7  // 坏簇标记
#define FAT32_FREE_CLUSTER  0x00000000  // 空闲簇

/**
 * 读取单个扇区
 */
static EFI_STATUS read_sector(fat32_fs_t *fs, uint32_t lba, uint8_t *buffer)
{
    EFI_STATUS status;
    EFI_BLOCK_IO_PROTOCOL *block_io = fs->block_io;
    
    if (!block_io || !buffer) {
        return EFI_INVALID_PARAMETER;
    }
    
    // 使用UEFI标准接口读取
    status = block_io->ReadBlocks(block_io,
                                  block_io->Media.MediaId,
                                  lba,
                                  512,  // BufferSize
                                  (VOID*)buffer);
    
    return status;
}

/**
 * 将短文件名转换为8.3格式
 */
static void convert_short_name(const uint8_t *name, const uint8_t *ext, char *out)
{
    int i;
    
    // 复制名称部分（去除空格）
    int name_len = 8;
    while (name_len > 0 && name[name_len - 1] == ' ') {
        name_len--;
    }
    
    for (i = 0; i < name_len; i++) {
        out[i] = (char)name[i];
    }
    
    // 复制扩展名
    int ext_len = 3;
    while (ext_len > 0 && ext[ext_len - 1] == ' ') {
        ext_len--;
    }
    
    if (ext_len > 0) {
        out[name_len] = '.';
        for (i = 0; i < ext_len; i++) {
            out[name_len + 1 + i] = (char)ext[i];
        }
        out[name_len + 1 + ext_len] = '\0';
    } else {
        out[name_len] = '\0';
    }
}

/**
 * 解析BPB
 */
static EFI_STATUS parse_bpb(fat32_fs_t *fs)
{
    fat32_bpb_t *bpb = (fat32_bpb_t *)fs->sector_buffer;
    
    // 检查引导签名
    if (bpb->signature != 0xAA55) {
        console_puts("[FAT32] Invalid boot signature\n");
        return EFI_DEVICE_ERROR;
    }
    
    // 读取关键参数
    fs->bytes_per_sector = bpb->bytes_per_sector;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->reserved_sectors = bpb->reserved_sectors;
    fs->num_fats = bpb->num_fats;
    fs->sectors_per_fat = bpb->sectors_per_fat_32;
    fs->root_cluster = bpb->root_cluster;
    
    // 计算衍生值
    fs->fat_start_lba = fs->reserved_sectors;
    fs->data_start_lba = fs->fat_start_lba + (fs->num_fats * fs->sectors_per_fat);
    fs->cluster_size = fs->bytes_per_sector * fs->sectors_per_cluster;
    
    // 计算总簇数
    uint32_t total_sectors = bpb->total_sectors_16;
    if (total_sectors == 0) {
        total_sectors = bpb->total_sectors_32;
    }
    
    uint32_t data_sectors = total_sectors - fs->data_start_lba;
    fs->total_clusters = data_sectors / fs->sectors_per_cluster;
    
    console_printf("[FAT32] BPB parsed: bytes_per_sector=%d, sectors_per_cluster=%d\n",
                 fs->bytes_per_sector, fs->sectors_per_cluster);
    console_printf("[FAT32] root_cluster=%d, total_clusters=%d\n",
                 fs->root_cluster, fs->total_clusters);
    
    return EFI_SUCCESS;
}

/**
 * 初始化FAT32文件系统
 */
__attribute__((unused)) EFI_STATUS fat32_init(fat32_fs_t *fs, EFI_BLOCK_IO_PROTOCOL *block_io)
{
    console_puts("[FAT32] init: start\n");
    
    if (!fs || !block_io) {
        console_puts("[FAT32] init: invalid parameters\n");
        return EFI_INVALID_PARAMETER;
    }
    
    console_puts("[FAT32] init: memset start\n");
    
    // 初始化上下文 - 使用简单的循环代替 memset
    uint8_t *ptr = (uint8_t *)fs;
    for (size_t i = 0; i < sizeof(fat32_fs_t); i++) {
        ptr[i] = 0;
    }
    
    console_puts("[FAT32] init: memset done\n");
    
    fs->block_io = block_io;
    
    console_puts("[FAT32] init: reading boot sector\n");
    
    // 读取引导扇区（LBA 0）
    EFI_STATUS status = read_sector(fs, 0, fs->sector_buffer);
    if (EFI_ERROR(status)) {
        console_puts("[FAT32] Failed to read boot sector\n");
        return status;
    }
    
    console_puts("[FAT32] init: boot sector read\n");
    
    // 解析BPB
    status = parse_bpb(fs);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // 验证是FAT32
    if (fs->sectors_per_fat == 0) {
        console_puts("[FAT32] Not a FAT32 filesystem\n");
        return EFI_UNSUPPORTED;
    }
    
    fs->initialized = 1;
    fs->status = EFI_SUCCESS;
    
    console_puts("[FAT32] FAT32 filesystem initialized\n");
    return EFI_SUCCESS;
}

/**
 * 检查FAT32文件系统是否有效
 */
__attribute__((unused)) BOOLEAN fat32_is_valid(fat32_fs_t *fs)
{
    return (fs && fs->initialized && fs->status == EFI_SUCCESS);
}

/**
 * 簇号转换为LBA
 */
static uint32_t cluster_to_lba(fat32_fs_t *fs, uint32_t cluster)
{
    return fs->data_start_lba + ((cluster - 2) * fs->sectors_per_cluster);
}

/**
 * 读取FAT表项
 */
static EFI_STATUS read_fat_entry(fat32_fs_t *fs, uint32_t cluster, uint32_t *next_cluster)
{
    if (cluster >= fs->total_clusters + 2) {
        return EFI_INVALID_PARAMETER;
    }
    
    // 计算FAT表项位置
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs->bytes_per_sector;
    
    // 读取FAT扇区
    EFI_STATUS status = read_sector(fs, fat_sector, fs->sector_buffer);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // 读取FAT表项（小端序）
    *next_cluster = *(uint32_t *)(fs->sector_buffer + entry_offset) & 0x0FFFFFFF;
    
    return EFI_SUCCESS;
}

/**
 * 在目录中查找文件
 */
static EFI_STATUS find_file_in_directory(fat32_fs_t *fs, uint32_t dir_cluster, 
                                         const char *target_name, uint32_t *out_cluster, 
                                         uint32_t *out_size)
{
    fat32_dentry_t *dentry;
    char short_name[13];
    uint32_t cluster = dir_cluster;
    EFI_STATUS status;
    
    // 遍历目录的所有簇
    do {
        uint32_t lba = cluster_to_lba(fs, cluster);
        
        // 读取簇的所有扇区
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            status = read_sector(fs, lba + sector, fs->sector_buffer);
            if (EFI_ERROR(status)) {
                return status;
            }
            
            // 遍历目录项（每扇区16个目录项）
            dentry = (fat32_dentry_t *)fs->sector_buffer;
            for (int i = 0; i < 16; i++) {
                // 检查是否是空项
                if (dentry[i].name[0] == 0x00) {
                    // 目录结束
                    return EFI_NOT_FOUND;
                }
                
                // 跳过已删除的项和长文件名项
                if (dentry[i].name[0] == 0xE5 || 
                    (dentry[i].attributes & FAT32_ATTR_LONG_NAME)) {
                    continue;
                }
                
                // 转换短文件名
                convert_short_name(dentry[i].name, dentry[i].ext, short_name);
                
                // 比较文件名（不区分大小写）
                if (strcmp(short_name, target_name) == 0) {
                    *out_cluster = (dentry[i].first_cluster_high << 16) | dentry[i].first_cluster_low;
                    *out_size = dentry[i].file_size;
                    return EFI_SUCCESS;
                }
            }
        }
        
        // 读取下一个簇
        status = read_fat_entry(fs, cluster, &cluster);
        if (EFI_ERROR(status)) {
            return status;
        }
        
    } while (cluster < FAT32_END_OF_CHAIN);
    
    return EFI_NOT_FOUND;
}

/**
 * 路径分解（支持多级目录）
 */
static EFI_STATUS navigate_path(fat32_fs_t *fs, const char *path, uint32_t *cluster, uint32_t *size)
{
    // 从根目录开始
    uint32_t current_cluster = fs->root_cluster;
    uint32_t current_size = 0;
    
    // 跳过开头的反斜杠
    const char *start = path;
    while (*start == '\\') {
        start++;
    }
    
    // 复制路径
    char path_copy[256];
    int i;
    for (i = 0; (UINTN)i < sizeof(path_copy) - 1 && start[i] != '\0'; i++) {
        path_copy[i] = start[i];
    }
    path_copy[i] = '\0';
    
    // 简单路径分解：只支持单级目录
    // 查找最后一个反斜杠
    char *last_sep = NULL;
    for (i = 0; path_copy[i] != '\0'; i++) {
        if (path_copy[i] == '\\') {
            path_copy[i] = '\0';
            last_sep = &path_copy[i + 1];
        }
    }
    
    if (last_sep != NULL && path_copy[0] != '\0') {
        // 两级路径：先处理目录，再处理文件
        EFI_STATUS status = find_file_in_directory(fs, current_cluster, path_copy, 
                                                   &current_cluster, &current_size);
        if (EFI_ERROR(status)) {
            return status;
        }
        status = find_file_in_directory(fs, current_cluster, last_sep, 
                                        &current_cluster, size);
        return status;
    } else {
        // 单级路径：直接查找
        return find_file_in_directory(fs, current_cluster, path_copy, cluster, size);
    }
}

/**
 * 读取文件内容
 */
static EFI_STATUS read_file_content(fat32_fs_t *fs, uint32_t first_cluster, 
                                    uint32_t file_size, void *buffer)
{
    uint8_t *out = (uint8_t *)buffer;
    uint32_t bytes_read = 0;
    uint32_t cluster = first_cluster;
    
    // 遍历簇链
    while (cluster < FAT32_END_OF_CHAIN && bytes_read < file_size) {
        uint32_t lba = cluster_to_lba(fs, cluster);
        uint32_t bytes_to_read = fs->cluster_size;
        
        // 确保不超过文件大小
        if (bytes_read + bytes_to_read > file_size) {
            bytes_to_read = file_size - bytes_read;
        }
        
        // 读取簇的所有扇区
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            uint32_t sector_bytes = fs->bytes_per_sector;
            
            // 最后一个扇区可能不需要全部
            if (bytes_read + sector_bytes > file_size) {
                sector_bytes = file_size - bytes_read;
            }
            
            EFI_STATUS status = read_sector(fs, lba + sector, fs->sector_buffer);
            if (EFI_ERROR(status)) {
                return status;
            }
            
            // 复制数据
            memcpy(out, fs->sector_buffer, sector_bytes);
            out += sector_bytes;
            bytes_read += sector_bytes;
            
            if (bytes_read >= file_size) {
                break;
            }
        }
        
        // 读取下一个簇
        EFI_STATUS status = read_fat_entry(fs, cluster, &cluster);
        if (EFI_ERROR(status)) {
            return status;
        }
    }
    
    return EFI_SUCCESS;
}

/**
 * 读取文件
 */
/**
 * 打开文件（仅获取文件信息，不读取内容）
 */
__attribute__((unused)) EFI_STATUS fat32_open_file(fat32_fs_t *fs, const char *path, uint64_t *file_size)
{
    if (!fs || !fs->initialized || !path || !file_size) {
        return EFI_INVALID_PARAMETER;
    }
    
    console_printf("[FAT32] Opening file: %s\n", path);
    
    // 导航到文件
    uint32_t first_cluster;
    uint32_t size;
    EFI_STATUS status = navigate_path(fs, path, &first_cluster, &size);
    if (EFI_ERROR(status)) {
        console_printf("[FAT32] File not found: %s\n", path);
        return status;
    }
    
    *file_size = size;
    console_printf("[FAT32] File opened: size=%d bytes\n", (int)size);
    
    return EFI_SUCCESS;
}

/**
 * 读取文件内容
 */
__attribute__((unused)) EFI_STATUS fat32_read_file(fat32_fs_t *fs, const char *path, void **buffer, uint64_t *size)
{
    if (!fs || !fs->initialized || !path || !buffer || !size || !gBS) {
        return EFI_INVALID_PARAMETER;
    }
    
    console_printf("[FAT32] Reading file: %s\n", path);
    
    // 导航到文件
    uint32_t first_cluster;
    uint32_t file_size;
    EFI_STATUS status = navigate_path(fs, path, &first_cluster, &file_size);
    if (EFI_ERROR(status)) {
        console_printf("[FAT32] File not found: %s\n", path);
        return status;
    }
    
    console_printf("[FAT32] File found: cluster=%d, size=%d bytes\n", 
                 first_cluster, file_size);
    
    // 分配缓冲区
    EFI_STATUS alloc_status = gBS->AllocatePool(EfiLoaderData, file_size, (void **)buffer);
    if (EFI_ERROR(alloc_status) || !*buffer) {
        console_puts("[FAT32] Failed to allocate buffer\n");
        return EFI_OUT_OF_RESOURCES;
    }
    
    // 读取文件内容
    status = read_file_content(fs, first_cluster, file_size, *buffer);
    if (EFI_ERROR(status)) {
        gBS->FreePool(*buffer);
        *buffer = NULL;
        console_puts("[FAT32] Failed to read file content\n");
        return status;
    }
    
    *size = file_size;
    console_puts("[FAT32] File read successfully\n");
    
    return EFI_SUCCESS;
}

/**
 * 释放缓冲区
 */
__attribute__((unused)) void fat32_free_buffer(void *buffer)
{
    if (buffer && gBS) {
        gBS->FreePool(buffer);
    }
}
