/**
 * 嵌入的平台配置头文件
 */

#ifndef PLATFORM_EMBEDDED_H
#define PLATFORM_EMBEDDED_H

#include <stdint.h>

/**
 * 获取嵌入的平台配置
 * @return 返回嵌入的YAML配置字符串
 */
const char* get_embedded_platform_config(void);

/**
 * 获取嵌入配置的大小
 * @return 返回配置数据的字节数
 */
uint32_t get_embedded_platform_config_size(void);

#endif // PLATFORM_EMBEDDED_H
