#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统交互式 Shell
支持 Tab 补全、系统级命令和构建命令
"""

import os
import sys
import subprocess
import cmd
import glob
import shutil
from pathlib import Path
from typing import List, Dict, Optional

# 配置
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
HIC_DIR = PROJECT_ROOT


class HICShell(cmd.Cmd):
    """HIC 构建系统交互式 Shell"""

    intro = f"""
HIC 构建系统 v0.1.0

欢迎使用！当前目录: {HIC_DIR}

快速开始:
  输入 BUILD      开始构建
  输入 HELP       查看帮助
  输入 EXIT       退出系统

"""
    prompt = '> '

    def __init__(self):
        super().__init__()
        self.current_dir = HIC_DIR
        self.build_config = {}

    # ==================== 基础命令 ====================

    def do_exit(self, arg):
        """退出 HIC Shell"""
        print("再见！")
        return True

    def do_quit(self, arg):
        """退出 HIC Shell"""
        return self.do_exit(arg)

    def do_help(self, arg):
        """显示帮助信息"""
        if arg:
            super().do_help(arg)
        else:
            print("""
HIC 构建系统 - 快速帮助

构建
  BUILD              构建所有组件
  BUILD-BOOTLOADER   构建引导程序
  BUILD-KERNEL       构建内核
  BUILD-ISO          创建ISO镜像
  CLEAN              清理构建文件
  INSTALL            安装到output目录

配置
  CONFIG-SHOW        查看当前配置
  PRESET debug       使用调试配置
  PRESET release     使用发布配置

镜像
  IMAGE-FAT32        创建USB启动镜像
  IMAGE-ALL          创建所有格式镜像

文件操作
  CD 目录            切换目录
  LS                 列出文件
  CAT 文件           查看文件内容

系统命令
  直接输入任何命令，如: make clean, git status

其他
  HELP               显示帮助
  CLEAR              清屏
  EXIT               退出

示例
  > BUILD
  > PRESET debug
  > BUILD-KERNEL
  > IMAGE-FAT32
  > EXIT
