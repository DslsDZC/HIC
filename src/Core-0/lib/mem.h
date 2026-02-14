/**
 * HIK内核内存操作库
 */

#ifndef HIK_LIB_MEM_H
#define HIK_LIB_MEM_H

#include <stdint.h>
#include <stddef.h>

/* 内存填充 */
void *memzero(void *ptr, size_t len);
void *memset(void *ptr, int value, size_t len);

/* 内存复制 */
void *memcopy(void *dest, const void *src, size_t len);
void *memmove(void *dest, const void *src, size_t len);

/* 内存比较 */
int memcmp(const void *ptr1, const void *ptr2, size_t len);

/* 字节交换 */
uint16_t swap16(uint16_t value);
uint32_t swap32(uint32_t value);
uint64_t swap64(uint64_t value);

/* 内存对齐 */
uintptr_t align_up(uintptr_t value, uintptr_t alignment);
uintptr_t align_down(uintptr_t value, uintptr_t alignment);

#endif /* HIK_LIB_MEM_H */