/**
 * 嵌入的平台配置
 * 将platform.yaml编译进引导程序，确保即使文件系统读取失败也能提供配置
 * 注意：YAML数据在编译时从platform.yaml文件生成
 */

#include <stdint.h>

// 外部声明的嵌入式配置数据和函数
extern const char* get_embedded_platform_config(void);
extern uint32_t get_embedded_platform_config_size(void);
