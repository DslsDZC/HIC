#!/usr/bin/env python3
"""
HIC 静态模块链接脚本段生成器
从 platform.yaml 读取静态模块配置，自动生成 linker.ld 所需的段定义

用法:
    python3 generate_linker_sections.py <platform.yaml> <output.ld.inc>

生成的片段可直接包含到 linker.ld 中:
    #include "static_modules.ld.inc"
    
或者在 linker.ld 中使用:
    /* @AUTO_GENERATED_STATIC_MODULES@ */
    /* 可被脚本替换 */
"""

import yaml
import os
import sys
import re
import datetime

def sanitize_name(name):
    """将服务名转换为合法的C标识符"""
    return re.sub(r'[^a-zA-Z0-9_]', '_', name)

def generate_linker_sections(config_path, output_path):
    """生成链接脚本段定义"""
    
    # 读取 platform.yaml
    print(f"读取配置: {config_path}")
    with open(config_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)
    
    # 获取静态模块列表
    static_modules = config.get('static_modules', [])
    
    if not static_modules:
        print("警告: 没有配置静态模块")
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write("/* 没有配置静态模块 */\n")
        return
    
    print(f"找到 {len(static_modules)} 个静态模块")
    
    # 获取当前日期
    today = datetime.date.today().strftime("%Y-%m-%d")
    
    # 生成链接脚本片段
    ld_code = f'''/*
 * 自动生成的静态模块链接脚本片段
 * 由 scripts/generate_linker_sections.py 从 platform.yaml 生成
 * 生成日期: {today}
 *
 * 注意: 不要手动修改此文件！
 * 如需添加新模块，请修改 platform.yaml 后重新运行:
 *   python3 scripts/generate_linker_sections.py platform.yaml <output>
 */

/* ==================== 静态服务代码段 ==================== */

'''

    # 生成代码段定义
    for module in static_modules:
        name = module['name']
        safe_name = sanitize_name(name)
        ld_code += f'''        /* {name} 服务段 */
        __start_static_svc_{safe_name}_text = .;
        KEEP(*(.static_svc.{safe_name}.text))
        KEEP(*(.static_svc.{safe_name}.rodata))
        __stop_static_svc_{safe_name}_text = .;

'''

    # 生成数据段定义
    ld_code += '''/* ==================== 静态服务数据段 ==================== */

'''
    for module in static_modules:
        name = module['name']
        safe_name = sanitize_name(name)
        ld_code += f'''        /* {name} 数据段 */
        __start_static_svc_{safe_name}_data = .;
        KEEP(*(.static_svc.{safe_name}.data))
        __stop_static_svc_{safe_name}_data = .;

'''

    # 生成 BSS 段定义
    ld_code += '''/* ==================== 静态服务 BSS 段 ==================== */

'''
    for module in static_modules:
        name = module['name']
        safe_name = sanitize_name(name)
        ld_code += f'''        /* {name} BSS 段 */
        __start_static_svc_{safe_name}_bss = .;
        KEEP(*(.static_svc.{safe_name}.bss))
        __stop_static_svc_{safe_name}_bss = .;

'''

    # 写入输出文件
    print(f"生成链接脚本片段: {output_path}")
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(ld_code)
    
    print(f"成功生成 {len(static_modules)} 个模块的段定义")

