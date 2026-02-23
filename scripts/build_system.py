#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
HIC系统构建系统
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
from typing import Dict, List, Optional, Tuple, Any
from typing import Dict, List, Optional, Tuple

# 配置
# __file__ 是 scripts/build_system.py，所以 parent 是 scripts/，parent.parent 是项目根目录
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent

CONFIG = {
    "project": "HIC System",
    "version": "0.1.0",
    "root_dir": PROJECT_ROOT,
    "build_dir": PROJECT_ROOT / "build",
    "output_dir": PROJECT_ROOT / "output",
    "sign_key_file": "signing_key.pem",
    "sign_cert_file": "signing_cert.pem",
}

# 存储找到的Python环境路径
PYTHON_ENVS = {
    "qt": None,
    "gtk": None,
}


class BuildError(Exception):
    """构建错误"""
    pass


class BuildSystem:
    """HIC构建系统"""

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

    def check_dependencies(self, auto_install: bool = False) -> bool:
        """检查构建依赖"""
        self.log("检查构建依赖...")

        required_tools = ["gcc", "make", "objcopy", "objdump"]
        missing_tools = []

        for tool in required_tools:
            if not self._command_exists(tool):
                missing_tools.append(tool)

        if missing_tools:
            self.log(f"缺少依赖: {', '.join(missing_tools)}", "ERROR")
            
            # 如果启用自动安装
            if auto_install:
                self.log("尝试自动安装依赖...", "INFO")
                try:
                    import subprocess
                    result = subprocess.run(
                        ["python3", str(SCRIPT_DIR / "dependency_manager.py")],
                        cwd=PROJECT_ROOT,
                        check=True,
                        capture_output=True,
                        text=True
                    )
                    self.log("依赖自动安装完成", "SUCCESS")
                    
                    # 重新检查
                    missing_tools = [tool for tool in required_tools if not self._command_exists(tool)]
                    if missing_tools:
                        self.log(f"自动安装后仍缺少依赖: {', '.join(missing_tools)}", "ERROR")
                        return False
                except subprocess.CalledProcessError as e:
                    self.log(f"自动安装失败: {e.stderr}", "ERROR")
                    self.log("请手动运行: python3 scripts/dependency_manager.py", "INFO")
                    return False
            else:
                self.log("请运行: python3 scripts/dependency_manager.py 或 make deps", "INFO")
                return False

        # 检查UEFI工具
        if not self._command_exists("x86_64-w64-mingw32-gcc"):
            if auto_install:
                self.log("尝试自动安装 x86_64-w64-mingw32-gcc...", "INFO")
                try:
                    subprocess.run(
                        ["python3", str(SCRIPT_DIR / "dependency_manager.py"), "--install", "x86_64-w64-mingw32-gcc"],
                        cwd=PROJECT_ROOT,
                        check=True,
                        capture_output=True
                    )
                except:
                    pass
            self.log("警告: 未找到 x86_64-w64-mingw32-gcc，无法构建UEFI引导程序", "WARNING")

        # 检查BIOS工具
        if not self._command_exists("x86_64-elf-gcc"):
            if auto_install:
                self.log("尝试自动安装 x86_64-elf-gcc...", "INFO")
                try:
                    subprocess.run(
                        ["python3", str(SCRIPT_DIR / "dependency_manager.py"), "--install", "x86_64-elf-gcc"],
                        cwd=PROJECT_ROOT,
                        check=True,
                        capture_output=True
                    )
                except:
                    pass
            self.log("警告: 未找到 x86_64-elf-gcc，无法构建内核", "WARNING")

        # 检查签名工具
        if not self._command_exists("openssl"):
            if auto_install:
                self.log("尝试自动安装 openssl...", "INFO")
                try:
                    subprocess.run(
                        ["python3", str(SCRIPT_DIR / "dependency_manager.py"), "--install", "openssl"],
                        cwd=PROJECT_ROOT,
                        check=True,
                        capture_output=True
                    )
                except:
                    pass
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
                "-subj", f"/C=CN/ST=Beijing/O=HIC/CN=HIC-{self.config['version']}"
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

            output = kernel_dir / "bin" / "hic-kernel.elf"

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
            kernel = self.config["root_dir"] / "kernel" / "bin" / "hic-kernel.elf"

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
        kernel = self.config["root_dir"] / "bin" / "hic-kernel.elf"

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

    def build(self, targets: List[str] = None, auto_install_deps: bool = False) -> bool:
        """执行完整构建"""
        self.log("=" * 60)
        self.log(f"{self.config['project']} 构建系统 v{self.config['version']}")
        self.log("=" * 60)

        # 默认构建目标
        if targets is None:
            targets = ["uefi", "bios", "kernel"]

        # 检查依赖
        if not self.check_dependencies(auto_install=auto_install_deps):
            if not auto_install_deps:
                self.log("依赖检查失败，构建终止", "ERROR")
                self.log("提示: 运行 'python3 scripts/dependency_manager.py' 自动安装依赖", "INFO")
                self.log("或运行 'make deps' 自动安装所有依赖", "INFO")
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
            self.log(f"\n{category}", "INFO")
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


