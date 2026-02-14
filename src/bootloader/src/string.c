/**
 * 字符串操作函数实现
 */

#include <stdint.h>
#include <stddef.h>

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
    return j;
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
            digit = *p - '0';
        } else if (base == 16 && isxdigit(*p)) {
            digit = toupper(*p) - 'A' + 10;
        } else {
            break;
        }
        
        if ((uint64_t)digit >= (uint64_t)base) {
            break;
        }
        
        result = result * base + digit;
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
 * 字符串转整数（简化版本）
 */