def update_linker_script(config_path, linker_path):
    """更新 linker.ld 中的静态模块段定义
    
    使用标记替换：
    /* @STATIC_MODULES_TEXT_START@ */
    ... 自动生成的代码 ...
    /* @STATIC_MODULES_TEXT_END@ */
    """
    
    # 读取 platform.yaml
    print(f"读取配置: {config_path}")
    with open(config_path, 'r', encoding='utf-8') as f:
        config = yaml.safe_load(f)
    
    static_modules = config.get('static_modules', [])
    
    # 读取 linker.ld
    print(f"读取链接脚本: {linker_path}")
    with open(linker_path, 'r', encoding='utf-8') as f:
        linker_content = f.read()
    
    today = datetime.date.today().strftime("%Y-%m-%d")
    
    # 生成代码段内容
    text_content = f'''        /* @STATIC_MODULES_TEXT_START@ - 自动生成 {today} */
'''
    for module in static_modules:
        name = module['name']
        safe_name = sanitize_name(name)
        text_content += f'''        /* {name} 服务段 */
        __start_static_svc_{safe_name}_text = .;
        KEEP(*(.static_svc.{safe_name}.text))
        KEEP(*(.static_svc.{safe_name}.rodata))
        __stop_static_svc_{safe_name}_text = .;

'''
    text_content += '''        /* @STATIC_MODULES_TEXT_END@ */'''
    
    # 生成数据段内容
    data_content = f'''        /* @STATIC_MODULES_DATA_START@ - 自动生成 {today} */
'''
    for module in static_modules:
        name = module['name']
        safe_name = sanitize_name(name)
        data_content += f'''        /* {name} 数据段 */
        __start_static_svc_{safe_name}_data = .;
        KEEP(*(.static_svc.{safe_name}.data))
        __stop_static_svc_{safe_name}_data = .;

'''
    data_content += '''        /* @STATIC_MODULES_DATA_END@ */'''
    
    # 生成 BSS 段内容
    bss_content = f'''        /* @STATIC_MODULES_BSS_START@ - 自动生成 {today} */
'''
    for module in static_modules:
        name = module['name']
        safe_name = sanitize_name(name)
        bss_content += f'''        /* {name} BSS 段 */
        __start_static_svc_{safe_name}_bss = .;
        KEEP(*(.static_svc.{safe_name}.bss))
        __stop_static_svc_{safe_name}_bss = .;

'''
    bss_content += '''        /* @STATIC_MODULES_BSS_END@ */'''
    
    # 替换标记内容
    text_pattern = r'/\* @STATIC_MODULES_TEXT_START@.*?@STATIC_MODULES_TEXT_END@ \*/'
    linker_content = re.sub(text_pattern, text_content, linker_content, flags=re.DOTALL)
    
    data_pattern = r'/\* @STATIC_MODULES_DATA_START@.*?@STATIC_MODULES_DATA_END@ \*/'
    linker_content = re.sub(data_pattern, data_content, linker_content, flags=re.DOTALL)
    
    bss_pattern = r'/\* @STATIC_MODULES_BSS_START@.*?@STATIC_MODULES_BSS_END@ \*/'
    linker_content = re.sub(bss_pattern, bss_content, linker_content, flags=re.DOTALL)
    
    # 检查是否替换成功
    if '@STATIC_MODULES_TEXT_START@' not in linker_content:
        print("警告: 未找到标记 @STATIC_MODULES_TEXT_START@，请手动添加标记到 linker.ld")
    
    # 写回文件
    print(f"更新链接脚本: {linker_path}")
    with open(linker_path, 'w', encoding='utf-8') as f:
        f.write(linker_content)
    
    print(f"成功更新，共 {len(static_modules)} 个模块")

def main():
    if len(sys.argv) < 3:
        print("用法:")
        print("  生成片段文件:")
        print("    generate_linker_sections.py <platform.yaml> <output.ld.inc>")
        print("")
        print("  直接更新 linker.ld:")
        print("    generate_linker_sections.py --update <platform.yaml> <linker.ld>")
        print("")
        print("功能:")
        print("  从 platform.yaml 读取静态模块配置")
        print("  自动生成链接脚本段定义")
        print("  确保 yaml 和 linker.ld 同步")
        sys.exit(1)
    
    if sys.argv[1] == '--update':
        if len(sys.argv) < 4:
            print("错误: --update 需要 platform.yaml 和 linker.ld 参数")
            sys.exit(1)
        config_path = sys.argv[2]
        linker_path = sys.argv[3]
        update_linker_script(config_path, linker_path)
    else:
        config_path = sys.argv[1]
        output_path = sys.argv[2]
        generate_linker_sections(config_path, output_path)

if __name__ == "__main__":
    main()