def check_interface_available(interface: str) -> bool:
    """检查指定的界面是否可用"""
    import subprocess
    import sys
    import os
    
    def try_import_in_env(module_name: str, env_python: str = None) -> bool:
        """尝试在指定Python环境中导入模块"""
        try:
            if env_python:
                # 使用指定Python环境
                result = subprocess.run(
                    [env_python, "-c", f"import {module_name}"],
                    capture_output=True,
                    timeout=5
                )
                return result.returncode == 0
            else:
                # 使用当前Python环境
                __import__(module_name)
                return True
        except:
            return False
    
    def has_display_environment() -> bool:
        """检查是否有显示环境"""
        # 检查X11或Wayland显示环境
        if os.environ.get('DISPLAY'):
            return True
        if os.environ.get('WAYLAND_DISPLAY'):
            return True
        if os.environ.get('XDG_SESSION_TYPE') in ['x11', 'wayland']:
            return True
        return False
    
    if interface == "qt":
        # Qt需要显示环境
        if not has_display_environment():
            return False
            
        # 尝试在多个Python环境中检测PyQt6
        pythons_to_try = [
            sys.executable,  # 当前Python
            "/usr/bin/python3",  # 系统Python3
            "/usr/bin/python",  # 系统python
            "/bin/python3",  # /bin/python3
        ]
        
        for py in pythons_to_try:
            if try_import_in_env("PyQt6", py):
                PYTHON_ENVS["qt"] = py  # 保存找到的Python环境
                return True
        return False
        
    elif interface == "gtk":
        # GTK需要显示环境
        if not has_display_environment():
            return False
            
        # 尝试在多个Python环境中检测GTK
        pythons_to_try = [
            sys.executable,
            "/usr/bin/python3",
            "/usr/bin/python",
            "/bin/python3",
        ]
        
        for py in pythons_to_try:
            if try_import_in_env("gi", py):
                result = subprocess.run(
                    [py, "-c", "import gi; gi.require_version('Gtk', '3.0')"],
                    capture_output=True,
                    timeout=5
                )
                if result.returncode == 0:
                    PYTHON_ENVS["gtk"] = py  # 保存找到的Python环境
                    return True
        return False
        
    elif interface == "tui":
        import curses
        import os
        # 只检查curses模块是否可用，不初始化
        # 实际初始化时如果失败会自动降级
        return True
    elif interface == "interactive":
        # 交互式CLI总是可用
        return True
    elif interface == "cli":
        return True  # CLI总是可用
    return False


def load_build_system_config() -> Dict[str, Any]:
    """从platform.yaml加载构建系统配置"""
    config_file = CONFIG["root_dir"] / "src" / "bootloader" / "platform.yaml"
    default_config = {
        "interface": {
            "primary": "auto",
            "fallback_chain": ["qt", "gtk", "tui", "interactive", "cli"],
            "auto_detect": True
        },
        "localization": {
            "language": "zh_CN",
            "auto_detect": True,
            "fallback_language": "en_US"
        },
        "presets": {
            "default": "balanced"
        }
    }
    
    if not config_file.exists():
        return default_config
    
    try:
        import yaml
        with open(config_file, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)
        
        if "build_system" in data:
            return {**default_config, **data["build_system"]}
        
        return default_config
    except Exception as e:
        print(f"警告: 加载构建配置失败: {e}，使用默认配置")
        return default_config