""")

    def do_clear(self, arg):
        """清屏"""
        os.system('clear' if os.name == 'posix' else 'cls')

    # ==================== 构建命令 ====================

    def do_BUILD(self, arg):
        """构建所有组件（引导程序 + 内核）"""
        print("开始构建...")
        print("这可能需要几分钟时间，请耐心等待")
        self._run_make('all')

    def do_BUILD_BOOTLOADER(self, arg):
        """仅构建引导程序"""
        self._run_make('bootloader')

    def do_BUILD_KERNEL(self, arg):
        """仅构建内核"""
        self._run_make('kernel')

    def do_BUILD_IMAGE(self, arg):
        """生成HIC镜像"""
        self._run_make('image')

    def do_BUILD_ISO(self, arg):
        """创建ISO镜像"""
        self._run_make('iso')

    def do_CLEAN(self, arg):
        """清理所有构建文件"""
        self._run_make('clean')

    def do_INSTALL(self, arg):
        """安装构建产物"""
        self._run_make('install')

    # ==================== 配置命令 ====================

    def do_CONFIG_SHOW(self, arg):
        """显示当前编译配置"""
        config_file = PROJECT_ROOT / 'build_config.mk'
        if config_file.exists():
            print("\n=== 当前编译配置 ===")
            with open(config_file, 'r') as f:
                for line in f:
                    if line.strip() and not line.startswith('#'):
                        print(line.rstrip())
            print()
        else:
            print("配置文件不存在")

    def do_CONFIG_SET(self, arg):
        """设置配置选项: CONFIG_SET CONFIG_DEBUG=1"""
        if '=' not in arg:
            print("用法: CONFIG_SET <选项>=<值>")
            print("示例: CONFIG_SET CONFIG_DEBUG=1")
            return

        key, value = arg.split('=', 1)
        key = key.strip()
        value = value.strip()

        if not key.startswith('CONFIG_'):
            print("配置选项必须以 CONFIG_ 开头")
            return

        self.build_config[key] = value
        print(f"已设置: {key}={value}")
        print("注意: 使用 CONFIG_RESET 或运行构建命令使配置生效")

    def do_CONFIG_RESET(self, arg):
        """重置为默认配置"""
        self.build_config.clear()
        print("配置已重置为默认值")

    def do_PRESET(self, arg):
        """使用预设配置: PRESET <balanced|release|debug|minimal|performance>"""
        if not arg:
            print("用法: PRESET <balanced|release|debug|minimal|performance>")
            return

        presets = ['balanced', 'release', 'debug', 'minimal', 'performance']
        if arg not in presets:
            print(f"无效的预设配置。可用选项: {', '.join(presets)}")
            return

        self._run_make(f'build-{arg}')

    # ==================== 镜像创建命令 ====================

    def do_IMAGE_FAT32(self, arg):
        """创建FAT32磁盘镜像"""
        print("创建FAT32磁盘镜像...")
        result = self._run_python_script('create_efi_disk.py', [
            '--bootloader', str(PROJECT_ROOT / 'src/bootloader/bin/bootx64.efi'),
            '--kernel', str(PROJECT_ROOT / 'build/bin/hic-kernel.bin'),
            '--output', str(PROJECT_ROOT / 'output/hic-fat32.img')
        ])
        if result == 0:
            print("✓ FAT32镜像创建完成")

    def do_IMAGE_VHD(self, arg):
        """创建VHD虚拟硬盘镜像"""
        print("创建VHD镜像...")
        if not shutil.which('qemu-img'):
            print("错误: qemu-img 未安装")
            return
        fat32_img = PROJECT_ROOT / 'output/hic-fat32.img'
        vhd_file = PROJECT_ROOT / 'output/hic.vhd'
        if fat32_img.exists():
            self._run_command(['qemu-img', 'convert', '-f', 'raw', '-O', 'vpc',
                            str(fat32_img), str(vhd_file)])
        else:
            print("错误: 请先创建FAT32镜像 (IMAGE-FAT32)")

    def do_IMAGE_VMDK(self, arg):
        """创建VMDK虚拟机镜像"""
        print("创建VMDK镜像...")
        if not shutil.which('qemu-img'):
            print("错误: qemu-img 未安装")
            return
        fat32_img = PROJECT_ROOT / 'output/hic-fat32.img'
        vmdk_file = PROJECT_ROOT / 'output/hic.vmdk'
        if fat32_img.exists():
            self._run_command(['qemu-img', 'convert', '-f', 'raw', '-O', 'vmdk',
                            str(fat32_img), str(vmdk_file)])
        else:
            print("错误: 请先创建FAT32镜像 (IMAGE-FAT32)")

    def do_IMAGE_QCOW2(self, arg):
        """创建QCOW2镜像"""
        print("创建QCOW2镜像...")
        if not shutil.which('qemu-img'):
            print("错误: qemu-img 未安装")
            return
        fat32_img = PROJECT_ROOT / 'output/hic-fat32.img'
        qcow2_file = PROJECT_ROOT / 'output/hic.qcow2'
        if fat32_img.exists():
            self._run_command(['qemu-img', 'convert', '-f', 'raw', '-O', 'qcow2',
                            str(fat32_img), str(qcow2_file)])
        else:
            print("错误: 请先创建FAT32镜像 (IMAGE-FAT32)")

    def do_IMAGE_ALL(self, arg):
        """创建所有镜像格式"""
        print("创建所有镜像格式...")
        self.do_IMAGE_FAT32('')
        self.do_IMAGE_VHD('')
        self.do_IMAGE_VMDK('')
        self.do_IMAGE_QCOW2('')
        print("✓ 所有镜像格式创建完成")

    # ==================== 系统命令 ====================

    def do_CD(self, arg):
        """切换目录（相对于HIC根目录）"""
        if not arg:
            print(f"当前目录: {self.current_dir}")
            return

        # 支持绝对路径和相对路径
        if arg.startswith('/'):
            # 绝对路径，相对于HIC根目录
            new_dir = PROJECT_ROOT / arg[1:]
        else:
            # 相对路径
            new_dir = self.current_dir / arg

        if new_dir.exists() and new_dir.is_dir():
            self.current_dir = new_dir.resolve()
            print(f"当前目录: {self.current_dir}")
        else:
            print(f"目录不存在: {arg}")

    def do_PWD(self, arg):
        """显示当前目录"""
        print(f"当前目录: {self.current_dir}")

    def do_LS(self, arg):
        """列出目录内容"""
        target_dir = self.current_dir / arg if arg else self.current_dir
        if not target_dir.exists():
            print(f"目录不存在: {target_dir}")
            return

        try:
            items = sorted(target_dir.iterdir(), key=lambda x: (not x.is_dir(), x.name))
            for item in items:
                marker = '/' if item.is_dir() else ''
                print(f"  {item.name}{marker}")
        except PermissionError:
            print(f"权限拒绝: {target_dir}")

    def do_CAT(self, arg):
        """显示文件内容"""
        if not arg:
            print("用法: CAT <文件>")
            return

        file_path = self.current_dir / arg
        if not file_path.exists():
            print(f"文件不存在: {arg}")
            return

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                print(f.read())
        except PermissionError:
            print(f"权限拒绝: {arg}")

    def do_MKDIR(self, arg):
        """创建目录"""
        if not arg:
            print("用法: MKDIR <目录>")
            return

        dir_path = self.current_dir / arg
        try:
            dir_path.mkdir(parents=True, exist_ok=True)
            print(f"目录已创建: {arg}")
        except Exception as e:
            print(f"创建目录失败: {e}")

    def do_RM(self, arg):
        """删除文件"""
        if not arg:
            print("用法: RM <文件>")
            return

        file_path = self.current_dir / arg
        if not file_path.exists():
            print(f"文件不存在: {arg}")
            return

        try:
            if file_path.is_dir():
                shutil.rmtree(file_path)
            else:
                file_path.unlink()
            print(f"已删除: {arg}")
        except Exception as e:
            print(f"删除失败: {e}")

    def do_CP(self, arg):
        """复制文件: CP <源> <目标>"""
        parts = arg.split()
        if len(parts) != 2:
            print("用法: CP <源> <目标>")
            return

        src, dst = parts
        src_path = self.current_dir / src
        dst_path = self.current_dir / dst

        if not src_path.exists():
            print(f"源文件不存在: {src}")
            return

        try:
            if src_path.is_dir():
                shutil.copytree(src_path, dst_path)
            else:
                shutil.copy2(src_path, dst_path)
            print(f"已复制: {src} -> {dst}")
        except Exception as e:
            print(f"复制失败: {e}")

    def do_MV(self, arg):
        """移动/重命名文件: MV <源> <目标>"""
        parts = arg.split()
        if len(parts) != 2:
            print("用法: MV <源> <目标>")
            return

        src, dst = parts
        src_path = self.current_dir / src
        dst_path = self.current_dir / dst

        if not src_path.exists():
            print(f"源文件不存在: {src}")
            return

        try:
            shutil.move(str(src_path), str(dst_path))
            print(f"已移动: {src} -> {dst}")
        except Exception as e:
            print(f"移动失败: {e}")

    # RUN 命令已移除，所有未识别的命令都会直接作为系统命令执行

    def default(self, line):
        """处理未识别的命令，直接作为系统命令执行"""
        if line.strip():
            print(f"执行: {line}")
            self._run_command(line, shell=True)

    # ==================== 调试命令 ====================

    def do_DEBUG(self, arg):
        """启动GDB调试"""
        self._run_make('debug')

    def do_TEST(self, arg):
        """快速测试启动"""
        self._run_make('test')

    def do_QEMU(self, arg):
        """启动QEMU"""
        self._run_make('qemu')

    # ==================== 辅助方法 ====================

    def _run_make(self, target: str) -> int:
        """运行 Make 命令"""
        cmd = ['make', target]
        if self.build_config:
            # 添加配置参数
            for key, value in self.build_config.items():
                cmd.append(f'{key}={value}')

        return self._run_command(cmd)

    def _run_command(self, cmd, shell=False) -> int:
        """运行命令"""
        try:
            if shell:
                result = subprocess.run(cmd, shell=True, cwd=str(self.current_dir))
            else:
                result = subprocess.run(cmd, cwd=str(self.current_dir))
            return result.returncode
        except Exception as e:
            print(f"命令执行失败: {e}")
            return 1

    def _run_python_script(self, script_name: str, args: List[str] = None) -> int:
        """运行 Python 脚本"""
        script_path = SCRIPT_DIR / script_name
        if not script_path.exists():
            print(f"脚本不存在: {script_name}")
            return 1

        cmd = ['python3', str(script_path)]
        if args:
            cmd.extend(args)

        return self._run_command(cmd)

    # ==================== Tab 补全 ====================

    def complete_CD(self, text, line, begidx, endidx):
        """CD 命令的 Tab 补全"""
        return self._complete_path(text)

    def complete_LS(self, text, line, begidx, endidx):
        """LS 命令的 Tab 补全"""
        return self._complete_path(text)

    def complete_CAT(self, text, line, begidx, endidx):
        """CAT 命令的 Tab 补全"""
        return self._complete_path(text, files_only=True)

    def complete_RM(self, text, line, begidx, endidx):
        """RM 命令的 Tab 补全"""
        return self._complete_path(text)

    def complete_CP(self, text, line, begidx, endidx):
        """CP 命令的 Tab 补全"""
        parts = line.split()
        if len(parts) == 2 or (len(parts) == 1 and not text):
            return self._complete_path(text)
        elif len(parts) == 3:
            return self._complete_path(text)
        return []

    def complete_MV(self, text, line, begidx, endidx):
        """MV 命令的 Tab 补全"""
        parts = line.split()
        if len(parts) == 2 or (len(parts) == 1 and not text):
            return self._complete_path(text)
        elif len(parts) == 3:
            return self._complete_path(text)
        return []

    # 移除 RUN 命令的 Tab 补全，因为所有命令都可以直接执行

    def _complete_path(self, text: str, files_only: bool = False, executables_only: bool = False) -> List[str]:
        """路径补全"""
        if not text:
            # 补全当前目录
            items = list(self.current_dir.iterdir())
        elif '/' in text:
            # 包含路径分隔符，补全指定路径
            base_dir = self.current_dir / text.rsplit('/', 1)[0]
            prefix = text.rsplit('/', 1)[1]
            if not base_dir.exists():
                return []
            items = list(base_dir.iterdir())
        else:
            # 补全当前目录
            base_dir = self.current_dir
            prefix = text
            items = list(base_dir.iterdir())

        completions = []
        for item in items:
            name = item.name
            if not name.startswith(prefix):
                continue

            if files_only and item.is_dir():
                continue

            if executables_only and not (item.is_file() and os.access(item, os.X_OK)):
                continue

            completion = name
            if item.is_dir():
                completion += '/'

            completions.append(completion)

        return completions

    def complete_PRESET(self, text, line, begidx, endidx):
        """PRESET 命令的 Tab 补全"""
        presets = ['balanced', 'release', 'debug', 'minimal', 'performance']
        return [p for p in presets if p.startswith(text)]


def main():
    """主函数"""
    # 切换到 HIC 目录
    os.chdir(HIC_DIR)

    # 创建并启动 Shell
    shell = HICShell()
    try:
        shell.cmdloop()
    except KeyboardInterrupt:
        print("\n\n再见！")
        sys.exit(0)


if __name__ == '__main__':
    main()