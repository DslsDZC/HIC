#ifndef HIC_BOOTLOADER_KERNEL_IMAGE_H
#define HIC_BOOTLOADER_KERNEL_IMAGE_H

#include <stdint.h>
#include "boot_info.h"

// HIC内核映像魔数
#define HIC_IMG_MAGIC  "HIC_IMG"

// 架构ID
#define HIC_ARCH_X86_64   1
#define HIC_ARCH_ARM64    2
#define HIC_ARCH_RISCV64  3

// HIC内核映像头
typedef struct {
    char     magic[8];              // "HIC_IMG"
    uint16_t arch_id;               // 架构ID
    uint16_t version;               // 版本号 (主版本 << 8 | 次版本)
    uint64_t entry_point;           // 入口点偏移
    uint64_t image_size;            // 映像总大小
    uint64_t segment_table_offset;  // 段表偏移
    uint64_t segment_count;         // 段表项数
    uint64_t config_table_offset;   // 配置表偏移
    uint64_t config_table_size;     // 配置表大小
    uint64_t signature_offset;      // 签名偏移
    uint64_t signature_size;        // 签名大小
    uint8_t  reserved[64];          // 预留
} hic_image_header_t;

// 段类型
#define HIC_SEGMENT_TYPE_CODE    1
#define HIC_SEGMENT_TYPE_DATA    2
#define HIC_SEGMENT_TYPE_RODATA  3
#define HIC_SEGMENT_TYPE_BSS     4
#define HIC_SEGMENT_TYPE_CONFIG  5

// 段标志
#define HIC_SEGMENT_FLAG_READABLE  (1 << 0)
#define HIC_SEGMENT_FLAG_WRITABLE  (1 << 1)
#define HIC_SEGMENT_FLAG_EXECUTABLE (1 << 2)

// 段表项
typedef struct {
    uint32_t type;                 // 段类型
    uint32_t flags;                // 段标志
    uint64_t file_offset;          // 文件中的偏移
    uint64_t memory_offset;        // 内存中的偏移
    uint64_t file_size;            // 文件大小
    uint64_t memory_size;          // 内存大小
    uint64_t alignment;            // 对齐要求
} hic_segment_entry_t;

// 签名算法
#define HIC_SIG_ALGO_RSA_3072_SHA384  1
#define HIC_SIG_ALGO_ED25519_SHA512   2

// 签名结构
typedef struct {
    uint32_t algorithm;             // 签名算法
    uint32_t signature_size;        // 签名数据大小
    uint8_t  signature_data[];      // 签名数据（变长）
} hic_signature_t;

// 公钥结构
typedef struct {
    uint32_t algorithm;             // 公钥算法
    uint32_t key_size;              // 密钥大小
    uint8_t  key_data[];            // 密钥数据（变长）
} hic_public_key_t;

// 配置表条目类型
#define HIC_CONFIG_TYPE_MEMORY_LAYOUT    1
#define HIC_CONFIG_TYPE_INTERRUPT_ROUTE  2
#define HIC_CONFIG_TYPE_CAPABILITY_INIT  3
#define HIC_CONFIG_TYPE_DEVICE_INIT_SEQ  4

// 配置表条目
typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t  data[];                 // 配置数据（变长）
} hic_config_entry_t;

// 内存布局配置
typedef struct {
    uint64_t core0_base;
    uint64_t core0_size;
    uint64_t privileged1_base;
    uint64_t privileged1_size;
    uint64_t shared_mem_base;
    uint64_t shared_mem_size;
} hic_config_memory_layout_t;

// 验证结果
typedef enum {
    HIC_VERIFY_SUCCESS = 0,
    HIC_VERIFY_INVALID_MAGIC,
    HIC_VERIFY_WRONG_ARCH,
    HIC_VERIFY_INVALID_SIGNATURE,
    HIC_VERIFY_LOAD_ERROR,
    HIC_VERIFY_UNSUPPORTED_VERSION
} hic_verify_result_t;

// HIC内核映像加载器
typedef struct {
    hic_image_header_t *header;
    void               *image_data;
    uint64_t            image_size;
    void               *loaded_base;
    hic_boot_info_t    *boot_info;
} hic_image_loader_t;

// 函数声明
hic_verify_result_t hic_image_verify(void *image_data, uint64_t size);
hic_verify_result_t hic_image_load(hic_image_loader_t *loader, void *target_addr);
uint64_t hic_image_get_entry_point(hic_image_loader_t *loader);
void *hic_image_get_config(hic_image_loader_t *loader, uint64_t *size);

#endif // HIC_BOOTLOADER_KERNEL_IMAGE_H