def detect_system_language() -> str:
    """检测系统语言"""
    import locale
    
    try:
        # 使用新的API（Python 3.11+）
        if hasattr(locale, 'getencoding'):
            import os
            sys_lang = os.environ.get('LANG', '').split('.')[0]
        else:
            # 旧API（向后兼容）
            sys_lang = locale.getdefaultlocale()[0]
        
        if sys_lang:
            # 映射系统语言到支持的语言
            lang_map = {
                'zh_CN': 'zh_CN',
                'zh_SG': 'zh_CN',
                'zh_TW': 'zh_CN',  # 可以扩展为zh_TW
                'zh_HK': 'zh_CN',
                'en_US': 'en_US',
                'en_GB': 'en_US',
                'en_AU': 'en_US',
                'en_CA': 'en_US',
                'en_NZ': 'en_US',
                'en_IE': 'en_US',
                'en_ZA': 'en_US',
                'en_IN': 'en_US',
                'ja_JP': 'ja_JP',
                'de_DE': 'de_DE',
                'de_AT': 'de_DE',
                'de_CH': 'de_DE',
                'de_BE': 'de_DE',
                'de_LU': 'de_DE',
                'de_LI': 'de_DE'
            }
            return lang_map.get(sys_lang, 'en_US')
    except:
        pass
    
    return 'en_US'


def launch_interface(interface: str, language: str, args) -> int:
    """启动指定的界面"""
    try:
        if interface == "qt":
            # 使用检测到的Python环境启动Qt GUI
            python_env = PYTHON_ENVS.get("qt", sys.executable)
            result = subprocess.run(
                [python_env, str(CONFIG["root_dir"] / "scripts" / "build_qt.py")] + args,
                cwd=CONFIG["root_dir"]
            )
            return result.returncode
        elif interface == "gtk":
            # 使用检测到的Python环境启动GTK GUI
            python_env = PYTHON_ENVS.get("gtk", sys.executable)
            result = subprocess.run(
                [python_env, str(CONFIG["root_dir"] / "scripts" / "build_gui.py")] + args,
                cwd=CONFIG["root_dir"]
            )
            return result.returncode
        elif interface == "tui":
            import curses
            from build_tui import main as tui_main
            sys.argv = [sys.argv[0]] + args
            try:
                return curses.wrapper(tui_main)
            except Exception as e:
                print(f"启动TUI界面失败: {e}")
                print("自动降级到交互式命令行界面...")
                return launch_interface("interactive", None, args)
        elif interface == "interactive":
            from build_interactive import main as interactive_main
            sys.argv = [sys.argv[0]] + args
            return interactive_main()
        elif interface == "cli":
            return cli_main(args)
    except Exception as e:
        print(f"启动{interface}界面失败: {e}")
        # 如果不是CLI或interactive界面，尝试降级到interactive
        if interface != "cli" and interface != "interactive":
            print("自动降级到交互式命令行界面...")
            return launch_interface("interactive", None, args)
        return 1
    
    return 0


