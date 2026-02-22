#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统 - 快速启动向导
为普通用户提供简单的构建向导
"""

import os
import sys
import subprocess
from pathlib import Path

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent


def clear_screen():
    """清屏"""
    os.system('clear' if os.name == 'posix' else 'cls')


def print_header():
    """打印标题"""
    print("""
╔════════════════════════════════════════════════════════════╗
║            HIC 构建系统 - 快速启动向导                      ║
╚════════════════════════════════════════════════════════════╝
""")


def run_command(cmd, description=""):
    """运行命令"""
    if description:
        print(f"\n{description}...")
    else:
        print(f"\n执行: {' '.join(cmd)}")

    result = subprocess.run(cmd, cwd=str(PROJECT_ROOT), shell=isinstance(cmd, str))
    return result.returncode == 0


def main_menu():
    """主菜单"""
    while True:
        print("\n请选择操作:")
        print("  1. 开始构建 (推荐)")
        print("  2. 仅构建引导程序")
        print("  3. 仅构建内核")
        print("  4. 创建 USB 启动镜像")
        print("  5. 创建 ISO 镜像")
        print("  6. 清理构建文件")
        print("  7. 查看帮助")
        print("  0. 退出")

        choice = input("\n请输入选项 (0-7): ").strip()

        if choice == '1':
            build_all()
        elif choice == '2':
            build_bootloader()
        elif choice == '3':
            build_kernel()
        elif choice == '4':
            create_usb_image()
        elif choice == '5':
            create_iso_image()
        elif choice == '6':
            clean_build()
        elif choice == '7':
            show_help()
        elif choice == '0':
            print("\n再见！")
            break
        else:
            print("\n无效选项，请重新选择")


def build_all():
    """构建所有组件"""
    print("\n开始构建所有组件...")
    print("这将包括引导程序和内核")
    confirm = input("确认继续? (y/n): ").strip().lower()

    if confirm == 'y':
        success = run_command(['make', 'all'], "正在构建")
        if success:
            print("\n✓ 构建成功！")
            print("  引导程序: src/bootloader/bin/bootx64.efi")
            print("  内核: build/bin/hic-kernel.elf")
        else:
            print("\n✗ 构建失败，请检查错误信息")
    else:
        print("已取消")


def build_bootloader():
    """仅构建引导程序"""
    print("\n开始构建引导程序...")
    success = run_command(['make', 'bootloader'], "正在构建引导程序")
    if success:
        print("\n✓ 引导程序构建成功！")
    else:
        print("\n✗ 构建失败")


def build_kernel():
    """仅构建内核"""
    print("\n开始构建内核...")
    success = run_command(['make', 'kernel'], "正在构建内核")
    if success:
        print("\n✓ 内核构建成功！")
    else:
        print("\n✗ 构建失败")


def create_usb_image():
    """创建 USB 启动镜像"""
    print("\n创建 FAT32 镜像（用于 USB 启动）...")
    print("需要先完成构建")

    # 检查构建文件是否存在
    bootloader = PROJECT_ROOT / 'src/bootloader/bin/bootx64.efi'
    kernel = PROJECT_ROOT / 'build/bin/hic-kernel.bin'

    if not bootloader.exists() or not kernel.exists():
        print("\n✗ 构建文件不存在，请先运行构建")
        choice = input("是否现在构建? (y/n): ").strip().lower()
        if choice == 'y':
            build_all()
        else:
            return

    success = run_command(
        ['python3', 'scripts/create_efi_disk_no_root.py',
         '--bootloader', str(bootloader),
         '--kernel', str(kernel),
         '--output', str(PROJECT_ROOT / 'output/hic-usb.img')],
        "正在创建镜像"
    )

    if success:
        print("\n✓ USB 镜像创建成功！")
        print(f"  镜像位置: output/hic-usb.img")
        print("\n如何使用:")
        print("  1. 将镜像写入 USB 设备:")
        print("     sudo dd if=output/hic-usb.img of=/dev/sdX bs=4M status=progress")
        print("  2. 从 USB 启动计算机")
    else:
        print("\n✗ 镜像创建失败")


def create_iso_image():
    """创建 ISO 镜像"""
    print("\n创建 ISO 镜像...")

    success = run_command(['make', 'iso'], "正在创建 ISO")
    if success:
        print("\n✓ ISO 镜像创建成功！")
        print(f"  镜像位置: output/hic-installer.iso")
        print("\n如何使用:")
        print("  1. 刻录到 CD/DVD 或写入 USB")
        print("  2. 从 CD/DVD/USB 启动计算机")
    else:
        print("\n✗ 镜像创建失败")


def clean_build():
    """清理构建文件"""
    print("\n清理构建文件...")
    confirm = input("确认删除所有构建文件? (y/n): ").strip().lower()

    if confirm == 'y':
        success = run_command(['make', 'clean'], "正在清理")
        if success:
            print("\n✓ 清理完成")
        else:
            print("\n✗ 清理失败")
    else:
        print("已取消")


def show_help():
    """显示帮助"""
    print("""
构建说明
--------
HIC 是一个现代操作系统内核

快速开始
  1. 选择"开始构建"完成第一次构建
  2. 构建成功后可以创建启动镜像
  3. 使用镜像启动 HIC 系统

文件说明
  output/bootx64.efi     - UEFI 引导程序
  output/hic-kernel.bin  - 内核二进制文件
  output/hic-usb.img     - USB 启动镜像
  output/hic-installer.iso - ISO 安装镜像

需要帮助?
  查看文档: docs/Wiki/
  或访问: https://github.com/*/HIC
""")


def main():
    """主函数"""
    clear_screen()
    print_header()

    print("欢迎使用 HIC 构建系统！")
    print("本向导将帮助您轻松构建 HIC 系统\n")

    input("按 Enter 键继续...")

    while True:
        clear_screen()
        print_header()
        main_menu()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n再见！")
        sys.exit(0)
    except Exception as e:
        print(f"\n错误: {e}")
        sys.exit(1)
