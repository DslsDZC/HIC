#!/usr/bin/env python3
"""
HIC 静态模块描述符生成器
从 platform.yaml 读取静态模块配置，自动生成 C 代码

设计理念：
- 静态服务解决"先有鸡还是先有蛋"问题（启动时即存在）
- 服务同样支持动态卸载/重载
- Core-0 直接访问内核代码段中的服务描述符
- 自动验证服务是否存在
"""

import yaml
import os
import sys
import re

def sanitize_name(name):
    """将服务名转换为合法的C标识符"""
    return re.sub(r'[^a-zA-Z0-9_]', '_', name)

def check_service_exists(services_dir, service_name):
    """检查服务是否存在"""
    service_path = os.path.join(services_dir, service_name)
    service_c = os.path.join(service_path, 'service.c')
    service_h = os.path.join(service_path, 'service.h')
    return os.path.exists(service_c) and os.path.exists(service_h)

def generate_static_modules_c(config_path, output_path, services_dir):
    """生成静态模块 C 文件"""

    # 读取 platform.yaml
    print(f"读取配置: {config_path}")
    with open(config_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)

    # 获取静态模块列表
    static_modules = config.get('static_modules', [])

    if not static_modules:
        print("警告: 没有配置静态模块")
        # 生成空文件
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write("/* 没有配置静态模块 */\n")
        return

    print(f"配置中找到 {len(static_modules)} 个静态模块")

    # 验证服务存在
    valid_modules = []
    for module in static_modules:
        name = module['name']
        if check_service_exists(services_dir, name):
            valid_modules.append(module)
            print(f"  ✓ {name} - 存在")
        else:
            print(f"  ✗ {name} - 不存在，跳过")

    if not valid_modules:
        print("错误: 没有有效的静态模块")
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write("/* 没有有效的静态模块 */\n")
        return

    print(f"有效模块: {len(valid_modules)} 个")

    # 生成 C 代码 - 使用内核定义的 static_module_desc_t 结构
    c_code = f'''/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 自动生成的静态模块描述符
 * 由 scripts/generate_static_modules.py 从 platform.yaml 生成
 * 
 * 有效服务数: {len(valid_modules)}
 *
 * 设计说明：
 * - 静态服务在内核启动时已存在于代码段
 * - 解决"先有鸡还是先有蛋"问题（基础服务无需从磁盘加载）
 * - 所有服务支持动态卸载/重载
 * - Core-0 通过此表直接访问服务
 *
 * 不要手动修改此文件！请修改 platform.yaml 后重新生成
 */

#include "include/static_module.h"

/* 模块类型映射 */
#define STATIC_MODULE_TYPE_DRIVER      1
#define STATIC_MODULE_TYPE_SERVICE     2
#define STATIC_MODULE_TYPE_SYSTEM      3

'''

    # 生成服务描述符 - 使用 static_module_desc_t 结构
    c_code += "/* ==================== 模块描述符表 ==================== */\n\n"
    
    for module in valid_modules:
        name = module['name']
        module_type = module.get('type', 'service')
        auto_start = module.get('auto_start', False)
        critical = module.get('critical', False)
        privileged = module.get('privileged', False)
        priority = module.get('priority', 100)

        # 转换为标志位（使用内核定义的宏）
        flags = 0
        if auto_start:
            flags |= (1 << 1)  # STATIC_MODULE_FLAG_AUTO_START
        if critical:
            flags |= (1 << 0)  # STATIC_MODULE_FLAG_CRITICAL
        if privileged:
            flags |= (1 << 2)  # STATIC_MODULE_FLAG_PRIVILEGED

        # 生成类型编号
        type_num = 2  # 默认 service
        if module_type == 'driver':
            type_num = 1
        elif module_type == 'service':
            type_num = 2
        elif module_type == 'system':
            type_num = 3

        # 填充 name 字段（固定 32 字符）
        name_field = name[:31]  # 最多 31 字符
        name_padded = name_field + '\\0' * (32 - len(name_field))

        safe_name = sanitize_name(name)
        c_code += f'''/* {name} 静态模块描述符 */
__attribute__((section(".static_modules"), used))
static static_module_desc_t g_static_module_{safe_name} = {{
    .name = "{name_field}",
    .type = {type_num},
    .version = 1,
    .code_start = NULL,          /* 运行时填充 */
    .code_end = NULL,
    .data_start = NULL,
    .data_end = NULL,
    .entry_offset = 0,           /* 运行时填充 */
    .capabilities = {{0}},       /* 运行时填充 */
    .flags = {flags}ULL,  /* STATIC_MODULE_FLAG_* */
}};

'''

    # 生成服务表结束标记
    c_code += f'''/* 模块表结束标记 */
__attribute__((section(".static_modules"), used))
static static_module_desc_t g_static_modules_end = {{
    .name = {{0}},  /* 空名称表示结束 */
    .type = 0,
    .version = 0,
    .code_start = NULL,
    .code_end = NULL,
    .data_start = NULL,
    .data_end = NULL,
    .entry_offset = 0,
    .capabilities = {{0}},
    .flags = 0,
}};
'''

    # 写入输出文件
    print(f"生成代码: {output_path}")
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(c_code)

    print(f"成功生成 {len(valid_modules)} 个静态服务描述符")

def main():
    if len(sys.argv) < 3:
        print("用法: generate_static_modules.py <platform.yaml> <output.c>")
        print("\n功能:")
        print("  从 platform.yaml 读取静态模块配置")
        print("  自动验证服务是否存在")
        print("  生成 C 代码，包含服务描述符表和入口点引用")
        print("\n设计理念:")
        print("  - 静态服务解决启动时的'鸡生蛋'问题")
        print("  - 所有服务支持动态卸载/重载")
        sys.exit(1)

    config_path = sys.argv[1]
    output_path = sys.argv[2]

    # 推断服务目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    services_dir = os.path.normpath(os.path.join(script_dir, '..', 'src', 'Privileged-1', 'services'))

    if not os.path.exists(config_path):
        print(f"错误: 配置文件不存在: {config_path}")
        sys.exit(1)

    if not os.path.exists(services_dir):
        print(f"错误: 服务目录不存在: {services_dir}")
        sys.exit(1)

    try:
        generate_static_modules_c(config_path, output_path, services_dir)
    except Exception as e:
        print(f"错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()