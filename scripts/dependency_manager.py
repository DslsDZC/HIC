#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 跨平台依赖管理器
支持 Linux、macOS、Windows
自动检测和安装构建依赖，无需 Docker
"""

import os
import sys
import platform
import subprocess
import urllib.request
import tarfile
import zipfile
import shutil
import hashlib
import json
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from enum import Enum

# 配置
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
TOOLCHAINS_DIR = PROJECT_ROOT / ".toolchains"
TOOLS_CACHE = TOOLCHAINS_DIR / "cache"
TOOLCHAINS_INSTALL = TOOLCHAINS_DIR / "install"

class Platform(Enum):
    """支持的平台"""
    LINUX = "linux"
    MACOS = "macos"
    WINDOWS = "windows"
    UNKNOWN = "unknown"

class PackageManager(Enum):
    """包管理器类型"""
    PACMAN = "pacman"          # Arch Linux
    APT = "apt"                # Debian/Ubuntu
    DNF = "dnf"                # Fedora
    BREW = "brew"              # macOS Homebrew
    CHOCO = "choco"            # Windows Chocolatey
    APT_GET = "apt-get"        # Debian/Ubuntu (备用)
    YUM = "yum"                # CentOS/RHEL (备用)
    NONE = "none"

class DependencyManager:
    """跨平台依赖管理器"""
    
    def __init__(self):
        self.platform = self._detect_platform()
        self.package_manager = self._detect_package_manager()
        self.system_arch = platform.machine().lower()
        
        # 创建工具链目录
        TOOLCHAINS_DIR.mkdir(parents=True, exist_ok=True)
        TOOLS_CACHE.mkdir(parents=True, exist_ok=True)
        TOOLCHAINS_INSTALL.mkdir(parents=True, exist_ok=True)
        
        # 加载工具链清单
        self.toolchain_manifest = self._load_toolchain_manifest()
    
    def log(self, message: str, level: str = "INFO"):
        """记录日志"""
        prefix = {
            "INFO": "✓",
            "WARNING": "⚠",
            "ERROR": "✗",
            "SUCCESS": "★",
            "PROGRESS": "▶"
        }.get(level, "•")
        print(f"{prefix} {message}")
    
    def _detect_platform(self) -> Platform:
        """检测平台"""
        system = platform.system().lower()
        if system == "linux":
            return Platform.LINUX
        elif system == "darwin":
            return Platform.MACOS
        elif system == "windows":
            return Platform.WINDOWS
        return Platform.UNKNOWN
    
    def _detect_package_manager(self) -> PackageManager:
        """检测包管理器"""
        managers = []
        
        if self.platform == Platform.LINUX:
            managers = [PackageManager.PACMAN, PackageManager.APT, PackageManager.DNF, 
                       PackageManager.APT_GET, PackageManager.YUM]
        elif self.platform == Platform.MACOS:
            managers = [PackageManager.BREW]
        elif self.platform == Platform.WINDOWS:
            managers = [PackageManager.CHOCO]
        
        for pm in managers:
            if self._command_exists(pm.value):
                return pm
        
        return PackageManager.NONE
    
    def _command_exists(self, command: str) -> bool:
        """检查命令是否存在"""
        try:
            if self.platform == Platform.WINDOWS:
                result = subprocess.run(
                    ["where", command],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
            else:
                result = subprocess.run(
                    ["which", command],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
            return result.returncode == 0
        except:
            return False
    
    def _load_toolchain_manifest(self) -> Dict:
        """加载工具链清单"""
        manifest_file = SCRIPT_DIR / "toolchain_manifest.json"
        
        if not manifest_file.exists():
            self.log("创建默认工具链清单", "INFO")
            self._create_default_manifest(manifest_file)
        
        try:
            with open(manifest_file, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception as e:
            self.log(f"加载工具链清单失败: {e}", "ERROR")
            return {}
    
    def _create_default_manifest(self, manifest_file: Path):
        """创建默认工具链清单"""
        manifest = {
            "x86_64-elf-gcc": {
                "name": "x86_64-elf GCC工具链",
                "version": "13.2.0",
                "sources": {
                    "linux": {
                        "pacman": ["cross-x86_64-elf-gcc", "cross-x86_64-elf-binutils"],
                        "apt": ["gcc-x86-64-elf-binutils", "gcc-x86-64-elf"],
                        "dnf": ["x86_64-elf-gcc", "x86_64-elf-binutils"],
                        "apt-get": ["gcc-x86-64-elf-binutils", "gcc-x86-64-elf"],
                        "yum": ["x86_64-elf-gcc", "x86_64-elf-binutils"],
                        "source": {
                            "url": "https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz",
                            "sha256": "e275e76493a0b074f43cda6e4c8b48b4403939863cf3fe4e5b06a2f8e5950d9d",
                            "binutils_url": "https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz",
                            "binutils_sha256": "ae9a5789e2544f8246294eef6cee9ac32aaabf2f5204b8aa816d8a5b467e3d8a"
                        }
                    },
                    "macos": {
                        "brew": ["x86_64-elf-gcc"],
                        "source": {
                            "url": "https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz",
                            "sha256": "e275e76493a0b074f43cda6e4c8b48b4403939863cf3fe4e5b06a2f8e5950d9d",
                            "binutils_url": "https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz",
                            "binutils_sha256": "ae9a5789e2544f8246294eef6cee9ac32aaabf2f5204b8aa816d8a5b467e3d8a"
                        }
                    },
                    "windows": {
                        "choco": ["mingw-w64-x86_64-toolchain"],
                        "source": {
                            "url": "https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz",
                            "sha256": "e275e76493a0b074f43cda6e4c8b48b4403939863cf3fe4e5b06a2f8e5950d9d",
                            "binutils_url": "https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz",
                            "binutils_sha256": "ae9a5789e2544f8246294eef6cee9ac32aaabf2f5204b8aa816d8a5b467e3d8a"
                        }
                    }
                }
            },
            "x86_64-w64-mingw32-gcc": {
                "name": "MinGW-w64 工具链",
                "version": "13.2.0",
                "sources": {
                    "linux": {
                        "pacman": ["mingw-w64-gcc", "mingw-w64-binutils", "mingw-w64-headers"],
                        "apt": ["gcc-mingw-w64-x86-64", "g++-mingw-w64-x86-64"],
                        "dnf": ["mingw64-gcc", "mingw64-binutils"],
                        "source": {
                            "url": "https://github.com/mirror/mingw-w64/releases/download/v11.0.0/mingw-w64-v11.0.0.tar.bz2",
                            "sha256": "a7569a6d3f9c4b5d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6"
                        }
                    },
                    "macos": {
                        "brew": ["mingw-w64"],
                        "source": {
                            "url": "https://github.com/mirror/mingw-w64/releases/download/v11.0.0/mingw-w64-v11.0.0.tar.bz2",
                            "sha256": "a7569a6d3f9c4b5d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6"
                        }
                    },
                    "windows": {
                        "choco": ["mingw"],
                        "source": {
                            "url": "https://github.com/mirror/mingw-w64/releases/download/v11.0.0/mingw-w64-v11.0.0.tar.bz2",
                            "sha256": "a7569a6d3f9c4b5d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6d5a6"
                        }
                    }
                }
            },
            "gcc": {
                "name": "GNU GCC",
                "version": "13.2.0",
                "sources": {
                    "linux": {
                        "pacman": ["gcc", "gcc-libs"],
                        "apt": ["build-essential", "gcc"],
                        "dnf": ["gcc", "gcc-c++"]
                    },
                    "macos": {
                        "brew": ["gcc"]
                    },
                    "windows": {
                        "choco": ["gcc"]
                    }
                }
            },
            "make": {
                "name": "GNU Make",
                "version": "4.4.1",
                "sources": {
                    "linux": {
                        "pacman": ["make"],
                        "apt": ["build-essential"],
                        "dnf": ["make"]
                    },
                    "macos": {
                        "brew": ["make"]
                    },
                    "windows": {
                        "choco": ["make"]
                    }
                }
            },
            "python3": {
                "name": "Python 3",
                "version": "3.12",
                "sources": {
                    "linux": {
                        "pacman": ["python", "python-pip"],
                        "apt": ["python3", "python3-pip"],
                        "dnf": ["python3", "python3-pip"]
                    },
                    "macos": {
                        "brew": ["python"]
                    },
                    "windows": {
                        "choco": ["python"]
                    }
                }
            },
            "nasm": {
                "name": "NASM 汇编器",
                "version": "2.16",
                "sources": {
                    "linux": {
                        "pacman": ["nasm"],
                        "apt": ["nasm"],
                        "dnf": ["nasm"]
                    },
                    "macos": {
                        "brew": ["nasm"]
                    },
                    "windows": {
                        "choco": ["nasm"]
                    }
                }
            },
            "qemu-system-x86_64": {
                "name": "QEMU",
                "version": "8.2",
                "sources": {
                    "linux": {
                        "pacman": ["qemu-system-x86", "ovmf"],
                        "apt": ["qemu-system-x86", "ovmf"],
                        "dnf": ["qemu-system-x86", "edk2-ovmf"]
                    },
                    "macos": {
                        "brew": ["qemu"]
                    },
                    "windows": {
                        "choco": ["qemu"]
                    }
                }
            },
            "openssl": {
                "name": "OpenSSL",
                "version": "3.0",
                "sources": {
                    "linux": {
                        "pacman": ["openssl"],
                        "apt": ["openssl"],
                        "dnf": ["openssl"]
                    },
                    "macos": {
                        "brew": ["openssl"]
                    },
                    "windows": {
                        "choco": ["openssl"]
                    }
                }
            },
            "git": {
                "name": "Git",
                "version": "2.43",
                "sources": {
                    "linux": {
                        "pacman": ["git"],
                        "apt": ["git"],
                        "dnf": ["git"]
                    },
                    "macos": {
                        "brew": ["git"]
                    },
                    "windows": {
                        "choco": ["git"]
                    }
                }
            }
        }
        
        with open(manifest_file, 'w', encoding='utf-8') as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)
    
    def check_dependencies(self, tools: List[str] = None) -> Dict[str, bool]:
        """检查依赖是否已安装"""
        if tools is None:
            tools = ["gcc", "make", "python3", "nasm", "qemu-system-x86_64", 
                     "openssl", "git", "x86_64-w64-mingw32-gcc", "x86_64-elf-gcc"]
        
        results = {}
        for tool in tools:
            results[tool] = self._command_exists(tool)
        
        return results
    
    def install_dependencies(self, tools: List[str] = None, force: bool = False) -> bool:
        """安装依赖"""
        self.log("=" * 60, "INFO")
        self.log(f"平台: {self.platform.value.upper()}", "INFO")
        self.log(f"包管理器: {self.package_manager.value.upper()}", "INFO")
        self.log(f"架构: {self.system_arch}", "INFO")
        self.log("=" * 60, "INFO")
        
        if tools is None:
            tools = ["gcc", "make", "python3", "nasm", "qemu-system-x86_64", 
                     "openssl", "git", "x86_64-w64-mingw32-gcc", "x86_64-elf-gcc"]
        
        # 检查已安装的依赖
        self.log("检查已安装的依赖...", "PROGRESS")
        installed = self.check_dependencies(tools)
        
        missing = [tool for tool, exists in installed.items() if not exists]
        
        if not missing:
            self.log("所有依赖已安装!", "SUCCESS")
            return True
        
        self.log(f"需要安装 {len(missing)} 个依赖", "INFO")
        for tool in missing:
            self.log(f"  - {tool}", "INFO")
        
        # 安装缺失的依赖
        success = True
        for tool in missing:
            self.log(f"\n安装 {tool}...", "PROGRESS")
            if not self._install_tool(tool, force):
                self.log(f"安装 {tool} 失败", "ERROR")
                success = False
        
        if success:
            self.log("\n所有依赖安装完成!", "SUCCESS")
        else:
            self.log("\n部分依赖安装失败，请检查错误信息", "WARNING")
        
        return success
    
    def _install_tool(self, tool: str, force: bool = False) -> bool:
        """安装单个工具"""
        if tool not in self.toolchain_manifest:
            self.log(f"未找到工具 {tool} 的安装信息", "ERROR")
            return False
        
        tool_info = self.toolchain_manifest[tool]
        
        # 检查是否已安装
        if not force and self._command_exists(tool):
            self.log(f"{tool} 已安装", "SUCCESS")
            return True
        
        # 尝试使用包管理器安装
        if self.package_manager != PackageManager.NONE:
            if self._install_with_package_manager(tool, tool_info):
                return True
        
        # 包管理器失败，尝试从源码安装
        self.log(f"包管理器安装失败，尝试从源码编译 {tool}...", "INFO")
        return self._install_from_source(tool, tool_info)
    
    def _install_with_package_manager(self, tool: str, tool_info: Dict) -> bool:
        """使用包管理器安装"""
        platform_key = self.platform.value
        pm_key = self.package_manager.value
        
        if platform_key not in tool_info["sources"]:
            self.log(f"平台 {platform_key} 不支持", "ERROR")
            return False
        
        platform_sources = tool_info["sources"][platform_key]
        
        if pm_key not in platform_sources:
            self.log(f"包管理器 {pm_key} 不支持此工具", "WARNING")
            return False
        
        packages = platform_sources[pm_key]
        
        self.log(f"使用 {self.package_manager.value} 安装: {', '.join(packages)}", "INFO")
        
        try:
            if self.package_manager == PackageManager.PACMAN:
                cmd = ["sudo", "pacman", "-S", "--needed", "--noconfirm"] + packages
            elif self.package_manager in [PackageManager.APT, PackageManager.APT_GET]:
                cmd = ["sudo", "apt-get", "install", "-y"] + packages
            elif self.package_manager == PackageManager.DNF:
                cmd = ["sudo", "dnf", "install", "-y"] + packages
            elif self.package_manager == PackageManager.BREW:
                cmd = ["brew", "install"] + packages
            elif self.package_manager == PackageManager.CHOCO:
                cmd = ["choco", "install", "-y"] + packages
            elif self.package_manager == PackageManager.YUM:
                cmd = ["sudo", "yum", "install", "-y"] + packages
            else:
                return False
            
            result = subprocess.run(cmd, check=True, capture_output=True, text=True)
            self.log(f"安装成功", "SUCCESS")
            return True
        except subprocess.CalledProcessError as e:
            self.log(f"安装失败: {e.stderr}", "ERROR")
            return False
    
    def _install_from_source(self, tool: str, tool_info: Dict) -> bool:
        """从源码安装工具"""
        platform_key = self.platform.value
        
        if platform_key not in tool_info["sources"]:
            self.log(f"平台 {platform_key} 不支持源码安装", "ERROR")
            return False
        
        platform_sources = tool_info["sources"][platform_key]
        
        if "source" not in platform_sources:
            self.log(f"没有源码安装信息", "ERROR")
            return False
        
        source_info = platform_sources["source"]
        
        # GCC 和 binutils 需要特殊处理
        if tool == "x86_64-elf-gcc":
            return self._build_x86_64_elf_gcc(source_info)
        else:
            return self._download_and_extract(source_info["url"], source_info["sha256"])
    
    def _download_and_extract(self, url: str, expected_sha256: str) -> bool:
        """下载并解压文件"""
        filename = url.split("/")[-1]
        cache_file = TOOLS_CACHE / filename
        
        # 检查缓存
        if cache_file.exists():
            self.log(f"使用缓存: {filename}", "INFO")
        else:
            self.log(f"下载 {url}...", "PROGRESS")
            try:
                urllib.request.urlretrieve(url, cache_file)
            except Exception as e:
                self.log(f"下载失败: {e}", "ERROR")
                return False
        
        # 验证哈希
        self.log("验证文件完整性...", "PROGRESS")
        sha256 = self._calculate_file_hash(cache_file)
        if sha256 != expected_sha256.lower():
            self.log(f"哈希不匹配: {sha256} != {expected_sha256}", "ERROR")
            cache_file.unlink()
            return False
        
        # 解压
        self.log("解压文件...", "PROGRESS")
        extract_dir = TOOLCHAINS_INSTALL / filename.replace(".tar.xz", "").replace(".tar.gz", "").replace(".tar.bz2", "")
        
        if extract_dir.exists():
            shutil.rmtree(extract_dir)
        
        try:
            if filename.endswith(".tar.xz") or filename.endswith(".tar.gz") or filename.endswith(".tar.bz2"):
                with tarfile.open(cache_file) as tar:
                    tar.extractall(TOOLCHAINS_INSTALL)
            elif filename.endswith(".zip"):
                with zipfile.ZipFile(cache_file) as zf:
                    zf.extractall(TOOLCHAINS_INSTALL)
            
            self.log(f"解压完成: {extract_dir}", "SUCCESS")
            return True
        except Exception as e:
            self.log(f"解压失败: {e}", "ERROR")
            return False
    
    def _calculate_file_hash(self, filepath: Path) -> str:
        """计算文件 SHA256 哈希"""
        sha256_hash = hashlib.sha256()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                sha256_hash.update(chunk)
        return sha256_hash.hexdigest()
    
    def _build_x86_64_elf_gcc(self, source_info: Dict) -> bool:
        """构建 x86_64-elf-gcc 工具链"""
        self.log("构建 x86_64-elf-gcc 工具链（这可能需要很长时间）...", "PROGRESS")
        
        install_prefix = TOOLCHAINS_INSTALL / "x86_64-elf"
        install_prefix.mkdir(parents=True, exist_ok=True)
        
        # 下载并解压 binutils
        if not self._download_and_extract(source_info["binutils_url"], source_info["binutils_sha256"]):
            return False
        
        binutils_dir = TOOLCHAINS_INSTALL / f"binutils-2.41"
        binutils_build = TOOLCHAINS_INSTALL / "binutils-build"
        
        # 构建 binutils
        self.log("构建 binutils...", "PROGRESS")
        if binutils_build.exists():
            shutil.rmtree(binutils_build)
        binutils_build.mkdir()
        
        try:
            subprocess.run([
                f"{binutils_dir}/configure",
                f"--target=x86_64-elf",
                f"--prefix={install_prefix}",
                "--with-sysroot",
                "--disable-nls",
                "--disable-werror"
            ], cwd=binutils_build, check=True)
            
            subprocess.run(["make", "-j4"], cwd=binutils_build, check=True)
            subprocess.run(["make", "install"], cwd=binutils_build, check=True)
            
            self.log("binutils 构建完成", "SUCCESS")
        except subprocess.CalledProcessError as e:
            self.log(f"binutils 构建失败: {e}", "ERROR")
            return False
        
        # 下载并解压 GCC
        if not self._download_and_extract(source_info["url"], source_info["sha256"]):
            return False
        
        gcc_dir = TOOLCHAINS_INSTALL / "gcc-13.2.0"
        gcc_build = TOOLCHAINS_INSTALL / "gcc-build"
        
        # 构建 GCC
        self.log("构建 GCC...", "PROGRESS")
        if gcc_build.exists():
            shutil.rmtree(gcc_build)
        gcc_build.mkdir()
        
        try:
            # 添加 binutils 到 PATH
            env = os.environ.copy()
            env["PATH"] = f"{install_prefix}/bin:{env.get('PATH', '')}"
            
            subprocess.run([
                f"{gcc_dir}/configure",
                f"--target=x86_64-elf",
                f"--prefix={install_prefix}",
                "--disable-nls",
                "--enable-languages=c,c++",
                "--without-headers"
            ], cwd=gcc_build, check=True, env=env)
            
            subprocess.run(["make", "all-gcc", "-j4"], cwd=gcc_build, check=True, env=env)
            subprocess.run(["make", "install-gcc", "-j4"], cwd=gcc_build, check=True, env=env)
            
            self.log("GCC 构建完成", "SUCCESS")
            
            # 创建符号链接
            self._create_toolchain_symlinks(install_prefix)
            
            return True
        except subprocess.CalledProcessError as e:
            self.log(f"GCC 构建失败: {e}", "ERROR")
            return False
    
    def _create_toolchain_symlinks(self, install_prefix: Path):
        """创建工具链符号链接"""
        bin_dir = install_prefix / "bin"
        if not bin_dir.exists():
            return
        
        self.log("创建工具链符号链接...", "PROGRESS")
        
        for tool in ["gcc", "g++", "ld", "as", "ar", "ranlib", "objcopy", "objdump", "strip"]:
            src = bin_dir / f"x86_64-elf-{tool}"
            dst = bin_dir / f"x86_64-elf-{tool}"
            if src.exists() and not dst.exists():
                try:
                    if self.platform != Platform.WINDOWS:
                        dst.symlink_to(src)
                except:
                    pass
        
        # 添加到 .toolchains/env.sh
        env_file = TOOLCHAINS_DIR / "env.sh"
        with open(env_file, 'w') as f:
            f.write(f"export PATH=\"{bin_dir}:$PATH\"\n")
            f.write(f"export CC=\"{bin_dir}/x86_64-elf-gcc\"\n")
        
        self.log(f"环境配置已保存到: {env_file}", "SUCCESS")
    
    def get_env_setup_command(self) -> str:
        """获取环境设置命令"""
        env_file = TOOLCHAINS_DIR / "env.sh"
        if env_file.exists():
            return f"source {env_file}"
        return ""

def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="HIC 跨平台依赖管理器",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                    # 检查并安装所有依赖
  %(prog)s --check            # 仅检查依赖
  %(prog)s --install gcc make # 仅安装指定工具
  %(prog)s --force            # 强制重新安装
  %(prog)s --env              # 显示环境设置命令
        """
    )
    
    parser.add_argument(
        "--check",
        action="store_true",
        help="仅检查依赖，不安装"
    )
    
    parser.add_argument(
        "--install",
        nargs="+",
        help="指定要安装的工具"
    )
    
    parser.add_argument(
        "--force",
        action="store_true",
        help="强制重新安装"
    )
    
    parser.add_argument(
        "--env",
        action="store_true",
        help="显示环境设置命令"
    )
    
    args = parser.parse_args()
    
    dm = DependencyManager()
    
    if args.env:
        cmd = dm.get_env_setup_command()
        if cmd:
            print(cmd)
        else:
            print("无需设置环境变量")
        return 0
    
    if args.check:
        results = dm.check_dependencies(args.install)
        print("\n依赖检查结果:")
        for tool, exists in results.items():
            status = "✓ 已安装" if exists else "✗ 未安装"
            print(f"  {tool}: {status}")
        return 0
    
    success = dm.install_dependencies(args.install, args.force)
    
    if success:
        env_cmd = dm.get_env_setup_command()
        if env_cmd:
            print(f"\n{env_cmd}")
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