def cli_main(args: List[str]) -> int:
    """命令行界面主函数"""
    import argparse

    parser = argparse.ArgumentParser(
        description="HIC系统构建系统 - 命令行模式",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                    # 启动交互式界面
  %(prog)s --target kernel    # 仅构建内核
  %(prog)s --target uefi bios # 构建UEFI和BIOS引导程序
  %(prog)s --clean            # 清理构建文件
  %(prog)s --config           # 显示当前编译配置
  %(prog)s --config-runtime   # 显示运行时配置说明
  %(prog)s --preset debug     # 使用debug预设配置
  %(prog)s --help             # 显示帮助
        """
    )

    parser.add_argument(
        "--target",
        nargs="+",
        choices=["uefi", "bios", "kernel", "all"],
        help="构建目标 (uefi, bios, kernel, all)"
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

    parser.add_argument(
        "--preset",
        choices=["balanced", "release", "debug", "minimal", "performance"],
        help="使用预设配置"
    )

    parser.add_argument(
        "--auto-deps",
        action="store_true",
        help="自动安装缺失的依赖"
    )

    parser.add_argument(
        "--install",
        action="store_true",
        help="构建后自动安装"
    )

    parsed_args = parser.parse_args(args)

    # 如果没有参数，启动交互式界面
    if len(args) == 0 or (len(args) == 1 and args[0] == ['--help']):
        # 启动交互式界面
        try:
            return launch_interface("interactive", "zh_CN", [])
        except:
            # 如果交互式界面失败，显示帮助
            parser.print_help()
            return 1

    build_system = BuildSystem()

    # 清理模式
    if parsed_args.clean:
        build_system.log("清理构建文件...")
        import shutil
        if build_system.config["build_dir"].exists():
            shutil.rmtree(build_system.config["build_dir"])
        if build_system.config["output_dir"].exists():
            shutil.rmtree(build_system.config["output_dir"])

        subprocess.run(["make", "clean"], cwd=ROOT_DIR, capture_output=True)

        build_system.log("清理完成")
        return 0

    # 显示配置模式
    if parsed_args.config:
        build_system.show_config()
        return 0
    
    if parsed_args.config_runtime:
        build_system.show_runtime_config()
        return 0

    # 使用预设配置
    if parsed_args.preset:
        print(f"应用预设配置: {parsed_args.preset}")
        success = run_command(["make", f"build-{parsed_args.preset}"])
        if parsed_args.install and success:
            run_command(["make", "install"])
        return 0 if success else 1

    # 构建模式
    targets = parsed_args.target if parsed_args.target else []

    if not targets:
        # 默认构建全部
        if parsed_args.auto_deps:
            build_system = BuildSystem()
            success = build_system.build(auto_install_deps=True)
        else:
            success = run_command(["make", "all"])
    else:
        # 根据目标构建
        if "all" in targets:
            success = run_command(["make", "all"])
        else:
            success = True
            for target in targets:
                if target == "uefi":
                    if not run_command(["make", "bootloader"]):
                        success = False
                elif target == "bios":
                    if not run_command(["make", "bootloader"]):
                        success = False
                elif target == "kernel":
                    if not run_command(["make", "kernel"]):
                        success = False
    
    # 自动安装
    if parsed_args.install and success:
        run_command(["make", "install"])
    
    return 0 if success else 1


def run_command(command: List[str], description: str = "") -> bool:
    """运行命令"""
    import subprocess
    
    if description:
        print(f"{description}...")
    
    print(f"执行: {' '.join(command)}")
    
    result = subprocess.run(
        command,
        cwd=ROOT_DIR,
        text=True
    )
    
    return result.returncode == 0


def main():
    """主函数 - 支持界面自动选择和降级"""
    import argparse
    
    # 解析基础参数（界面选择相关）
    base_parser = argparse.ArgumentParser(
        description="HIC系统构建系统",
        add_help=False
    )
    
    base_parser.add_argument(
        "--interface",
        choices=["auto", "qt", "gtk", "tui", "interactive", "cli"],
        default="auto",
        help="指定界面类型 (默认: auto)"
    )
    
    base_parser.add_argument(
        "--language",
        choices=["zh_CN", "en_US", "ja_JP", "de_DE"],
        default=None,
        help="指定语言 (默认: 自动检测)"
    )
    
    base_args, remaining_args = base_parser.parse_known_args()
    
    # 加载构建系统配置
    build_config = load_build_system_config()
    
    # 确定语言
    if base_args.language:
        language = base_args.language
    elif build_config["localization"]["auto_detect"]:
        language = detect_system_language()
    else:
        language = build_config["localization"]["language"]
    
    # 确定界面
    if base_args.interface == "auto":
        interface = build_config["interface"]["primary"]
        if interface == "auto" or not build_config["interface"]["auto_detect"]:
            interface = "auto"  # 启用自动检测
    else:
        interface = base_args.interface
    
    # 自动检测或使用指定界面
    if interface == "auto":
        fallback_chain = build_config["interface"]["fallback_chain"]
        # 优先尝试GUI界面，因为它们更友好
        for iface in fallback_chain:
            if check_interface_available(iface):
                print(f"使用 {iface.upper()} 界面")
                try:
                    return launch_interface(iface, language, remaining_args)
                except Exception as e:
                    print(f"启动 {iface.upper()} 界面失败: {e}")
                    print("尝试下一个可用界面...")
                    continue
        # 所有界面都不可用，使用交互式CLI（不是纯CLI）
        print("未检测到可用的图形界面，使用交互式命令行界面")
        return launch_interface("interactive", language, remaining_args)
    else:
        # 使用指定的界面
        if check_interface_available(interface):
            print(f"使用 {interface.upper()} 界面")
            try:
                return launch_interface(interface, language, remaining_args)
            except Exception as e:
                print(f"启动 {interface.upper()} 界面失败: {e}")
                # 如果是指定界面失败，不自动降级，返回错误
                return 1
        else:
            print(f"警告: {interface.upper()} 界面不可用，尝试降级...")
            fallback_chain = build_config["interface"]["fallback_chain"]
            for iface in fallback_chain:
                if iface != interface and check_interface_available(iface):
                    print(f"降级到 {iface.upper()} 界面")
                    try:
                        return launch_interface(iface, language, remaining_args)
                    except Exception as e:
                        print(f"启动 {iface.upper()} 界面失败: {e}")
                        continue
            # 所有界面都不可用，使用交互式CLI
            print("所有图形界面都不可用，使用交互式命令行界面")
            return launch_interface("interactive", language, remaining_args)


if __name__ == "__main__":
    sys.exit(main())
