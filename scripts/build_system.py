#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
HIK系统构建系统
功能：
1. 自动获取签名
2. 构建引导程序和内核
3. 签名验证
4. 输出最终镜像
"""

import os
import sys
import subprocess
import argparse
import hashlib
import struct
import json
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple

# 配置
CONFIG = {
    "project": "HIK System",
    "version": "0.1.0",
    "root_dir": Path(__file__).parent,
    "build_dir": Path(__file__).parent / "build",
    "output_dir": Path(__file__).parent / "output",
    "sign_key_file": "signing_key.pem",
    "sign_cert_file": "signing_cert.pem",
}


class BuildError(Exception):
    """构建错误"""
    pass


class BuildSystem:
    """HIK构建系统"""

    def __init__(self):
        self.config = CONFIG
        self.start_time = datetime.now()
        self.build_log: List[str] = []

    def log(self, message: str, level: str = "INFO"):
        """记录日志"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_entry = f"[{timestamp}] [{level}] {message}"
        self.build_log.append(log_entry)
        print(log_entry)

    def check_dependencies(self) -> bool:
        """检查构建依赖"""
        self.log("检查构建依赖...")
        
        required_tools = ["gcc", "make", "objcopy", "objdump"]
        missing_tools = []

        for tool in required_tools:
            if not self._command_exists(tool):
                missing_tools.append(tool)

        if missing_tools:
            self.log(f"缺少依赖: {', '.join(missing_tools)}", "ERROR")
            return False

        # 检查UEFI工具
        if not self._command_exists("x86_64-w64-mingw32-gcc"):
            self.log("警告: 未找到 x86_64-w64-mingw32-gcc，无法构建UEFI引导程序", "WARNING")

        # 检查BIOS工具
        if not self._command_exists("x86_64-elf-gcc"):
            self.log("警告: 未找到 x86_64-elf-gcc，无法构建内核", "WARNING")

        # 检查签名工具
        if not self._command_exists("openssl"):
            self.log("警告: 未找到 openssl，无法进行签名操作", "WARNING")

        self.log("依赖检查完成")
        return True

    def _command_exists(self, command: str) -> bool:
        """检查命令是否存在"""
        try:
            subprocess.run(
                ["which", command],
                check=True,
                capture_output=True,
                text=True
            )
            return True
        except subprocess.CalledProcessError:
            return False

    def generate_signing_keys(self) -> Tuple[bool, str]:
        """生成签名密钥对"""
        self.log("生成签名密钥对...")

        key_file = self.config["build_dir"] / self.config["sign_key_file"]
        cert_file = self.config["build_dir"] / self.config["sign_cert_file"]

        if key_file.exists() and cert_file.exists():
            self.log("密钥文件已存在，跳过生成")
            return True, str(key_file)

        try:
            # 生成私钥
            subprocess.run([
                "openssl", "genrsa",
                "-out", str(key_file),
                "4096"
            ], check=True, capture_output=True)

            # 生成自签名证书
            subprocess.run([
                "openssl", "req",
                "-new",
                "-x509",
                "-key", str(key_file),
                "-out", str(cert_file),
                "-days", "3650",
                "-subj", f"/C=CN/ST=Beijing/O=HIK/CN=HIK-{self.config['version']}"
            ], check=True, capture_output=True)

            self.log(f"密钥生成成功: {key_file}")
            self.log(f"证书生成成功: {cert_file}")
            return True, str(key_file)

        except subprocess.CalledProcessError as e:
            self.log(f"密钥生成失败: {e.stderr}", "ERROR")
            return False, ""

    def sign_file(self, input_file: Path, output_file: Path) -> bool:
        """对文件进行签名"""
        self.log(f"签名文件: {input_file}")

        key_file = self.config["build_dir"] / self.config["sign_key_file"]

        if not key_file.exists():
            self.log("签名密钥不存在，跳过签名", "WARNING")
            return False

        try:
            # 计算文件哈希
            file_hash = self._calculate_hash(input_file)

            # 使用OpenSSL签名
            sig_file = output_file.with_suffix(".sig")
            subprocess.run([
                "openssl", "dgst",
                "-sha384",
                "-sign", str(key_file),
                "-out", str(sig_file),
                str(input_file)
            ], check=True, capture_output=True)

            # 读取签名
            with open(sig_file, "rb") as f:
                signature = f.read()

            # 创建签名信息
            sig_info = {
                "version": self.config["version"],
                "timestamp": datetime.now().isoformat(),
                "algorithm": "RSA-4096",
                "hash": "SHA-384",
                "file_hash": file_hash.hex(),
                "signature_size": len(signature),
                "signature": signature.hex()
            }

            # 写入签名信息
            with open(output_file, "w") as f:
                json.dump(sig_info, f, indent=2)

            self.log(f"签名成功: {output_file}")
            self.log(f"签名大小: {len(signature)} 字节")
            return True

        except subprocess.CalledProcessError as e:
            self.log(f"签名失败: {e.stderr}", "ERROR")
            return False

    def _calculate_hash(self, file_path: Path) -> bytes:
        """计算文件SHA-384哈希"""
        sha384 = hashlib.sha384()
        with open(file_path, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                sha384.update(chunk)
        return sha384.digest()

    def build_bootloader(self, target: str = "uefi") -> bool:
        """构建引导程序"""
        self.log(f"构建引导程序 (目标: {target})...")

        bootloader_dir = self.config["root_dir"].parent / "src" / "bootloader"

        try:
            # 清理旧的构建
            subprocess.run(
                ["make", "clean"],
                cwd=bootloader_dir,
                check=True,
                capture_output=True
            )

            # 构建目标
            subprocess.run(
                ["make", target],
                cwd=bootloader_dir,
                check=True,
                capture_output=True
            )

            if target == "uefi":
                output = bootloader_dir / "bin" / "bootx64.efi"
            else:
                output = bootloader_dir / "bin" / "bios.bin"

            if not output.exists():
                self.log(f"引导程序构建失败: {output} 不存在", "ERROR")
                return False

            self.log(f"引导程序构建成功: {output}")
            return True

        except subprocess.CalledProcessError as e:
            self.log(f"引导程序构建失败: {e.stderr}", "ERROR")
            return False

    def build_kernel(self) -> bool:
        """构建内核"""
        self.log("构建内核...")

        kernel_dir = self.config["root_dir"].parent / "build"

        try:
            # 清理旧的构建
            subprocess.run(
                ["make", "clean"],
                cwd=kernel_dir,
                check=True,
                capture_output=True
            )

            # 构建内核
            subprocess.run(
                ["make", "all"],
                cwd=kernel_dir,
                check=True,
                capture_output=True
            )

            output = kernel_dir / "bin" / "hik-kernel.elf"

            if not output.exists():
                self.log(f"内核构建失败: {output} 不存在", "ERROR")
                return False

            self.log(f"内核构建成功: {output}")
            return True

        except subprocess.CalledProcessError as e:
            self.log(f"内核构建失败: {e.stderr}", "ERROR")
            return False

    def create_boot_image(self) -> bool:
        """创建启动镜像"""
        self.log("创建启动镜像...")

        # 创建输出目录
        output_dir = self.config["output_dir"]
        output_dir.mkdir(parents=True, exist_ok=True)

        try:
            # 复制文件
            bootloader_uefi = self.config["root_dir"] / "bootloader" / "bin" / "bootx64.efi"
            bootloader_bios = self.config["root_dir"] / "bootloader" / "bin" / "bios.bin"
            kernel = self.config["root_dir"] / "kernel" / "bin" / "hik-kernel.elf"

            # UEFI镜像
            if bootloader_uefi.exists():
                uefi_dir = output_dir / "EFI" / "BOOT"
                uefi_dir.mkdir(parents=True, exist_ok=True)
                import shutil
                shutil.copy(bootloader_uefi, uefi_dir / "bootx64.efi")
                self.log(f"UEFI镜像创建完成: {uefi_dir / 'bootx64.efi'}")

            # BIOS镜像
            if bootloader_bios.exists():
                shutil.copy(bootloader_bios, output_dir / "bios.bin")
                self.log(f"BIOS镜像创建完成: {output_dir / 'bios.bin'}")

            # 内核
            if kernel.exists():
                shutil.copy(kernel, output_dir / "kernel.elf")
                self.log(f"内核复制完成: {output_dir / 'kernel.elf'}")

                # 签名内核
                sig_file = output_dir / "kernel.sig.json"
                self.sign_file(kernel, sig_file)

            # 生成构建报告
            self._generate_build_report(output_dir)

            return True

        except Exception as e:
            self.log(f"启动镜像创建失败: {e}", "ERROR")
            return False

    def _generate_build_report(self, output_dir: Path):
        """生成构建报告"""
        report = {
            "project": self.config["project"],
            "version": self.config["version"],
            "build_time": self.start_time.isoformat(),
            "build_duration": str(datetime.now() - self.start_time),
            "build_type": "full",
            "components": {}
        }

        # 检查组件
        bootloader_uefi = self.config["root_dir"].parent / "src" / "bootloader" / "bin" / "bootx64.efi"
        bootloader_bios = self.config["root_dir"].parent / "src" / "bootloader" / "bin" / "bios.bin"
        kernel = self.config["root_dir"] / "bin" / "hik-kernel.elf"

        if bootloader_uefi.exists():
            report["components"]["uefi_bootloader"] = {
                "path": "EFI/BOOT/bootx64.efi",
                "size": bootloader_uefi.stat().st_size,
                "hash": self._calculate_hash(bootloader_uefi).hex()
            }

        if bootloader_bios.exists():
            report["components"]["bios_bootloader"] = {
                "path": "bios.bin",
                "size": bootloader_bios.stat().st_size,
                "hash": self._calculate_hash(bootloader_bios).hex()
            }

        if kernel.exists():
            report["components"]["kernel"] = {
                "path": "kernel.elf",
                "size": kernel.stat().st_size,
                "hash": self._calculate_hash(kernel).hex()
            }

        # 写入报告
        report_file = output_dir / "build_report.json"
        with open(report_file, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, ensure_ascii=False)

        self.log(f"构建报告生成完成: {report_file}")

    def build(self, targets: List[str] = None) -> bool:
        """执行完整构建"""
        self.log("=" * 60)
        self.log(f"{self.config['project']} 构建系统 v{self.config['version']}")
        self.log("=" * 60)

        # 默认构建目标
        if targets is None:
            targets = ["uefi", "bios", "kernel"]

        # 检查依赖
        if not self.check_dependencies():
            self.log("依赖检查失败，构建终止", "ERROR")
            return False

        # 创建构建目录
        self.config["build_dir"].mkdir(parents=True, exist_ok=True)

        # 生成签名密钥
        self.generate_signing_keys()

        # 构建目标
        success = True
        for target in targets:
            if target in ["uefi", "bios"]:
                if not self.build_bootloader(target):
                    success = False
            elif target == "kernel":
                if not self.build_kernel():
                    success = False

        if not success:
            self.log("部分构建失败", "WARNING")

        # 创建启动镜像
        if not self.create_boot_image():
            self.log("启动镜像创建失败", "ERROR")
            return False

        # 保存构建日志
        self._save_build_log()

        self.log("=" * 60)
        self.log("构建完成!")
        self.log(f"输出目录: {self.config['output_dir']}")
        self.log("=" * 60)

        return True

    def _save_build_log(self):
        """保存构建日志"""
        log_file = self.config["output_dir"] / "build.log"
        with open(log_file, "w", encoding="utf-8") as f:
            f.write("\n".join(self.build_log))
        self.log(f"构建日志保存完成: {log_file}")
    
    def show_config(self):
        """显示当前编译配置"""
        config_file = self.config["root_dir"].parent / "build_config.mk"
        
        if not config_file.exists():
            self.log("配置文件不存在: build_config.mk", "ERROR")
            return
        
        self.log("=" * 60)
        self.log("当前编译配置")
        self.log("=" * 60)
        
        # 读取并解析配置
        config = {}
        with open(config_file, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line.startswith('CONFIG_') and '=' in line:
                    # 分离配置项和注释
                    parts = line.split('#', 1)
                    config_line = parts[0].strip()
                    
                    if '=' in config_line:
                        key, value = config_line.split('=', 1)
                        # 清理键和值
                        key = key.strip().replace('?', '').strip()
                        value = value.strip().replace('?', '').strip()
                        config[key] = value
        
        # 按分类显示配置
        categories = {
            "调试配置": ["CONFIG_DEBUG", "CONFIG_TRACE", "CONFIG_VERBOSE"],
            "安全配置": ["CONFIG_KASLR", "CONFIG_SMEP", "CONFIG_SMAP", "CONFIG_AUDIT", "CONFIG_SECURITY_LEVEL"],
            "性能配置": ["CONFIG_PERF", "CONFIG_FAST_PATH"],
            "功能配置": ["CONFIG_PCI", "CONFIG_ACPI", "CONFIG_SERIAL"],
            "内存配置": ["CONFIG_HEAP_SIZE_MB", "CONFIG_STACK_SIZE_KB", "CONFIG_PAGE_CACHE_PERCENT"],
            "调度器配置": ["CONFIG_SCHEDULER_POLICY", "CONFIG_TIME_SLICE_MS", "CONFIG_MAX_THREADS"],
            "能力系统配置": ["CONFIG_MAX_CAPABILITIES", "CONFIG_CAPABILITY_DERIVATION"],
            "域配置": ["CONFIG_MAX_DOMAINS", "CONFIG_DOMAIN_STACK_SIZE_KB"],
            "中断配置": ["CONFIG_MAX_IRQS", "CONFIG_IRQ_FAIRNESS"],
            "模块配置": ["CONFIG_MODULE_LOADING", "CONFIG_MAX_MODULES"]
        }
        
        for category, keys in categories.items():
            self.log(f"\n【{category}】", "INFO")
            for key in keys:
                if key in config:
                    value = config[key]
                    # 美化显示布尔值
                    if value in ['0', '1']:
                        display_value = "启用" if value == '1' else "禁用"
                    else:
                        display_value = value
                    self.log(f"  {key}: {display_value}", "INFO")
        
        self.log("\n" + "=" * 60)
        self.log("提示: 修改配置后需要重新编译才能生效", "INFO")
        self.log("使用 ./build.sh gui 可通过图形界面修改配置", "INFO")
    
    def show_runtime_config(self):
        """显示运行时配置说明"""
        runtime_config = self.config["root_dir"].parent / "runtime_config.yaml.example"
        
        if not runtime_config.exists():
            self.log("运行时配置示例文件不存在", "ERROR")
            return
        
        self.log("=" * 60)
        self.log("运行时配置说明")
        self.log("=" * 60)
        self.log(f"配置文件位置: {runtime_config}")
        self.log("")
        self.log("主要配置项:")
        self.log("  system:")
        self.log("    log_level: 日志级别 (error, warn, info, debug, trace)")
        self.log("    scheduler_policy: 调度策略 (fifo, rr, priority)")
        self.log("    memory_policy: 内存策略 (firstfit, bestfit, buddy)")
        self.log("    security_level: 安全级别 (minimal, standard, strict)")
        self.log("")
        self.log("  scheduler:")
        self.log("    time_slice_ms: 时间片长度(毫秒)")
        self.log("    max_threads: 最大线程数")
        self.log("")
        self.log("  memory:")
        self.log("    heap_size_mb: 堆大小(MB)")
        self.log("    stack_size_kb: 栈大小(KB)")
        self.log("    page_cache_percent: 页面缓存百分比")
        self.log("")
        self.log("  security:")
        self.log("    enable_secure_boot: 启用安全启动")
        self.log("    enable_kaslr: 启用KASLR")
        self.log("    enable_smap: 启用SMAP")
        self.log("    enable_smep: 启用SMEP")
        self.log("    enable_audit: 启用审计日志")
        self.log("")
        self.log("提示: 运行时配置无需重新编译，通过引导层传递给内核", "INFO")
        self.log("=" * 60)


def main():
    """主函数"""
    import argparse

    parser = argparse.ArgumentParser(
        description="HIK系统构建系统",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                    # 构建所有组件
  %(prog)s --target uefi      # 仅构建UEFI引导程序
  %(prog)s --target kernel    # 仅构建内核
  %(prog)s --target uefi bios # 构建UEFI和BIOS引导程序
  %(prog)s --clean            # 清理构建文件
  %(prog)s --config           # 显示当前编译配置
  %(prog)s --config-runtime   # 显示运行时配置说明
  %(prog)s --help             # 显示帮助
        """
    )

    parser.add_argument(
        "--target",
        nargs="+",
        choices=["uefi", "bios", "kernel"],
        help="构建目标 (uefi, bios, kernel)"
    )

    parser.add_argument(
        "--clean",
        action="store_true",
        help="清理构建文件"
    )
    
    parser.add_argument(
        "--config",
        action="store_true",
        help="显示当前编译配置"
    )
    
    parser.add_argument(
        "--config-runtime",
        action="store_true",
        help="显示运行时配置说明"
    )

    args = parser.parse_args()

    build_system = BuildSystem()

    # 显示配置模式
    if args.config:
        build_system.show_config()
        return 0
    
    if args.config_runtime:
        build_system.show_runtime_config()
        return 0

    # 清理模式
    if args.clean:
        build_system.log("清理构建文件...")
        import shutil
        if build_system.config["build_dir"].exists():
            shutil.rmtree(build_system.config["build_dir"])
        if build_system.config["output_dir"].exists():
            shutil.rmtree(build_system.config["output_dir"])

        # 清理子目录
        bootloader_dir = build_system.config["root_dir"] / "bootloader"
        kernel_dir = build_system.config["root_dir"] / "kernel"

        subprocess.run(["make", "clean"], cwd=bootloader_dir, capture_output=True)
        subprocess.run(["make", "clean"], cwd=kernel_dir, capture_output=True)

        build_system.log("清理完成")
        return 0

    # 构建模式
    if build_system.build(args.target):
        return 0
    else:
        return 1


if __name__ == "__main__":
    sys.exit(main())
