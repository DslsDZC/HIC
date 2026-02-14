#ifndef HIK_BOOTLOADER_STDLIB_H
#define HIK_BOOTLOADER_STDLIB_H

#include <stdint.h>
#include <stddef.h>

// 绝对值
static inline int abs(int x) {
    return (x < 0) ? -x : x;
}

// 64位绝对值
static inline int64_t llabs(int64_t x) {
    return (x < 0) ? -x : x;
}

// 最小值
static inline int min(int a, int b) {
    return (a < b) ? a : b;
}

// 最大值
static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

// 64位最小值
static inline int64_t min64(int64_t a, int64_t b) {
    return (a < b) ? a : b;
}

// 64位最大值
static inline int64_t max64(int64_t a, int64_t b) {
    return (a > b) ? a : b;
}

// 对齐到指定边界
static inline uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static inline uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

// 检查是否对齐
static inline int is_aligned(uint64_t value, uint64_t alignment) {
    return (value & (alignment - 1)) == 0;
}

// 页面对齐
#define PAGE_SIZE 4096
static inline uint64_t page_align_up(uint64_t value) {
    return align_up(value, PAGE_SIZE);
}

static inline uint64_t page_align_down(uint64_t value) {
    return align_down(value, PAGE_SIZE);
}

// 字节转页
static inline uint64_t bytes_to_pages(uint64_t bytes) {
    return (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
}

// 页转字节
static inline uint64_t pages_to_bytes(uint64_t pages) {
    return pages * PAGE_SIZE;
}

// 位操作
static inline int test_bit(uint64_t value, int bit) {
    return (value >> bit) & 1;
}

static inline uint64_t set_bit(uint64_t value, int bit) {
    return value | (1ULL << bit);
}

static inline uint64_t clear_bit(uint64_t value, int bit) {
    return value & ~(1ULL << bit);
}

static inline uint64_t toggle_bit(uint64_t value, int bit) {
    return value ^ (1ULL << bit);
}

// 字节序转换
static inline uint16_t swap16(uint16_t value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

static inline uint32_t swap32(uint32_t value) {
    return ((value & 0xFF) << 24) |
           ((value & 0xFF00) << 8) |
           ((value >> 8) & 0xFF00) |
           ((value >> 24) & 0xFF);
}

static inline uint64_t swap64(uint64_t value) {
    return ((value & 0xFF) << 56) |
           ((value & 0xFF00) << 40) |
           ((value & 0xFF0000) << 24) |
           ((value & 0xFF000000) << 8) |
           ((value >> 8) & 0xFF000000) |
           ((value >> 24) & 0xFF0000) |
           ((value >> 40) & 0xFF00) |
           ((value >> 56) & 0xFF);
}

// CRC32计算
uint32_t crc32(const uint8_t *data, size_t len);

// 内存分配（需要UEFI Boot Services支持）
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

// 字符串转整数
int atoi(const char *str);

#endif // HIK_BOOTLOADER_STDLIB_H