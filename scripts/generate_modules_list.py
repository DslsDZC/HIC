#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

"""
modules.list 生成器

从构建目录生成模块加载列表文件。

modules.list 格式：
-------------------
# 注释以 # 开头
# 格式: 模块名称 [优先级] [依赖:dep1,dep2,...] [自动启动:yes/no]

config_service 4 auto:yes deps:
password_manager_service 7 auto:yes deps:
vga_service 5 auto:yes deps:
serial_service 5 auto:yes deps:
crypto_service 6 auto:yes deps:
lib_manager_service 8 auto:yes deps:cryto_service
module_manager_service 3 auto:yes deps:fat32_service,password_manager_service,crypto_service
cli_service 9 auto:yes deps:serial_service,vga_service
fat32_service 2 auto:yes deps:
libc_service 10 auto:yes deps:
"""

import os
import sys
import yaml
from pathlib import Path

# 模块加载顺序（按优先级排序，数字越小越先加载）
MODULE_ORDER = {
    'fat32_service': 2,
    'module_manager_service': 3,
    'config_service': 4,
    'vga_service': 5,
    'serial_service': 5,
    'crypto_service': 6,
    'password_manager_service': 7,
    'lib_manager_service': 8,
    'cli_service': 9,
    'libc_service': 10,
}

# 模块依赖
MODULE_DEPS = {
    'fat32_service': [],
    'module_manager_service': ['fat32_service', 'password_manager_service', 'crypto_service'],
    'config_service': [],
    'vga_service': [],
    'serial_service': [],
    'crypto_service': [],
    'password_manager_service': [],
    'lib_manager_service': ['crypto_service'],
    'cli_service': ['serial_service', 'vga_service'],
    'libc_service': [],
}

# 是否自动启动
MODULE_AUTO_START = {
    'fat32_service': True,
    'module_manager_service': True,
    'config_service': True,
    'vga_service': True,
    'serial_service': True,
    'crypto_service': True,
    'password_manager_service': True,
    'lib_manager_service': True,
    'cli_service': True,
    'libc_service': True,
}


def parse_hicmod_file(hicmod_path):
    """解析 .hicmod 文件获取模块信息"""
    info = {
        'name': None,
        'version': None,
        'uuid': None,
        'priority': 10,
        'auto_start': True,
        'dependencies': [],
    }
    
    if not os.path.exists(hicmod_path):
        return info
    
    with open(hicmod_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 简单的 INI 格式解析
    current_section = None
    for line in content.split('\n'):
        line = line.strip()
        
        if line.startswith('[') and line.endswith(']'):
            current_section = line[1:-1]
            continue
        
        if '=' in line:
            key, value = line.split('=', 1)
            key = key.strip()
            value = value.strip()
            
            if current_section == 'service':
                if key == 'name':
                    info['name'] = value
                elif key == 'version':
                    info['version'] = value
                elif key == 'uuid':
                    info['uuid'] = value
            
            elif current_section == 'build':
                if key == 'priority':
                    try:
                        info['priority'] = int(value)
                    except ValueError:
                        pass
                elif key == 'autostart':
                    info['auto_start'] = value.lower() in ('true', 'yes', '1')
            
            elif current_section == 'dependencies':
                if key != 'core_api':
                    info['dependencies'].append(key)
    
    return info


def generate_modules_list(modules_dir, output_path):
    """生成 modules.list 文件"""
    
    modules = []
    
    # 扫描模块目录
    if os.path.exists(modules_dir):
        for filename in os.listdir(modules_dir):
            if filename.endswith('.hicmod'):
                module_name = filename[:-7]  # 去掉 .hicmod 后缀
                hicmod_path = os.path.join(modules_dir, filename)
                
                # 解析 hicmod 文件
                info = parse_hicmod_file(hicmod_path)
                
                # 使用预定义的顺序和依赖
                priority = info.get('priority') or MODULE_ORDER.get(module_name, 10)
                deps = MODULE_DEPS.get(module_name, info.get('dependencies', []))
                auto_start = MODULE_AUTO_START.get(module_name, info.get('auto_start', True))
                
                modules.append({
                    'name': module_name,
                    'priority': priority,
                    'deps': deps,
                    'auto_start': auto_start,
                })
    
    # 按优先级排序
    modules.sort(key=lambda x: x['priority'])
    
    # 生成文件内容
    lines = [
        "# HIC 模块加载列表",
        "# 格式: 模块名称 优先级 auto:yes/no deps:依赖1,依赖2,...",
        "# 优先级数字越小越先加载",
        "#",
        "# 此文件由 generate_modules_list.py 自动生成",
        "# 不要手动编辑",
        "",
    ]
    
    for mod in modules:
        deps_str = ','.join(mod['deps']) if mod['deps'] else ''
        auto_str = 'yes' if mod['auto_start'] else 'no'
        
        line = f"{mod['name']} {mod['priority']} auto:{auto_str}"
        if deps_str:
            line += f" deps:{deps_str}"
        
        lines.append(line)
    
    # 写入文件
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"生成 modules.list: {output_path}")
    print(f"包含 {len(modules)} 个模块:")
    for mod in modules:
        print(f"  - {mod['name']} (优先级: {mod['priority']})")
    
    return len(modules)


def main():
    # 默认路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    modules_dir = os.path.join(project_root, 'build', 'privileged-1', 'modules')
    output_path = os.path.join(project_root, 'output', 'modules.list')
    
    # 解析命令行参数
    if len(sys.argv) >= 2:
        modules_dir = sys.argv[1]
    if len(sys.argv) >= 3:
        output_path = sys.argv[2]
    
    # 确保输出目录存在
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    # 生成文件
    count = generate_modules_list(modules_dir, output_path)
    
    return 0 if count > 0 else 1


if __name__ == '__main__':
    sys.exit(main())
