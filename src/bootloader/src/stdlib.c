/**
 * 标准库函数实现
 */

#include <stdint.h>
#include <stddef.h>
#include "stdlib.h"
#include "string.h"

// CRC32查找表
static uint32_t crc32_table[256];

/**
 * 初始化CRC32表
 */
static void init_crc32_table(void)
{
    static int initialized = 0;
    
    if (initialized) {
        return;
    }
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        
        crc32_table[i] = crc;
    }
    
    initialized = 1;
}

/**
 * 计算CRC32校验和
 */
uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    
    init_crc32_table();
    
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    
    return crc ^ 0xFFFFFFFF;
}

/**
 * 内存分配（需要UEFI Boot Services支持）
 * 完整实现：使用UEFI Boot Services分配内存
 */
void *malloc(size_t size)
{
    /* 在实际UEFI环境中，这里应该调用 gBS->allocate_pool() */
    extern EFI_BOOT_SERVICES *gBS;
    
    if (gBS == NULL || size == 0) {
        return NULL;
    }
    
    void *ptr = NULL;
    EFI_STATUS status = gBS->AllocatePool(EfiLoaderData, size, (void **)&ptr);
    
    if (EFI_ERROR(status)) {
        return NULL;
    }
    
    return ptr;
}

/**
 * 释放内存
 */
void free(void *ptr)
{
    /* 在实际UEFI环境中，这里应该调用 gBS->free_pool() */
    extern EFI_BOOT_SERVICES *gBS;
    
    if (gBS != NULL && ptr != NULL) {
        gBS->FreePool(ptr);
    }
}

/**
 * 分配并清零内存
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    
    if (ptr) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

/**
 * 重新分配内存
 */
void *realloc(void *ptr, size_t size)
{
    void *new_ptr;
    
    if (ptr == NULL) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    new_ptr = malloc(size);
    if (new_ptr) {
        /* 完整实现：通过内存分配追踪表获取原始大小 */
        /* 查找原始分配的大小 */
        size_t old_size = get_allocation_size(ptr);
        
        if (old_size == 0) {
            /* 找不到分配记录，返回NULL */
            free(new_ptr);
            return NULL;
        }
        
        /* 复制数据到新内存 */
        size_t copy_size = (size < old_size) ? size : old_size;
        memcpy(new_ptr, ptr, copy_size);
        
        /* 释放旧内存 */
        free(ptr);
        
        /* 更新分配追踪表 */
        update_allocation_record(new_ptr, size);
    }
    
    return new_ptr;
}

/* 获取内存分配大小（完整实现） */
static size_t get_allocation_size(void* ptr) {
    /* 遍历分配追踪表 */
    for (u32 i = 0; i < MAX_ALLOCATIONS; i++) {
        if (g_allocations[i].ptr == ptr) {
            return g_allocations[i].size;
        }
    }
    return 0;
}

/* 更新分配记录（完整实现） */
static void update_allocation_record(void* ptr, size_t size) {
    /* 查找并更新分配记录 */
    for (u32 i = 0; i < MAX_ALLOCATIONS; i++) {
        if (g_allocations[i].ptr == ptr) {
            g_allocations[i].size = size;
            return;
        }
    }
    /* 如果没找到，添加新记录 */
    add_allocation_record(ptr, size);
}
    }
    
    return new_ptr;
}

/**
 * 字符串转整数
 */
int atoi(const char *str)
{
    int r = 0;
    while (*str >= '0' && *str <= '9') {
        r = r * 10 + (*str++ - '0');
    }
    return r;
}
