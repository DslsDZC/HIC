/**
 * 字符串操作函数实现
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/**
 * 内存复制
 */
void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

/**
 * 内存填充
 */
void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    
    while (n--) {
        *p++ = (uint8_t)c;
    }
    
    return s;
}

/**
 * 内存比较
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    
    return 0;
}

/**
 * 字符串长度
 */
size_t strlen(const char *s)
{
    size_t len = 0;
    
    while (*s++) {
        len++;
    }
    
    return len;
}

/**
 * 字符串比较
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

/**
 * 查找子字符串
 */
char *strstr(const char *haystack, const char *needle)
{
    if (!needle || !*needle) {
        return (char *)haystack;
    }
    
    if (!haystack) {
        return NULL;
    }
    
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    
    if (needle_len > haystack_len) {
        return NULL;
    }
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return (char *)(haystack + i);
        }
    }
    
    return NULL;
}

/**
 * 字符串比较（限制长度）
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

/**
 * 字符串复制
 */
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    
    while ((*d++ = *src++)) {
        ;
    }
    
    return dest;
}

/**
 * 字符串连接
 */
char *strcat(char *dest, const char *src)
{
    char *d = dest;
    
    while (*d) {
        d++;
    }
    
    while ((*d++ = *src++)) {
        ;
    }
    
    return dest;
}

/**
 * Unicode字符串长度
 */
size_t wcslen(const uint16_t *s)
{
    size_t len = 0;
    
    while (*s++) {
        len++;
    }
    
    return len;
}

/**
 * Unicode字符串比较
 */
int wcscmp(const uint16_t *s1, const uint16_t *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *s1 - *s2;
}

/**
 * Unicode字符串复制
 */
void wcscpy(uint16_t *dest, const uint16_t *src)
{
    while ((*dest++ = *src++)) {
        ;
    }
}

/**
 * UTF-16转UTF-8（简化版本）
 */
int utf16_to_utf8(const uint16_t *src, char *dest, size_t dest_size)
{
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] && j < dest_size - 1) {
        uint16_t c = src[i];
        
        if (c < 0x80) {
            // 1字节
            dest[j++] = (char)c;
        } else if (c < 0x800) {
            // 2字节
            if (j + 2 >= dest_size) break;
            dest[j++] = (char)(0xC0 | (c >> 6));
            dest[j++] = (char)(0x80 | (c & 0x3F));
        } else {
            // 3字节（简化，不处理代理对）
            if (j + 3 >= dest_size) break;
            dest[j++] = (char)(0xE0 | (c >> 12));
            dest[j++] = (char)(0x80 | ((c >> 6) & 0x3F));
            dest[j++] = (char)(0x80 | (c & 0x3F));
        }
        
        i++;
    }
    
    dest[j] = '\0';
    return (int)j;
}

/**
 * UTF-8转UTF-16（简化版本）
 */
void utf8_to_utf16(const char *src, uint16_t *dest, size_t dest_size)
{
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] && j < dest_size - 1) {
        uint8_t c1 = (uint8_t)src[i];
        
        if (c1 < 0x80) {
            // 1字节
            dest[j++] = (uint16_t)c1;
            i++;
        } else if ((c1 & 0xE0) == 0xC0) {
            // 2字节
            if (src[i + 1] && j < dest_size - 1) {
                dest[j++] = ((c1 & 0x1F) << 6) | (src[i + 1] & 0x3F);
                i += 2;
            }
        } else if ((c1 & 0xF0) == 0xE0) {
            // 3字节
            if (src[i + 1] && src[i + 2] && j < dest_size - 1) {
                dest[j++] = ((c1 & 0x0F) << 12) | 
                           ((src[i + 1] & 0x3F) << 6) | 
                           (src[i + 2] & 0x3F);
                i += 3;
            }
        } else {
            i++;
        }
    }
    
    dest[j] = 0;
}

/**
 * 判断是否为数字
 */
int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

/**
 * 判断是否为十六进制数字
 */
int isxdigit(int c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));
}

/**
 * 判断是否为空白字符
 */
int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '\v' || c == '\f');
}

/**
 * 转换为大写
 */
