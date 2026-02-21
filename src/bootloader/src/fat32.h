/**
 * FAT32文件系统解析器
 * 
 * 自定义FAT32文件系统读取器，用于绕过UEFI FAT32驱动的bug
 * 直接使用EFI_BLOCK_IO_PROTOCOL读取扇区，解析FAT32元数据
 */

#ifndef HIC_BOOTLOADER_FAT32_H
#define HIC_BOOTLOADER_FAT32_H

#include <efi.h>
#include <stdint.h>

#ifndef EFIAPI
#define EFIAPI
#endif

#ifndef EFI_LBA
typedef UINT64 EFI_LBA;
#endif

/* Forward declaration of EFI_BLOCK_IO_PROTOCOL */
typedef struct _EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;

/* EFI Block I/O Protocol GUID */
#define EFI_BLOCK_IO_PROTOCOL_GUID \
    {0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

extern EFI_GUID gEfiBlockIoProtocolGuid;

/* Basic structure for EFI_BLOCK_IO_PROTOCOL (minimal) */
struct _EFI_BLOCK_IO_PROTOCOL {
    UINT64 Revision;
    // Media structure
    struct {
        UINT32 MediaId;
        BOOLEAN RemovableMedia;
        BOOLEAN MediaPresent;
        BOOLEAN LogicalPartition;
        BOOLEAN ReadOnly;
        BOOLEAN WriteCaching;
        UINT32 BlockSize;
        UINT32 IoAlign;
        EFI_LBA LastBlock;
    } Media;
    
    // Function pointers
    EFI_STATUS (*Reset)(struct _EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN ExtendedVerification);
    EFI_STATUS (*ReadBlocks)(struct _EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA LBA, UINTN BufferSize, VOID *Buffer);
    EFI_STATUS (*WriteBlocks)(struct _EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA LBA, UINTN BufferSize, VOID *Buffer);
    EFI_STATUS (*FlushBlocks)(struct _EFI_BLOCK_IO_PROTOCOL *This);
};

/* FAT32 BPB (BIOS Parameter Block) 结构 */
typedef struct {
    uint8_t  jmp_boot[3];        // 跳转指令
    uint8_t  oem_name[8];        // OEM名称
    uint16_t bytes_per_sector;   // 每扇区字节数 (通常512)
    uint8_t  sectors_per_cluster;// 每簇扇区数
    uint16_t reserved_sectors;   // 保留扇区数
    uint8_t  num_fats;           // FAT表数量 (通常2)
    uint16_t root_entry_count;   // 根目录项数 (FAT32为0)
    uint16_t total_sectors_16;   // 总扇区数 (16位，FAT32为0)
    uint8_t  media_descriptor;   // 媒体描述符
    uint16_t sectors_per_fat_16; // 每FAT扇区数 (FAT32为0)
    uint16_t sectors_per_track;  // 每磁道扇区数
    uint16_t num_heads;          // 磁头数
    uint32_t hidden_sectors;     // 隐藏扇区数
    uint32_t total_sectors_32;   // 总扇区数 (32位)
    uint32_t sectors_per_fat_32; // 每FAT扇区数 (FAT32)
    uint16_t extended_flags;     // 扩展标志
    uint16_t fs_version;         // 文件系统版本
    uint32_t root_cluster;       // 根目录起始簇号
    uint16_t fsinfo_sector;      // FSINFO扇区号
    uint16_t backup_boot_sector; // 备份引导扇区
    uint8_t  reserved[12];       // 保留
    uint8_t  drive_number;       // 驱动器号
    uint8_t  reserved1;          // 保留
    uint8_t  ext_boot_signature; // 扩展引导签名 (0x29)
    uint32_t volume_serial;      // 卷序列号
    uint8_t  volume_label[11];   // 卷标
    uint8_t  fs_type[8];         // 文件系统类型
    uint8_t  boot_code[420];     // 引导代码
    uint16_t signature;          // 签名 (0xAA55)
} __attribute__((packed)) fat32_bpb_t;

/* FAT32目录项结构 */
typedef struct {
    uint8_t  name[8];            // 短文件名
    uint8_t  ext[3];             // 扩展名
    uint8_t  attributes;         // 属性
    uint8_t  reserved;           // NT保留
    uint8_t  create_time_tenth;  // 创建时间(1/10秒)
    uint16_t create_time;        // 创建时间
    uint16_t create_date;        // 创建日期
    uint16_t last_access_date;   // 最后访问日期
    uint16_t first_cluster_high; // 起始簇号高16位
    uint16_t write_time;         // 写入时间
    uint16_t write_date;         // 写入日期
    uint16_t first_cluster_low;  // 起始簇号低16位
    uint32_t file_size;          // 文件大小
} __attribute__((packed)) fat32_dentry_t;

/* 长文件名目录项结构 */
typedef struct {
    uint8_t  sequence;           // 序列号
    uint16_t name1[5];           // 文件名第1部分 (UTF-16)
    uint8_t  attributes;         // 属性 (必须是0x0F)
    uint8_t  reserved;           // 保留
    uint8_t  checksum;           // 校验和
    uint16_t name2[6];           // 文件名第2部分 (UTF-16)
    uint16_t first_cluster;      // 起始簇号 (总是0)
    uint16_t name3[2];           // 文件名第3部分 (UTF-16)
} __attribute__((packed)) fat32_lfn_t;

/* FAT32文件系统结构 */
typedef struct {
    EFI_BLOCK_IO_PROTOCOL *block_io;  // 块设备协议
    fat32_bpb_t bpb;                   // BPB
    uint32_t root_dir_start;          // 根目录起始扇区
    uint32_t data_area_start;         // 数据区起始扇区
    uint32_t fat_start;               // FAT表起始扇区
    uint32_t bytes_per_cluster;       // 每簇字节数
    uint32_t root_cluster;            // 根目录簇号
    uint32_t total_clusters;          // 总簇数
    uint32_t current_file_start;      // 当前文件起始簇
    uint32_t current_file_size;       // 当前文件大小
    uint8_t sector_buffer[512];       // 扇区缓冲区
    
    // BPB解析出的参数
    uint32_t bytes_per_sector;        // 每扇区字节数
    uint32_t sectors_per_cluster;     // 每簇扇区数
    uint32_t reserved_sectors;        // 保留扇区数
    uint32_t num_fats;                // FAT表数量
    uint32_t sectors_per_fat;         // 每个FAT的扇区数
    uint32_t fat_start_lba;           // FAT表起始LBA
    uint32_t data_start_lba;          // 数据区起始LBA
    uint32_t cluster_size;            // 簇大小（字节）
    
    // 状态标志
    BOOLEAN initialized;              // 是否已初始化
    EFI_STATUS status;                // 初始化状态
} fat32_fs_t;

/* FAT32文件属性 */
#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LONG_NAME  0x0F
#define FAT32_ATTR_MASK       0x3F

/* 函数声明 */
EFI_STATUS fat32_init(fat32_fs_t *fs, EFI_BLOCK_IO_PROTOCOL *block_io);
BOOLEAN fat32_is_valid(fat32_fs_t *fs);
EFI_STATUS fat32_open_file(fat32_fs_t *fs, const char *path, uint64_t *file_size);
EFI_STATUS fat32_read_file(fat32_fs_t *fs, const char *path, void **buffer, uint64_t *size);
void fat32_free_buffer(void *buffer);

#endif /* HIC_BOOTLOADER_FAT32_H */
