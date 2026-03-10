#!/usr/bin/env python3
"""
HIC 静态模块描述符生成器
从 platform.yaml 读取静态模块配置，自动生成 C 代码
"""

import yaml
import os
import sys

def generate_static_modules_c(config_path, output_path, template_dir):
    """生成静态模块 C 文件"""

    # 读取 platform.yaml
    print(f"Reading configuration from: {config_path}")
    with open(config_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)

    # 获取静态模块列表
    static_modules = config.get('static_modules', [])

    if not static_modules:
        print("No static modules configured")
        return

    print(f"Found {len(static_modules)} static module(s)")

    # 生成 C 代码
    c_code = """/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 自动生成的静态模块描述符
 * 由 scripts/generate_static_modules.py 从 platform.yaml 生成
 *
 * 不要手动修改此文件！请修改 platform.yaml 后重新生成
 */

#include <stddef.h>
#include "types.h"
#include "static_module.h"

"""

    for module in static_modules:
        name = module['name']
        module_type = module.get('type', 'service')
        auto_start = module.get('auto_start', False)
        critical = module.get('critical', False)
        privileged = module.get('privileged', False)
        capabilities = module.get('capabilities', [])

        # 转换为标志位
        flags = 0
        if auto_start:
            flags |= (1 << 1)  # STATIC_MODULE_FLAG_AUTO_START
        if critical:
            flags |= (1 << 0)  # STATIC_MODULE_FLAG_CRITICAL
        if privileged:
            flags |= (1 << 2)  # STATIC_MODULE_FLAG_PRIVILEGED

        # 生成类型编号
        type_num = 1  # 默认 service
        if module_type == 'driver':
            type_num = 1
        elif module_type == 'service':
            type_num = 2
        elif module_type == 'system':
            type_num = 3

        c_code += f"""/* {name} 静态模块描述符 */
__attribute__((section(".static_modules")))
static static_module_desc_t g_static_module_{name.replace('-', '_')} = {{
    .name = "{name}",
    .type = {type_num},
    .version = 1,
    .code_start = NULL,  /* 由链接器填充 */
    .code_end = NULL,    /* 由链接器填充 */
    .data_start = NULL,  /* 由链接器填充 */
    .data_end = NULL,    /* 由链接器填充 */
    .entry_offset = 0,   /* 由链接器计算 */
    .capabilities = {{0}},
    .flags = {flags},
}};

"""

    c_code += """
/* 静态模块表结束标记 */
__attribute__((section(".static_modules")))
static static_module_desc_t g_static_modules_end_marker = {
    .name = "",
    .type = 0,
    .version = 0,
    .code_start = NULL,
    .code_end = NULL,
    .data_start = NULL,
    .data_end = NULL,
    .entry_offset = 0,
    .capabilities = {0},
    .flags = 0,
};
"""

    # 写入输出文件
    print(f"Writing generated code to: {output_path}")
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(c_code)

    print(f"Successfully generated static modules descriptor")

def main():
    if len(sys.argv) < 3:
        print("Usage: generate_static_modules.py <platform.yaml> <output.c> [template_dir]")
        sys.exit(1)

    config_path = sys.argv[1]
    output_path = sys.argv[2]
    template_dir = sys.argv[3] if len(sys.argv) > 3 else None

    if not os.path.exists(config_path):
        print(f"Error: Configuration file not found: {config_path}")
        sys.exit(1)

    try:
        generate_static_modules_c(config_path, output_path, template_dir)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()