int toupper(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

/**
 * 转换为小写
 */
int tolower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

/**
 * 字符串转无符号长整型
 */
uint64_t strtoull(const char *str, char **endptr, int base)
{
    uint64_t result = 0;
    const char *p = str;
    
    // 跳过空白
    while (isspace(*p)) {
        p++;
    }
    
    // 确定基数
    if (base == 0) {
        if (*p == '0') {
            p++;
            if (*p == 'x' || *p == 'X') {
                base = 16;
                p++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    }
    
    // 转换数字
    while (*p) {
        uint64_t digit;
        
        if (isdigit(*p)) {
            digit = (uint64_t)(*p - '0');
        } else if (base == 16 && isxdigit(*p)) {
            digit = (uint64_t)(toupper(*p) - 'A' + 10);
        } else {
            break;
        }
        
        if ((uint64_t)digit >= (uint64_t)base) {
            break;
        }
        
        result = result * (uint64_t)base + (uint64_t)digit;
        p++;
    }
    
    if (endptr) {
        *endptr = (char *)p;
    }
    
    return result;
}

/**
 * 字符串转有符号长整型
 */
int64_t strtoll(const char *str, char **endptr, int base)
{
    const char *p = str;
    int sign = 1;
    uint64_t result;
    
    // 跳过空白
    while (isspace(*p)) {
        p++;
    }
    
    // 处理符号
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    
    result = strtoull(p, endptr, base);

    return sign * (int64_t)result;
}

/**
 * 简化的格式化输出函数（仅支持%s和%d）
 */
int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    const char *p = fmt;
    char *buf = str;
    size_t remaining = size;

    while (*p && remaining > 1) {
        if (*p == '%') {
            p++;
            if (*p == 's') {
                const char *s = va_arg(args, const char*);
                while (*s && remaining > 1) {
                    *buf++ = *s++;
                    remaining--;
                }
            } else if (*p == 'p') {
                void *ptr = va_arg(args, void*);
                char temp[32];
                int i = 0;
                uint64_t val = (uint64_t)ptr;
                /* 输出0x前缀 */
                const char *prefix = "0x";
                while (*prefix && remaining > 1) {
                    *buf++ = *prefix++;
                    remaining--;
                }
                do {
                    temp[i++] = "0123456789ABCDEF"[val % 16];
                    val /= 16;
                } while (val > 0);
                while (i > 0 && remaining > 1) {
                    *buf++ = temp[--i];
                    remaining--;
                }
            } else if (*p == 'd' || *p == 'i') {
                int val = va_arg(args, int);
                char temp[32];
                int i = 0;
                if (val < 0) {
                    *buf++ = '-';
                    remaining--;
                    val = -val;
                }
                do {
                    temp[i++] = (char)('0' + (val % 10));
                    val /= 10;
                } while (val > 0);
                while (i > 0 && remaining > 1) {
                    *buf++ = temp[--i];
                    remaining--;
                }
            } else if (*p == 'x' || *p == 'X') {
                unsigned int val = va_arg(args, unsigned int);
                char temp[32];
                int i = 0;
                do {
                    temp[i++] = "0123456789ABCDEF"[val % 16];
                    val /= 16;
                } while (val > 0);
                while (i > 0 && remaining > 1) {
                    *buf++ = temp[--i];
                    remaining--;
                }
            } else if (*p == 'l' && *(p+1) == 'l' && *(p+2) == 'x') {
                uint64_t val = va_arg(args, uint64_t);
                p += 2;
                char temp[32];
                int i = 0;
                do {
                    temp[i++] = "0123456789ABCDEF"[val % 16];
                    val /= 16;
                } while (val > 0);
                while (i > 0 && remaining > 1) {
                    *buf++ = temp[--i];
                    remaining--;
                }
            } else if (*p == 'l' && *(p+1) == 'x') {
                unsigned long val = va_arg(args, unsigned long);
                p++;
                char temp[32];
                int i = 0;
                do {
                    temp[i++] = "0123456789ABCDEF"[val % 16];
                    val /= 16;
                } while (val > 0);
                while (i > 0 && remaining > 1) {
                    *buf++ = temp[--i];
                    remaining--;
                }
            } else if (*p == 'l' && *(p+1) == 'l' && *(p+2) == 'd') {
                int64_t val = va_arg(args, int64_t);
                p += 2;
                char temp[32];
                int i = 0;
                if (val < 0) {
                    *buf++ = '-';
                    remaining--;
                    val = -val;
                }
                do {
                    temp[i++] = (char)('0' + (val % 10));
                    val /= 10;
                } while (val > 0);
                while (i > 0 && remaining > 1) {
                    *buf++ = temp[--i];
                    remaining--;
                }
            } else if (*p == 'l' && *(p+1) == 'd') {
                long val = va_arg(args, long);
                p++;
                char temp[32];
                int i = 0;
                if (val < 0) {
                    *buf++ = '-';
                    remaining--;
                    val = -val;
                }
                do {
                    temp[i++] = (char)('0' + (val % 10));
                    val /= 10;
                } while (val > 0);
                while (i > 0 && remaining > 1) {
                    *buf++ = temp[--i];
                    remaining--;
                }
            } else {
                *buf++ = *p;
                remaining--;
            }
            p++;
        } else {
            *buf++ = *p++;
            remaining--;
        }
    }

    *buf = '\0';

    va_end(args);
    return (int)(size - remaining);
}

/**
 * 简化的格式化输入函数（仅支持%d）
 */
int sscanf(const char *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    const char *p = fmt;
    const char *s = str;
    int count = 0;

    while (*p && *s) {
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int *val = va_arg(args, int*);
                int sign = 1;
                int result = 0;

                while (*s == ' ') s++;
                if (*s == '-') {
                    sign = -1;
                    s++;
                } else if (*s == '+') {
                    s++;
                }

                while (*s >= '0' && *s <= '9') {
                    result = result * 10 + (*s - '0');
                    s++;
                }

                *val = sign * result;
                count++;
            } else if (*p == 'u') {
                unsigned int *val = va_arg(args, unsigned int*);
                unsigned int result = 0;

                while (*s == ' ') s++;
                while (*s >= '0' && *s <= '9') {
                    result = result * 10 + (unsigned int)(*s - '0');
                    s++;
                }

                *val = result;
                count++;
            } else if (*p == 'x') {
                unsigned int *val = va_arg(args, unsigned int*);
                unsigned int result = 0;

                while (*s == ' ') s++;
                while (*s) {
                    if (*s >= '0' && *s <= '9') {
                        result = result * 16 + (unsigned int)(*s - '0');
                    } else if (*s >= 'a' && *s <= 'f') {
                        result = result * 16 + (unsigned int)(*s - 'a' + 10);
                    } else if (*s >= 'A' && *s <= 'F') {
                        result = result * 16 + (unsigned int)(*s - 'A' + 10);
                    } else {
                        break;
                    }
                    s++;
                }

                *val = result;
                count++;
            }
            p++;
        } else if (*p == *s) {
            p++;
            s++;
        } else {
            break;
        }
    }

    va_end(args);
    return count;
}

/**
 * 字符串转整数（简化版本）
 */
