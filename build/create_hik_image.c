#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define HIK_IMG_MAGIC "HIK_IMG"
#define HIK_ARCH_X86_64 1

typedef struct {
    char     magic[8];              // "HIK_IMG"
    uint16_t arch_id;
    uint16_t version;
    uint64_t entry_point;
    uint64_t image_size;
    uint64_t segment_table_offset;
    uint64_t segment_count;
    uint64_t config_table_offset;
    uint64_t config_table_size;
    uint8_t  reserved[32];
} __attribute__((packed)) hik_image_header_t;

typedef struct {
    uint64_t virt_addr;
    uint64_t phys_addr;
    uint64_t file_size;
    uint64_t mem_size;
    uint64_t flags;
} __attribute__((packed)) hik_segment_t;

typedef struct {
    uint64_t key_offset;
    uint64_t key_size;
    uint64_t value_offset;
    uint64_t value_size;
} __attribute__((packed)) hik_config_entry_t;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input.elf> <output.hik>\n", argv[0]);
        return 1;
    }

    // 读取ELF文件
    FILE *elf_file = fopen(argv[1], "rb");
    if (!elf_file) {
        perror("Failed to open ELF file");
        return 1;
    }

    fseek(elf_file, 0, SEEK_END);
    long elf_size = ftell(elf_file);
    fseek(elf_file, 0, SEEK_SET);

    uint8_t *elf_data = malloc(elf_size);
    if (!elf_data) {
        perror("Failed to allocate memory");
        fclose(elf_file);
        return 1;
    }

    fread(elf_data, 1, elf_size, elf_file);
    fclose(elf_file);

    // 解析ELF头部
    uint64_t entry_point = 0x100000;  // 默认入口点
    if (elf_size >= 24 && memcmp(elf_data, "\x7fELF", 4) == 0) {
        // ELF文件，读取entry point
        if (elf_size >= 64) {
            entry_point = *(uint64_t*)(elf_data + 24);
        }
    }

    // 创建HIK镜像
    FILE *hik_file = fopen(argv[2], "wb");
    if (!hik_file) {
        perror("Failed to create HIK file");
        free(elf_data);
        return 1;
    }

    // 准备段表（单个代码段）
    hik_segment_t segment = {
        .virt_addr = 0x100000,
        .phys_addr = 0x100000,
        .file_size = elf_size,
        .mem_size = elf_size,
        .flags = 0x5  // 可读可执行
    };

    // 准备配置表（最小配置）
    hik_config_entry_t config_entries[] = {
        {0, 0, 0, 0}  // 空配置
    };

    // 计算偏移量
    uint64_t header_size = sizeof(hik_image_header_t);
    uint64_t segment_table_size = sizeof(segment);
    uint64_t config_table_size = sizeof(config_entries);
    uint64_t kernel_offset = header_size + segment_table_size + config_table_size;

    // 准备头部
    hik_image_header_t header = {0};
    memcpy(header.magic, HIK_IMG_MAGIC, 8);
    header.arch_id = HIK_ARCH_X86_64;
    header.version = 1;
    header.entry_point = entry_point;
    header.image_size = elf_size;
    header.segment_table_offset = header_size;
    header.segment_count = 1;
    header.config_table_offset = header_size + segment_table_size;
    header.config_table_size = config_table_size;

    // 写入HIK镜像
    fwrite(&header, 1, sizeof(header), hik_file);
    fwrite(&segment, 1, sizeof(segment), hik_file);
    fwrite(&config_entries, 1, sizeof(config_entries), hik_file);
    fwrite(elf_data, 1, elf_size, hik_file);

    fclose(hik_file);
    free(elf_data);

    printf("HIK image created successfully:\n");
    printf("  Entry point: 0x%lx\n", entry_point);
    printf("  Image size: %ld bytes\n", elf_size);
    printf("  Output: %s\n", argv[2]);

    return 0;
}