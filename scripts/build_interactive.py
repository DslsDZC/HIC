#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
HIC构建系统 - 交互式CLI模式
提供简单的命令行交互界面
"""

import sys
import os
from pathlib import Path

# 项目信息
PROJECT = "HIC System"
VERSION = "0.1.0"
ROOT_DIR = Path(__file__).parent.parent

COLORS = {
    'reset': '\033[0m',
    'bold': '\033[1m',
    'red': '\033[91m',
    'green': '\033[92m',
    'yellow': '\033[93m',
    'blue': '\033[94m',
    'magenta': '\033[95m',
    'cyan': '\033[96m',
    'white': '\033[97m',
}

def print_colored(text, color='white'):
    """打印彩色文本"""
    print(f"{COLORS.get(color, '')}{text}{COLORS['reset']}")

def print_header():
    """打印标题"""
    print()
    print_colored(f"╔{'═' * 50}╗", 'cyan')
    print_colored(f"║  {PROJECT} v{VERSION} - 交互式构建系统  ║", 'cyan')
    print_colored(f"╚{'═' * 50}╝", 'cyan')
    print()

def print_menu():
    """打印主菜单"""
    print_colored("请选择操作：", 'bold')
    print()
    print("  1. 配置编译选项")
    print("  2. 配置运行时选项")
    print("  3. 查看当前配置")
    print("  4. 选择预设配置")
    print("  5. 开始构建")
    print("  6. 清理构建文件")
    print("  7. 帮助")
    print("  0. 退出")
    print()

def show_preset_menu():
    """显示预设配置菜单"""
    print_colored("预设配置：", 'bold')
    print()
    print("  1. balanced  - 平衡配置（推荐）")
    print("  2. release   - 发布配置")
    print("  3. debug     - 调试配置")
    print("  4. minimal   - 最小配置")
    print("  5. performance - 性能配置")
    print("  0. 返回")
    print()

def run_command(command, description=""):
    """运行命令"""
    import subprocess
    
    if description:
        print_colored(f"\n{description}...", 'yellow')
    
    print_colored(f"执行命令: {' '.join(command)}", 'cyan')
    print()
    
    result = subprocess.run(
        command,
        cwd=ROOT_DIR,
        text=True
    )
    
    return result.returncode == 0

def show_current_config():
    """显示当前配置"""
    print_colored("\n当前配置：", 'bold')
    print()
    
    # 显示YAML配置
    config_file = ROOT_DIR / "src" / "bootloader" / "platform.yaml"
    if config_file.exists():
        print_colored(f"配置文件: {config_file}", 'cyan')
        print()
        
        try:
            import yaml
            with open(config_file, 'r', encoding='utf-8') as f:
                config = yaml.safe_load(f)
            
            # 显示关键配置
            if 'build' in config:
                print_colored("编译选项：", 'green')
                for key, value in config['build'].items():
                    print(f"  {key}: {value}")
                print()
            
            if 'system_limits' in config:
                print_colored("系统限制：", 'green')
                limits = config['system_limits']
                print(f"  最大域数: {limits.get('max_domains', 'N/A')}")
                print(f"  最大能力数: {limits.get('max_capabilities', 'N/A')}")
                print(f"  最大线程数: {limits.get('max_threads', 'N/A')}")
                print()
            
            if 'features' in config:
                print_colored("启用功能：", 'green')
                features = config['features']
                enabled = [k for k, v in features.items() if v]
                print(f"  {', '.join(enabled)}")
                print()
                
        except Exception as e:
            print_colored(f"读取配置失败: {e}", 'red')
    else:
        print_colored("配置文件不存在", 'red')

def configure_build_options():
    """配置编译选项"""
    import yaml
    
    config_file = ROOT_DIR / "src" / "bootloader" / "platform.yaml"
    
    if not config_file.exists():
        print_colored("配置文件不存在！", 'red')
        return
    
    try:
        with open(config_file, 'r', encoding='utf-8') as f:
            config = yaml.safe_load(f)
        
        if 'build' not in config:
            config['build'] = {}
        
        print_colored("\n配置编译选项：", 'bold')
        print()
        
        # 优化级别
        print(f"当前优化级别: {config['build'].get('optimize_level', 2)}")
        print("  0 - 无优化")
        print("  1 - 基础优化")
        print("  2 - 标准优化 (推荐)")
        print("  3 - 激进优化")
        opt = input("选择优化级别 (0-3, 留空保持不变): ").strip()
        if opt in ['0', '1', '2', '3']:
            config['build']['optimize_level'] = int(opt)
        
        # 调试符号
        debug_symbols = config['build'].get('debug_symbols', True)
        print(f"\n调试符号: {'是' if debug_symbols else '否'}")
        ds = input("是否包含调试符号？(y/n, 留空保持不变): ").strip().lower()
        if ds == 'y':
            config['build']['debug_symbols'] = True
        elif ds == 'n':
            config['build']['debug_symbols'] = False
        
        # LTO
        lto = config['build'].get('lto', False)
        print(f"\n链接时优化(LTO): {'是' if lto else '否'}")
        lto_choice = input("是否启用LTO？(y/n, 留空保持不变): ").strip().lower()
        if lto_choice == 'y':
            config['build']['lto'] = True
        elif lto_choice == 'n':
            config['build']['lto'] = False
        
        # 保存配置
        with open(config_file, 'w', encoding='utf-8') as f:
            yaml.safe_dump(config, f, default_flow_style=False, allow_unicode=True)
        
        print_colored("\n配置已保存！", 'green')
        
    except Exception as e:
        print_colored(f"配置失败: {e}", 'red')

def configure_features():
    """配置功能特性"""
    import yaml
    
    config_file = ROOT_DIR / "src" / "bootloader" / "platform.yaml"
    
    if not config_file.exists():
        print_colored("配置文件不存在！", 'red')
        return
    
    try:
        with open(config_file, 'r', encoding='utf-8') as f:
            config = yaml.safe_load(f)
        
        if 'features' not in config:
            config['features'] = {}
        
        print_colored("\n配置功能特性：", 'bold')
        print()
        
        # 可配置的功能列表
        feature_options = [
            ('smp', '对称多处理器支持'),
            ('acpi', 'ACPI硬件探测'),
            ('pci', 'PCI总线支持'),
            ('usb', 'USB支持'),
            ('virtio', 'VirtIO虚拟化支持'),
            ('efi', 'UEFI引导支持'),
        ]
        
        for feature, description in feature_options:
            current = config['features'].get(feature, True)
            print(f"\n{feature} - {description}")
            print(f"  当前状态: {'启用' if current else '禁用'}")
            choice = input("  是否启用？(y/n, 留空保持不变): ").strip().lower()
            if choice == 'y':
                config['features'][feature] = True
            elif choice == 'n':
                config['features'][feature] = False
        
        # 保存配置
        with open(config_file, 'w', encoding='utf-8') as f:
            yaml.safe_dump(config, f, default_flow_style=False, allow_unicode=True)
        
        print_colored("\n配置已保存！", 'green')
        
    except Exception as e:
        print_colored(f"配置失败: {e}", 'red')
    """选择预设配置"""
    show_preset_menu()
    
    choice = input("请选择预设配置 (0-5): ").strip()
    
    presets = {
        '1': 'balanced',
        '2': 'release',
        '3': 'debug',
        '4': 'minimal',
        '5': 'performance'
    }
    
    if choice in presets:
        preset = presets[choice]
        print_colored(f"\n应用预设配置: {preset}", 'green')
        
        # 使用make命令应用预设
        run_command(['make', f'build-{preset}'])
    elif choice == '0':
        pass
    else:
        print_colored("无效的选择", 'red')
    
    input("\n按Enter键继续...")

def select_preset():
    """选择预设配置"""
    show_preset_menu()
    
    choice = input("请选择预设配置 (0-5): ").strip()
    
    presets = {
        '1': 'balanced',
        '2': 'release',
        '3': 'debug',
        '4': 'minimal',
        '5': 'performance'
    }
    
    if choice in presets:
        preset = presets[choice]
        print_colored(f"\n应用预设配置: {preset}", 'green')
        
        # 使用make命令应用预设
        run_command(['make', f'build-{preset}'])
    elif choice == '0':
        pass
    else:
        print_colored("无效的选择", 'red')
    
    input("\n按Enter键继续...")

def build():
    print_colored("\n开始构建...", 'yellow')
    print()
    
    # 从命令行参数或用户输入获取构建目标
    import sys
    
    # 检查是否有预设参数
    if len(sys.argv) > 1:
        target_arg = sys.argv[1]
        
        if target_arg in ['balanced', 'release', 'debug', 'minimal', 'performance']:
            print_colored(f"使用预设配置: {target_arg}", 'green')
            print()
            success = run_command(['make', f'build-{target_arg}'])
        elif target_arg in ['bootloader', 'kernel', 'all']:
            if target_arg == 'all':
                success = run_command(['make', 'all'])
            elif target_arg == 'bootloader':
                success = run_command(['make', 'bootloader'])
            else:
                success = run_command(['make', 'kernel'])
        else:
            print_colored(f"未知目标: {target_arg}", 'red')
            success = False
        
        if success:
            print_colored("\n构建成功！", 'green')
        else:
            print_colored("\n构建失败！", 'red')
        
        return
    
    # 交互式选择构建目标
    print_colored("选择构建目标：", 'bold')
    print()
    print("  1. 全部 (引导程序 + 内核)")
    print("  2. 仅引导程序")
    print("  3. 仅内核")
    print("  b. 返回")
    print()
    
    try:
        target_choice = input("请选择 (1-3, b): ").strip()
    except EOFError:
        # 非交互式环境，默认构建全部
        print_colored("非交互式环境，使用默认配置构建全部组件", 'yellow')
        target_choice = '1'
    
    success = False
    
    if target_choice == '1' or target_choice == '':
        success = run_command(['make', 'all'], "构建全部组件")
    elif target_choice == '2':
        success = run_command(['make', 'bootloader'], "构建引导程序")
    elif target_choice == '3':
        success = run_command(['make', 'kernel'], "构建内核")
    elif target_choice.lower() == 'b':
        return
    else:
        print_colored("无效的选择", 'red')
        return
    
    if success:
        print_colored("\n构建成功！", 'green')
        
        # 询问是否安装
        try:
            install = input("\n是否安装到output目录？(y/n): ").strip().lower()
            if install == 'y':
                run_command(['make', 'install'], "安装构建产物")
        except EOFError:
            pass
    else:
        print_colored("\n构建失败！", 'red')
    
    try:
        input("\n按Enter键继续...")
    except EOFError:
        pass

def main():
    """主函数"""
    import subprocess
    
    while True:
        os.system('clear' if os.name != 'nt' else 'cls')
        print_header()
        print_menu()
        
        choice = input("请输入选项 (0-7): ").strip()
        
        if choice == '1':
            configure_build_options()
            input("\n按Enter键继续...")
        
        elif choice == '2':
            configure_features()
            input("\n按Enter键继续...")
        
        elif choice == '3':
            show_current_config()
            input("\n按Enter键继续...")
        
        elif choice == '4':
            select_preset()
        
        elif choice == '5':
            build()
        
        elif choice == '6':
            clean = input("\n确定要清理所有构建文件吗？(y/n): ").strip().lower()
            if clean == 'y':
                run_command(['make', 'clean'], "清理构建文件")
                print_colored("\n清理完成！", 'green')
            input("\n按Enter键继续...")
        
        elif choice == '7':
            print_colored("\n帮助信息", 'yellow')
            print()
            print("界面说明：")
            print("  - TUI界面：需要真正的终端环境（支持键盘操作）")
            print("  - Qt GUI：现代化图形界面（需要PyQt6）")
            print("  - GTK GUI：跨平台图形界面（需要GTK3）")
            print()
            print("预设配置：")
            print("  - balanced: 平衡配置，适合日常开发")
            print("  - release: 发布配置，优化性能和大小")
            print("  - debug: 调试配置，包含完整调试信息")
            print("  - minimal: 最小配置，适合嵌入式系统")
            print("  - performance: 性能配置，启用所有优化")
            print()
            print("快速构建命令：")
            print("  make build-balanced      # 使用平衡配置")
            print("  make build-release       # 使用发布配置")
            print("  make build-debug         # 使用调试配置")
            print("  make build-performance   # 使用性能配置")
            print()
            print("文档：")
            print("  docs/BUILD_SYSTEM_GUIDE.md - 构建系统完整指南")
            print()
            input("按Enter键继续...")
        
        elif choice == '0':
            print_colored("\n感谢使用 HIC 构建系统！", 'green')
            break
        
        else:
            print_colored("\n无效的选择，请重试", 'red')
            input("按Enter键继续...")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print()
        print_colored("\n\n用户中断，退出程序", 'yellow')
        sys.exit(0)
    except Exception as e:
        print_colored(f"\n错误: {e}", 'red')
        import traceback
        traceback.print_exc()
        sys.exit(1)
