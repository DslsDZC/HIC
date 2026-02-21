#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
HIC系统构建系统 - Qt GUI模式
使用PyQt6实现现代化图形用户界面
支持多语言、主题切换、配置预设
"""

import sys
import os
import subprocess
import threading
import json
from pathlib import Path
from typing import Dict, List, Optional, Any
from datetime import datetime

# 检查PyQt6是否可用
try:
    from PyQt6.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QPushButton, QLabel, QTextEdit, QProgressBar, QTabWidget,
        QGroupBox, QFormLayout, QComboBox, QSpinBox, QCheckBox,
        QSlider, QListWidget, QSplitter, QStatusBar, QMenuBar,
        QToolBar, QFileDialog, QMessageBox, QDialog, QDialogButtonBox
    )
    from PyQt6.QtCore import Qt, QThread, pyqtSignal, QTimer, QSize
    from PyQt6.QtGui import QAction, QIcon, QPalette, QColor, QFont
except ImportError:
    print("错误: 缺少 PyQt6 库")
    print("请安装以下依赖:")
    print("  pip install PyQt6")
    print("或使用系统包管理器:")
    print("  Arch Linux: sudo pacman -S python-pyqt6")
    print("  Ubuntu/Debian: sudo apt-get install python3-pyqt6")
    print("  Fedora: sudo dnf install python3-pyqt6")
    sys.exit(1)

# 项目信息
PROJECT = "HIC System"
VERSION = "0.1.0"
ROOT_DIR = Path(__file__).parent.parent
BUILD_DIR = ROOT_DIR / "build"
OUTPUT_DIR = ROOT_DIR / "output"
PLATFORM_YAML = ROOT_DIR / "src" / "bootloader" / "platform.yaml"

# 翻译加载函数
def load_translations():
    """从translations文件夹加载翻译"""
    translations_dir = Path(__file__).parent / "translations"
    
    try:
        import yaml
        
        # 加载翻译键
        keys_file = translations_dir / "_keys.yaml"
        if keys_file.exists():
            with open(keys_file, 'r', encoding='utf-8') as f:
                keys_data = yaml.safe_load(f)
                language_keys = {}
                for key in keys_data.get('language_keys', []):
                    language_keys[key] = key
        else:
            language_keys = {}
        
        # 加载语言显示名称
        display_names_file = translations_dir / "_display_names.yaml"
        if display_names_file.exists():
            with open(display_names_file, 'r', encoding='utf-8') as f:
                display_names_data = yaml.safe_load(f)
                language_display_names = display_names_data.get('language_display_names', {})
        else:
            language_display_names = {}
        
        # 加载所有语言文件
        I18N = {}
        for lang_file in translations_dir.glob("*.yaml"):
            # 跳过配置文件
            if lang_file.name.startswith('_'):
                continue
            
            lang_code = lang_file.stem  # 文件名就是语言代码
            with open(lang_file, 'r', encoding='utf-8') as f:
                translations = yaml.safe_load(f)
                I18N[lang_code] = translations
        
        if I18N:
            return I18N, language_keys, language_display_names
            
    except Exception as e:
        print(f"警告: 加载翻译文件失败 ({e}), 使用默认英语翻译")
    
    # 如果加载失败，返回基本的英语翻译
    return {"en_US": {}}, {}, {}

# 加载翻译
I18N, LANGUAGE_KEYS, LANGUAGE_DISPLAY_NAMES = load_translations()

class BuildThread(QThread):
    """构建线程"""
    update_signal = pyqtSignal(str)
    progress_signal = pyqtSignal(int)
    finished_signal = pyqtSignal(int, str)  # exit_code, message
    
    def __init__(self, command: List[str], cwd: Path):
        super().__init__()
        self.command = command
        self.cwd = cwd
        self.process: Optional[subprocess.Popen] = None
        self.running = True
    
    def run(self):
        """运行构建命令"""
        try:
            self.process = subprocess.Popen(
                self.command,
                cwd=self.cwd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            while self.process.poll() is None and self.running:
                line = self.process.stdout.readline()
                if line:
                    self.update_signal.emit(line.rstrip())
            
            exit_code = self.process.returncode if self.process else -1
            message = I18N[self.get_language()]["build_success"] if exit_code == 0 else I18N[self.get_language()]["build_failed"]
            self.finished_signal.emit(exit_code, message)
            
        except Exception as e:
            self.update_signal.emit(f"错误: {str(e)}")
            self.finished_signal.emit(-1, str(e))
    
    def stop(self):
        """停止构建"""
        self.running = False
        if self.process:
            self.process.terminate()
    
    def get_language(self) -> str:
        """获取当前语言"""
        # 简单实现，实际应该从配置读取
        return "zh_CN"


class HICBuildGUI(QMainWindow):
    """HIC构建系统主窗口"""
    
    def __init__(self):
        super().__init__()
        self.current_language = "zh_CN"
        self.current_theme = "dark"
        self.current_preset = "balanced"
        self.build_thread: Optional[BuildThread] = None
        self.config_data: Dict[str, Any] = {}
        
        self.init_ui()
        self.load_config()
        self.apply_theme()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle(self._("title"))
        self.setMinimumSize(1200, 800)
        
        # 创建菜单栏
        self.create_menu_bar()
        # 保存菜单引用以便重新翻译
        self.menu_bar_items = {}
        self._collect_menu_items()
        
        # 创建工具栏
        self.create_tool_bar()
        
        # 创建中央部件
        self.create_central_widget()
        
        # 创建状态栏
        self.create_status_bar()
    
    def _collect_menu_items(self):
        """收集菜单项以便重新翻译"""
        menubar = self.menuBar()
        
        # 文件菜单
        file_menu = menubar.actions()[0].menu()
        self.menu_bar_items['file_menu'] = file_menu
        self.menu_bar_items['file_menu_actions'] = list(file_menu.actions())
        
        # 编辑菜单
        edit_menu = menubar.actions()[1].menu()
        self.menu_bar_items['edit_menu'] = edit_menu
        self.menu_bar_items['edit_menu_actions'] = list(edit_menu.actions())
        
        # 视图菜单
        view_menu = menubar.actions()[2].menu()
        self.menu_bar_items['view_menu'] = view_menu
        self.menu_bar_items['view_menu_actions'] = list(view_menu.actions())
        
        # 构建菜单
        build_menu = menubar.actions()[3].menu()
        self.menu_bar_items['build_menu'] = build_menu
        self.menu_bar_items['build_menu_actions'] = list(build_menu.actions())
        
        # 帮助菜单
        help_menu = menubar.actions()[4].menu()
        self.menu_bar_items['help_menu'] = help_menu
        self.menu_bar_items['help_menu_actions'] = list(help_menu.actions())
    
    def create_menu_bar(self):
        """创建菜单栏"""
        menubar = self.menuBar()
        
        # 文件菜单
        file_menu = menubar.addMenu(self._("file"))
        self.new_profile_action = file_menu.addAction(self._("new_profile"), self.new_profile)
        self.open_profile_action = file_menu.addAction(self._("open_profile"), self.open_profile)
        self.save_profile_action = file_menu.addAction(self._("save_profile"), self.save_profile)
        file_menu.addSeparator()
        self.export_config_action = file_menu.addAction(self._("export_config"), self.export_config)
        self.import_config_action = file_menu.addAction(self._("import_config"), self.import_config)
        file_menu.addSeparator()
        self.preferences_action = file_menu.addAction(self._("preferences"), self.preferences)
        file_menu.addSeparator()
        self.exit_action = file_menu.addAction(self._("exit"), self.close)
        
        # 编辑菜单
        edit_menu = menubar.addMenu(self._("edit"))
        self.undo_action = edit_menu.addAction(self._("undo"))
        self.redo_action = edit_menu.addAction(self._("redo"))
        edit_menu.addSeparator()
        self.cut_action = edit_menu.addAction(self._("cut"))
        self.copy_action = edit_menu.addAction(self._("copy"))
        self.paste_action = edit_menu.addAction(self._("paste"))
        
        # 视图菜单
        view_menu = menubar.addMenu(self._("view"))
        self.dark_theme_action = view_menu.addAction(self._("dark_theme"), lambda: self.set_theme("dark"))
        self.light_theme_action = view_menu.addAction(self._("light_theme"), lambda: self.set_theme("light"))
        
        # 构建菜单
        build_menu = menubar.addMenu(self._("build"))
        self.start_build_menu_action = build_menu.addAction(self._("start_build"), self.start_build)
        self.stop_build_menu_action = build_menu.addAction(self._("stop_build"), self.stop_build)
        build_menu.addSeparator()
        self.clean_menu_action = build_menu.addAction(self._("clean"), self.clean)
        self.install_menu_action = build_menu.addAction(self._("install"), self.install)
        
        # 帮助菜单
        help_menu = menubar.addMenu(self._("help"))
        self.documentation_action = help_menu.addAction(self._("documentation"))
        self.about_action = help_menu.addAction(self._("about"), self.show_about)
    
    def create_tool_bar(self):
        """创建工具栏"""
        toolbar = QToolBar("Main Toolbar")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)
        
        # 构建按钮
        self.build_action = QAction(self._("start_build"), self)
        self.build_action.triggered.connect(self.start_build)
        toolbar.addAction(self.build_action)
        
        # 停止按钮
        self.stop_action = QAction(self._("stop_build"), self)
        self.stop_action.triggered.connect(self.stop_build)
        self.stop_action.setEnabled(False)
        toolbar.addAction(self.stop_action)
        
        # 清理按钮
        self.clean_action = QAction(self._("clean"), self)
        self.clean_action.triggered.connect(self.clean)
        toolbar.addAction(self.clean_action)
        
        # 安装按钮
        self.install_action = QAction(self._("install"), self)
        self.install_action.triggered.connect(self.install)
        toolbar.addAction(self.install_action)
        
        toolbar.addSeparator()
        
        # 预设选择
        self.preset_label = QLabel(self._("preset") + ": ")
        toolbar.addWidget(self.preset_label)
        self.preset_combo = QComboBox()
        presets = ["balanced", "release", "debug", "minimal", "performance"]
        for preset in presets:
            self.preset_combo.addItem(self._(preset))
        self.preset_combo.setCurrentText(self._(self.current_preset))
        self.preset_combo.currentTextChanged.connect(self.on_preset_changed)
        toolbar.addWidget(self.preset_combo)
        
        toolbar.addSeparator()
        
        # 语言选择
        self.language_label = QLabel()
        self.update_language_label()
        toolbar.addWidget(self.language_label)
        self.language_combo = QComboBox()
        languages = I18N["zh_CN"]["languages"]
        # 格式: "简体中文" 或 "English"
        for code, name in languages.items():
            self.language_combo.addItem(name, code)
        self.language_combo.setCurrentIndex(list(languages.keys()).index(self.current_language))
        self.language_combo.currentIndexChanged.connect(self.on_language_changed)
        toolbar.addWidget(self.language_combo)
    
    def create_central_widget(self):
        """创建中央部件"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        layout = QVBoxLayout(central_widget)
        
        # 创建分割器
        splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # 左侧：配置选项卡
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        
        self.config_tabs = QTabWidget()
        self.create_config_tabs()
        # 保存标签页引用
        self.build_config_tab = None
        self.runtime_config_tab = None
        self.system_limits_tab = None
        self.features_tab = None
        left_layout.addWidget(self.config_tabs)
        
        # 右侧：输出和日志
        right_widget = QWidget()
        right_layout = QVBoxLayout(right_widget)
        
        # 进度条
        self.progress_bar = QProgressBar()
        self.progress_bar.setValue(0)
        right_layout.addWidget(self.progress_bar)
        
        # 输出标签页
        self.output_tabs = QTabWidget()
        
        # 构建输出
        self.build_output = QTextEdit()
        self.build_output.setReadOnly(True)
        self.build_output.setFont(QFont("Monospace", 9))
        self.output_tabs.addTab(self.build_output, self._("output"))
        
        # 构建日志
        self.build_log = QTextEdit()
        self.build_log.setReadOnly(True)
        self.build_log.setFont(QFont("Monospace", 9))
        self.output_tabs.addTab(self.build_log, self._("log"))
        
        right_layout.addWidget(self.output_tabs)
        
        # 添加到分割器
        splitter.addWidget(left_widget)
        splitter.addWidget(right_widget)
        splitter.setSizes([500, 700])
        
        layout.addWidget(splitter)
    
    def create_config_tabs(self):
        """创建配置标签页"""
        # 构建配置
        build_config_tab = QWidget()
        self.create_build_config_page(build_config_tab)
        self.config_tabs.addTab(build_config_tab, self._("build_config"))
        self.build_config_tab_index = 0
        
        # 运行时配置
        runtime_config_tab = QWidget()
        self.create_runtime_config_page(runtime_config_tab)
        self.config_tabs.addTab(runtime_config_tab, self._("runtime_config"))
        self.runtime_config_tab_index = 1
        
        # 系统限制
        system_limits_tab = QWidget()
        self.create_system_limits_page(system_limits_tab)
        self.config_tabs.addTab(system_limits_tab, self._("system_limits"))
        self.system_limits_tab_index = 2
        
        # 功能特性
        features_tab = QWidget()
        self.create_features_page(features_tab)
        self.config_tabs.addTab(features_tab, self._("features"))
        self.features_tab_index = 3
        
        # CPU特性
        cpu_features_tab = QWidget()
        self.create_cpu_features_page(cpu_features_tab)
        self.config_tabs.addTab(cpu_features_tab, self._("cpu_features"))
        self.cpu_features_tab_index = 4
        
        # 调度器
        scheduler_tab = QWidget()
        self.create_scheduler_page(scheduler_tab)
        self.config_tabs.addTab(scheduler_tab, self._("scheduler"))
        self.scheduler_tab_index = 5
        
        # 安全配置
        security_tab = QWidget()
        self.create_security_page(security_tab)
        self.config_tabs.addTab(security_tab, self._("security"))
        self.security_tab_index = 6
        
        # 内存配置
        memory_tab = QWidget()
        self.create_memory_page(memory_tab)
        self.config_tabs.addTab(memory_tab, self._("memory"))
        self.memory_tab_index = 7
        
        # 调试选项
        debug_tab = QWidget()
        self.create_debug_page(debug_tab)
        self.config_tabs.addTab(debug_tab, self._("debug_tab"))
        self.debug_tab_index = 8
        
        # 驱动配置
        drivers_tab = QWidget()
        self.create_drivers_page(drivers_tab)
        self.config_tabs.addTab(drivers_tab, self._("drivers"))
        self.drivers_tab_index = 9
        
        # 性能配置
        performance_tab = QWidget()
        self.create_performance_page(performance_tab)
        self.config_tabs.addTab(performance_tab, self._("performance_tab"))
        self.performance_tab_index = 10
    
    def create_build_config_page(self, parent: QWidget):
        """创建构建配置页面"""
        layout = QFormLayout(parent)
        
        # 构建模式
        self.build_mode_combo = QComboBox()
        self.build_mode_combo.addItems(["dynamic", "static"])
        layout.addRow("Build Mode:", self.build_mode_combo)
        
        # 优化级别
        self.optimize_spin = QSpinBox()
        self.optimize_spin.setRange(0, 3)
        self.optimize_spin.setValue(2)
        layout.addRow(self._("optimize_level") + ":", self.optimize_spin)
        
        # 调试符号
        self.debug_symbols_check = QCheckBox()
        self.debug_symbols_check.setChecked(True)
        layout.addRow(self._("debug_symbols") + ":", self.debug_symbols_check)
        
        # LTO
        self.lto_check = QCheckBox()
        self.lto_check.setChecked(False)
        layout.addRow(self._("lto") + ":", self.lto_check)
        
        # 剥离符号
        self.strip_check = QCheckBox()
        self.strip_check.setChecked(False)
        layout.addRow("Strip Symbols:", self.strip_check)
    
    def create_runtime_config_page(self, parent: QWidget):
        """创建运行时配置页面"""
        layout = QFormLayout(parent)
        
        # 控制台日志
        self.console_log_check = QCheckBox()
        self.console_log_check.setChecked(True)
        layout.addRow(self._("console_log") + ":", self.console_log_check)
        
        # 串口日志
        self.serial_log_check = QCheckBox()
        self.serial_log_check.setChecked(True)
        layout.addRow(self._("serial_log") + ":", self.serial_log_check)
        
        # Panic on bug
        self.panic_on_bug_check = QCheckBox()
        self.panic_on_bug_check.setChecked(True)
        layout.addRow(self._("panic_on_bug") + ":", self.panic_on_bug_check)
        
        # 栈金丝雀
        self.stack_canary_check = QCheckBox()
        self.stack_canary_check.setChecked(True)
        layout.addRow(self._("stack_canary") + ":", self.stack_canary_check)
        
        # 边界检查
        self.bounds_check_check = QCheckBox()
        self.bounds_check_check.setChecked(False)
        layout.addRow(self._("bounds_check") + ":", self.bounds_check_check)
    
    def create_system_limits_page(self, parent: QWidget):
        """创建系统限制页面"""
        layout = QFormLayout(parent)
        
        # 最大域数
        self.max_domains_spin = QSpinBox()
        self.max_domains_spin.setRange(1, 1024)
        self.max_domains_spin.setValue(256)
        layout.addRow("Max Domains:", self.max_domains_spin)
        
        # 最大能力数
        self.max_capabilities_spin = QSpinBox()
        self.max_capabilities_spin.setRange(1, 16384)
        self.max_capabilities_spin.setValue(2048)
        layout.addRow("Max Capabilities:", self.max_capabilities_spin)
        
        # 最大线程数
        self.max_threads_spin = QSpinBox()
        self.max_threads_spin.setRange(1, 4096)
        self.max_threads_spin.setValue(256)
        layout.addRow("Max Threads:", self.max_threads_spin)
        
        # 每域最大能力数
        self.cap_per_domain_spin = QSpinBox()
        self.cap_per_domain_spin.setRange(1, 1024)
        self.cap_per_domain_spin.setValue(128)
        layout.addRow("Max Caps per Domain:", self.cap_per_domain_spin)
        
        # 每域最大线程数
        self.thread_per_domain_spin = QSpinBox()
        self.thread_per_domain_spin.setRange(1, 256)
        self.thread_per_domain_spin.setValue(16)
        layout.addRow("Max Threads per Domain:", self.thread_per_domain_spin)
    
    def create_features_page(self, parent: QWidget):
        """创建功能特性页面"""
        layout = QFormLayout(parent)
        
        # SMP
        self.smp_check = QCheckBox()
        self.smp_check.setChecked(True)
        layout.addRow("SMP Support:", self.smp_check)
        
        # APIC
        self.apic_check = QCheckBox()
        self.apic_check.setChecked(True)
        layout.addRow("APIC Support:", self.apic_check)
        
        # ACPI
        self.acpi_check = QCheckBox()
        self.acpi_check.setChecked(True)
        layout.addRow(self._("acpi") + ":", self.acpi_check)
        
        # PCI
        self.pci_check = QCheckBox()
        self.pci_check.setChecked(True)
        layout.addRow(self._("pci") + ":", self.pci_check)
        
        # AHCI
        self.ahci_check = QCheckBox()
        self.ahci_check.setChecked(True)
        layout.addRow("AHCI Support:", self.ahci_check)
        
        # USB
        self.usb_check = QCheckBox()
        self.usb_check.setChecked(True)
        layout.addRow(self._("usb") + ":", self.usb_check)
        
        # VirtIO
        self.virtio_check = QCheckBox()
        self.virtio_check.setChecked(True)
        layout.addRow(self._("virtio") + ":", self.virtio_check)
        
        # UEFI
        self.efi_check = QCheckBox()
        self.efi_check.setChecked(True)
        layout.addRow(self._("efi") + ":", self.efi_check)
    
    def create_cpu_features_page(self, parent: QWidget):
        """创建CPU特性页面"""
        layout = QFormLayout(parent)
        
        # MMX
        self.mmx_check = QCheckBox()
        self.mmx_check.setChecked(True)
        layout.addRow("MMX:", self.mmx_check)
        
        # SSE
        self.sse_check = QCheckBox()
        self.sse_check.setChecked(True)
        layout.addRow("SSE:", self.sse_check)
        
        # SSE2
        self.sse2_check = QCheckBox()
        self.sse2_check.setChecked(True)
        layout.addRow("SSE2:", self.sse2_check)
        
        # SSE3
        self.sse3_check = QCheckBox()
        self.sse3_check.setChecked(True)
        layout.addRow("SSE3:", self.sse3_check)
        
        # SSSE3
        self.ssse3_check = QCheckBox()
        self.ssse3_check.setChecked(True)
        layout.addRow("SSSE3:", self.ssse3_check)
        
        # SSE4.1
        self.sse4_1_check = QCheckBox()
        self.sse4_1_check.setChecked(True)
        layout.addRow("SSE4.1:", self.sse4_1_check)
        
        # SSE4.2
        self.sse4_2_check = QCheckBox()
        self.sse4_2_check.setChecked(True)
        layout.addRow("SSE4.2:", self.sse4_2_check)
        
        # AVX
        self.avx_check = QCheckBox()
        self.avx_check.setChecked(True)
        layout.addRow("AVX:", self.avx_check)
        
        # AVX2
        self.avx2_check = QCheckBox()
        self.avx2_check.setChecked(True)
        layout.addRow("AVX2:", self.avx2_check)
        
        # AES
        self.aes_check = QCheckBox()
        self.aes_check.setChecked(True)
        layout.addRow("AES-NI:", self.aes_check)
        
        # RDRAND
        self.rdrand_check = QCheckBox()
        self.rdrand_check.setChecked(True)
        layout.addRow("RDRAND:", self.rdrand_check)
    
    def create_scheduler_page(self, parent: QWidget):
        """创建调度器页面"""
        layout = QFormLayout(parent)
        
        # 调度策略
        self.scheduler_policy_combo = QComboBox()
        self.scheduler_policy_combo.addItems(["priority_rr", "round_robin", "fifo"])
        layout.addRow(self._("scheduler_policy") + ":", self.scheduler_policy_combo)
        
        # 时间片
        self.time_slice_spin = QSpinBox()
        self.time_slice_spin.setRange(1, 1000)
        self.time_slice_spin.setValue(10)
        layout.addRow(self._("time_slice") + "(ms):", self.time_slice_spin)
        
        # 抢占式调度
        self.preemptive_check = QCheckBox()
        self.preemptive_check.setChecked(True)
        layout.addRow(self._("preemptive") + ":", self.preemptive_check)
        
        # 负载均衡
        self.load_balancing_check = QCheckBox()
        self.load_balancing_check.setChecked(True)
        layout.addRow(self._("load_balancing") + ":", self.load_balancing_check)
        
        # 负载均衡阈值
        self.load_balance_threshold_spin = QSpinBox()
        self.load_balance_threshold_spin.setRange(1, 100)
        self.load_balance_threshold_spin.setValue(80)
        layout.addRow(self._("load_threshold") + "(%):", self.load_balance_threshold_spin)
    
    def create_security_page(self, parent: QWidget):
        """创建安全配置页面"""
        layout = QFormLayout(parent)
        
        # 隔离模式
        self.isolation_mode_combo = QComboBox()
        self.isolation_mode_combo.addItems(["strict", "permissive"])
        layout.addRow(self._("isolation_mode") + ":", self.isolation_mode_combo)
        
        # KASLR
        self.kaslr_check = QCheckBox()
        self.kaslr_check.setChecked(False)
        layout.addRow("KASLR:", self.kaslr_check)
        
        # SMEP
        self.smep_check = QCheckBox()
        self.smep_check.setChecked(False)
        layout.addRow("SMEP:", self.smep_check)
        
        # SMAP
        self.smap_check = QCheckBox()
        self.smap_check.setChecked(False)
        layout.addRow("SMAP:", self.smap_check)
        
        # 审计日志
        self.audit_check = QCheckBox()
        self.audit_check.setChecked(True)
        layout.addRow(self._("audit") + ":", self.audit_check)
        
        # 形式化验证
        self.form_verification_check = QCheckBox()
        self.form_verification_check.setChecked(True)
        layout.addRow(self._("form_verification") + ":", self.form_verification_check)
        
        # 能力验证
        self.cap_verify_check = QCheckBox()
        self.cap_verify_check.setChecked(True)
        layout.addRow(self._("cap_verify") + ":", self.cap_verify_check)
        
        # 保护页
        self.guard_pages_check = QCheckBox()
        self.guard_pages_check.setChecked(True)
        layout.addRow(self._("guard_pages") + ":", self.guard_pages_check)
        
        # 零释放
        self.zero_on_free_check = QCheckBox()
        self.zero_on_free_check.setChecked(True)
        layout.addRow(self._("zero_on_free") + ":", self.zero_on_free_check)
    
    def create_memory_page(self, parent: QWidget):
        """创建内存配置页面"""
        layout = QFormLayout(parent)
        
        # 堆大小
        self.heap_size_spin = QSpinBox()
        self.heap_size_spin.setRange(16, 4096)
        self.heap_size_spin.setValue(128)
        self.heap_size_spin.setSuffix(" MB")
        layout.addRow(self._("heap_size") + ":", self.heap_size_spin)
        
        # 栈大小
        self.stack_size_spin = QSpinBox()
        self.stack_size_spin.setRange(4, 64)
        self.stack_size_spin.setValue(8)
        self.stack_size_spin.setSuffix(" KB")
        layout.addRow(self._("stack_size") + ":", self.stack_size_spin)
        
        # 页面缓存百分比
        self.page_cache_spin = QSpinBox()
        self.page_cache_spin.setRange(0, 50)
        self.page_cache_spin.setValue(20)
        self.page_cache_spin.setSuffix(" %")
        layout.addRow(self._("page_cache") + ":", self.page_cache_spin)
        
        # 缓冲区缓存大小
        self.buffer_cache_spin = QSpinBox()
        self.buffer_cache_spin.setRange(256, 16384)
        self.buffer_cache_spin.setValue(1024)
        self.buffer_cache_spin.setSuffix(" KB")
        layout.addRow(self._("buffer_cache") + ":", self.buffer_cache_spin)
        
        # 最大页表数
        self.max_page_tables_spin = QSpinBox()
        self.max_page_tables_spin.setRange(64, 1024)
        self.max_page_tables_spin.setValue(256)
        layout.addRow(self._("max_page_tables") + ":", self.max_page_tables_spin)
    
    def create_debug_page(self, parent: QWidget):
        """创建调试选项页面"""
        layout = QFormLayout(parent)
        
        # 控制台日志
        self.console_log_check = QCheckBox()
        self.console_log_check.setChecked(True)
        layout.addRow(self._("console_log") + ":", self.console_log_check)
        
        # 串口日志
        self.serial_log_check = QCheckBox()
        self.serial_log_check.setChecked(True)
        layout.addRow(self._("serial_log") + ":", self.serial_log_check)
        
        # Panic on bug
        self.panic_on_bug_check = QCheckBox()
        self.panic_on_bug_check.setChecked(True)
        layout.addRow(self._("panic_on_bug") + ":", self.panic_on_bug_check)
        
        # 栈金丝雀
        self.stack_canary_check = QCheckBox()
        self.stack_canary_check.setChecked(True)
        layout.addRow(self._("stack_canary") + ":", self.stack_canary_check)
        
        # 边界检查
        self.bounds_check_check = QCheckBox()
        self.bounds_check_check.setChecked(False)
        layout.addRow(self._("bounds_check") + ":", self.bounds_check_check)
        
        # 详细输出
        self.verbose_check = QCheckBox()
        self.verbose_check.setChecked(False)
        layout.addRow(self._("verbose") + ":", self.verbose_check)
        
        # 跟踪功能
        self.trace_check = QCheckBox()
        self.trace_check.setChecked(False)
        layout.addRow(self._("trace") + ":", self.trace_check)
    
    def create_drivers_page(self, parent: QWidget):
        """创建驱动配置页面"""
        layout = QFormLayout(parent)
        
        # 控制台驱动
        self.console_driver_check = QCheckBox()
        self.console_driver_check.setChecked(True)
        layout.addRow(self._("console_driver") + ":", self.console_driver_check)
        
        # 键盘驱动
        self.keyboard_driver_check = QCheckBox()
        self.keyboard_driver_check.setChecked(True)
        layout.addRow("键盘驱动:", self.keyboard_driver_check)
        
        # PS/2鼠标驱动
        self.ps2_mouse_check = QCheckBox()
        self.ps2_mouse_check.setChecked(True)
        layout.addRow("PS/2鼠标:", self.ps2_mouse_check)
        
        # UART驱动
        self.uart_driver_check = QCheckBox()
        self.uart_driver_check.setChecked(True)
        layout.addRow("UART驱动:", self.uart_driver_check)
        
        # 串口波特率
        self.baud_rate_combo = QComboBox()
        self.baud_rate_combo.addItems(["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"])
        self.baud_rate_combo.setCurrentText("115200")
        layout.addRow("波特率:", self.baud_rate_combo)
        
        # 串口数据位
        self.data_bits_combo = QComboBox()
        self.data_bits_combo.addItems(["5", "6", "7", "8"])
        self.data_bits_combo.setCurrentText("8")
        layout.addRow("数据位:", self.data_bits_combo)
    
    def create_performance_page(self, parent: QWidget):
        """创建性能配置页面"""
        layout = QFormLayout(parent)
        
        # 快速路径
        self.fast_path_check = QCheckBox()
        self.fast_path_check.setChecked(True)
        layout.addRow("快速路径:", self.fast_path_check)
        
        # 性能计数器
        self.perf_counter_check = QCheckBox()
        self.perf_counter_check.setChecked(False)
        layout.addRow("性能计数器:", self.perf_counter_check)
        
        # CPU亲和性
        self.cpu_affinity_check = QCheckBox()
        self.cpu_affinity_check.setChecked(True)
        layout.addRow(self._("cpu_affinity") + ":", self.cpu_affinity_check)
        
        # NUMA优化
        self.numa_opt_check = QCheckBox()
        self.numa_opt_check.setChecked(False)
        layout.addRow("NUMA优化:", self.numa_opt_check)
        
        # 延迟优化
        self.latency_opt_check = QCheckBox()
        self.latency_opt_check.setChecked(False)
        layout.addRow("延迟优化:", self.latency_opt_check)
        
        # 吞吐量优化
        self.throughput_opt_check = QCheckBox()
        self.throughput_opt_check.setChecked(False)
        layout.addRow("吞吐量优化:", self.throughput_opt_check)
    
    def create_status_bar(self):
        """创建状态栏"""
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.update_status(self._("ready"))
    
    def _(self, key: str) -> str:
        """翻译函数"""
        if key in I18N[self.current_language]:
            return I18N[self.current_language][key]
        return key

    def update_language_label(self):
        """更新语言标签，显示当前选择的语言名称"""
        languages = I18N["zh_CN"]["languages"]
        if self.current_language in languages:
            current_language_name = languages[self.current_language]
            self.language_label.setText(current_language_name + ":")

    def update_status(self, message: str):
        """更新状态栏"""
        self.status_bar.showMessage(message)
    
    def set_theme(self, theme: str):
        """设置主题"""
        self.current_theme = theme
        self.apply_theme()
    
    def apply_theme(self):
        """应用主题"""
        app = QApplication.instance()
        palette = QPalette()
        
        if self.current_theme == "dark":
            # 深色主题
            palette.setColor(QPalette.ColorRole.Window, QColor(53, 53, 53))
            palette.setColor(QPalette.ColorRole.WindowText, QColor(255, 255, 255))
            palette.setColor(QPalette.ColorRole.Base, QColor(25, 25, 25))
            palette.setColor(QPalette.ColorRole.AlternateBase, QColor(53, 53, 53))
            palette.setColor(QPalette.ColorRole.ToolTipBase, QColor(255, 255, 255))
            palette.setColor(QPalette.ColorRole.ToolTipText, QColor(255, 255, 255))
            palette.setColor(QPalette.ColorRole.Text, QColor(255, 255, 255))
            palette.setColor(QPalette.ColorRole.Button, QColor(53, 53, 53))
            palette.setColor(QPalette.ColorRole.ButtonText, QColor(255, 255, 255))
            palette.setColor(QPalette.ColorRole.BrightText, QColor(255, 0, 0))
            palette.setColor(QPalette.ColorRole.Link, QColor(42, 130, 218))
            palette.setColor(QPalette.ColorRole.Highlight, QColor(42, 130, 218))
            palette.setColor(QPalette.ColorRole.HighlightedText, QColor(0, 0, 0))
        else:
            # 浅色主题
            palette.setColor(QPalette.ColorRole.Window, QColor(240, 240, 240))
            palette.setColor(QPalette.ColorRole.WindowText, QColor(0, 0, 0))
            palette.setColor(QPalette.ColorRole.Base, QColor(255, 255, 255))
            palette.setColor(QPalette.ColorRole.AlternateBase, QColor(245, 245, 245))
            palette.setColor(QPalette.ColorRole.ToolTipBase, QColor(255, 255, 220))
            palette.setColor(QPalette.ColorRole.ToolTipText, QColor(0, 0, 0))
            palette.setColor(QPalette.ColorRole.Text, QColor(0, 0, 0))
            palette.setColor(QPalette.ColorRole.Button, QColor(240, 240, 240))
            palette.setColor(QPalette.ColorRole.ButtonText, QColor(0, 0, 0))
            palette.setColor(QPalette.ColorRole.BrightText, QColor(255, 0, 0))
            palette.setColor(QPalette.ColorRole.Link, QColor(0, 0, 255))
            palette.setColor(QPalette.ColorRole.Highlight, QColor(76, 163, 224))
            palette.setColor(QPalette.ColorRole.HighlightedText, QColor(255, 255, 255))
        
        app.setPalette(palette)
    
    def on_language_changed(self, index: int):
        """语言改变事件"""
        code = self.language_combo.itemData(index)
        if code and code in I18N:
            self.current_language = code

            # 更新语言标签
            self.update_language_label()

            self.retranslate_ui()
    
    def retranslate_ui(self):
        """重新翻译UI"""
        self.setWindowTitle(self._("title"))
        
        # 重新翻译菜单
        menubar = self.menuBar()
        
        # 文件菜单
        file_menu = menubar.actions()[0].menu()
        file_menu.setTitle(self._("file"))
        self.new_profile_action.setText(self._("new_profile"))
        self.open_profile_action.setText(self._("open_profile"))
        self.save_profile_action.setText(self._("save_profile"))
        self.export_config_action.setText(self._("export_config"))
        self.import_config_action.setText(self._("import_config"))
        self.preferences_action.setText(self._("preferences"))
        self.exit_action.setText(self._("exit"))
        
        # 编辑菜单
        edit_menu = menubar.actions()[1].menu()
        edit_menu.setTitle(self._("edit"))
        self.undo_action.setText(self._("undo"))
        self.redo_action.setText(self._("redo"))
        self.cut_action.setText(self._("cut"))
        self.copy_action.setText(self._("copy"))
        self.paste_action.setText(self._("paste"))
        
        # 视图菜单
        view_menu = menubar.actions()[2].menu()
        view_menu.setTitle(self._("view"))
        self.dark_theme_action.setText(self._("dark_theme"))
        self.light_theme_action.setText(self._("light_theme"))
        
        # 构建菜单
        build_menu = menubar.actions()[3].menu()
        build_menu.setTitle(self._("build"))
        self.start_build_menu_action.setText(self._("start_build"))
        self.stop_build_menu_action.setText(self._("stop_build"))
        self.clean_menu_action.setText(self._("clean"))
        self.install_menu_action.setText(self._("install"))
        
        # 帮助菜单
        help_menu = menubar.actions()[4].menu()
        help_menu.setTitle(self._("help"))
        self.documentation_action.setText(self._("documentation"))
        self.about_action.setText(self._("about"))
        
        # 重新翻译工具栏
        self.build_action.setText(self._("start_build"))
        self.stop_action.setText(self._("stop_build"))
        self.clean_action.setText(self._("clean"))
        self.install_action.setText(self._("install"))
        self.preset_label.setText(self._("preset") + ": ")
        self.language_label.setText(self._("language") + ": ")
        
        # 重新翻译配置标签页
        self.config_tabs.setTabText(0, self._("build_config"))
        self.config_tabs.setTabText(1, self._("runtime_config"))
        self.config_tabs.setTabText(2, self._("system_limits"))
        self.config_tabs.setTabText(3, self._("features"))
        # 新增的标签页使用翻译
        self.config_tabs.setTabText(4, self._("cpu_features"))
        self.config_tabs.setTabText(5, self._("scheduler"))
        self.config_tabs.setTabText(6, self._("security"))
        self.config_tabs.setTabText(7, self._("memory"))
        self.config_tabs.setTabText(8, self._("debug_tab"))
        self.config_tabs.setTabText(9, self._("drivers"))
        self.config_tabs.setTabText(10, self._("performance_tab"))
        
        # 重新翻译预设下拉框
        current_preset = self.preset_combo.currentText()
        current_index = self.preset_combo.currentIndex()
        
        # 阻止信号触发以避免递归
        self.preset_combo.blockSignals(True)
        self.preset_combo.clear()
        presets = ["balanced", "release", "debug", "minimal", "performance"]
        for preset in presets:
            self.preset_combo.addItem(self._(preset), preset)
        self.preset_combo.setCurrentIndex(current_index)
        self.preset_combo.blockSignals(False)
        
        # 重新翻译语言下拉框
        self.update_language_label()
        current_lang_code = self.current_language  # 保存当前语言代码
        current_index_lang = self.language_combo.currentIndex()
        
        # 阻止信号触发以避免递归
        self.language_combo.blockSignals(True)
        self.language_combo.clear()
        languages = I18N.get("zh_CN", {}).get("languages", {})
        lang_code_list = list(languages.keys())
        for i, code in enumerate(lang_code_list):
            name = languages[code]
            display_text = f"{name} ({name})"
            self.language_combo.addItem(display_text, code)
        
        # 根据语言代码恢复选择
        if current_lang_code in lang_code_list:
            self.language_combo.setCurrentIndex(lang_code_list.index(current_lang_code))
        elif current_index_lang < len(lang_code_list):
            self.language_combo.setCurrentIndex(current_index_lang)
        
        # 确保current_language有效
        selected_code = self.language_combo.currentData()
        if selected_code and selected_code in I18N:
            self.current_language = selected_code
        elif not self.current_language or self.current_language not in I18N:
            self.current_language = "zh_CN"  # 默认语言
        
        # 恢复信号
        self.language_combo.blockSignals(False)
        
        # 重新翻译输出标签页
        self.output_tabs.setTabText(0, self._("output"))
        self.output_tabs.setTabText(1, self._("log"))
        
        # 更新状态
        self.update_status(self._("ready"))
    
    def on_preset_changed(self, text: str):
        """预设改变事件"""
        # 将文本映射回代码
        preset_map = {
            self._("balanced"): "balanced",
            self._("release"): "release",
            self._("debug"): "debug",
            self._("minimal"): "minimal",
            self._("performance"): "performance"
        }
        self.current_preset = preset_map.get(text, "balanced")
        self.apply_preset()
    
    def apply_preset(self):
        """应用预设配置"""
        # 根据预设更新UI控件的值
        if self.current_preset == "debug":
            self.optimize_spin.setValue(0)
            self.debug_symbols_check.setChecked(True)
            self.bounds_check_check.setChecked(True)
        elif self.current_preset == "release":
            self.optimize_spin.setValue(3)
            self.debug_symbols_check.setChecked(False)
            self.strip_check.setChecked(True)
        elif self.current_preset == "minimal":
            self.optimize_spin.setValue(2)
            self.debug_symbols_check.setChecked(False)
            self.strip_check.setChecked(True)
            self.smp_check.setChecked(False)
            self.acpi_check.setChecked(False)
        elif self.current_preset == "performance":
            self.optimize_spin.setValue(3)
            self.lto_check.setChecked(True)
            self.debug_symbols_check.setChecked(False)
    
    def load_config(self):
        """加载配置"""
        if PLATFORM_YAML.exists():
            try:
                import yaml
                with open(PLATFORM_YAML, 'r', encoding='utf-8') as f:
                    self.config_data = yaml.safe_load(f)
                self.apply_config_to_ui()
            except Exception as e:
                self.append_log(f"加载配置失败: {str(e)}")
    
    def apply_config_to_ui(self):
        """应用配置到UI"""
        if "build" in self.config_data:
            build_config = self.config_data["build"]
            self.build_mode_combo.setCurrentText(build_config.get("mode", "dynamic"))
            self.optimize_spin.setValue(build_config.get("optimize_level", 2))
            self.debug_symbols_check.setChecked(build_config.get("debug_symbols", True))
            self.lto_check.setChecked(build_config.get("lto", False))
            self.strip_check.setChecked(build_config.get("strip", False))
        
        if "debug" in self.config_data:
            debug_config = self.config_data["debug"]
            self.console_log_check.setChecked(debug_config.get("console_log", True))
            self.serial_log_check.setChecked(debug_config.get("serial_log", True))
            self.panic_on_bug_check.setChecked(debug_config.get("panic_on_bug", True))
            self.stack_canary_check.setChecked(debug_config.get("stack_canary", True))
            self.bounds_check_check.setChecked(debug_config.get("bounds_check", False))
        
        if "system_limits" in self.config_data:
            limits_config = self.config_data["system_limits"]
            self.max_domains_spin.setValue(limits_config.get("max_domains", 256))
            self.max_capabilities_spin.setValue(limits_config.get("max_capabilities", 2048))
            self.max_threads_spin.setValue(limits_config.get("max_threads", 256))
            self.cap_per_domain_spin.setValue(limits_config.get("capabilities_per_domain", 128))
            self.thread_per_domain_spin.setValue(limits_config.get("threads_per_domain", 16))
        
        if "features" in self.config_data:
            features_config = self.config_data["features"]
            self.smp_check.setChecked(features_config.get("smp", True))
            self.apic_check.setChecked(features_config.get("apic", True))
            self.acpi_check.setChecked(features_config.get("acpi", True))
            self.pci_check.setChecked(features_config.get("pci", True))
            self.ahci_check.setChecked(features_config.get("ahci", True))
            self.usb_check.setChecked(features_config.get("usb", True))
            self.virtio_check.setChecked(features_config.get("virtio", True))
            self.efi_check.setChecked(features_config.get("efi", True))
    
    def save_config_to_file(self):
        """保存配置到文件"""
        try:
            import yaml
            
            # 从UI读取配置
            self.config_data["build"] = {
                "mode": self.build_mode_combo.currentText(),
                "optimize_level": self.optimize_spin.value(),
                "debug_symbols": self.debug_symbols_check.isChecked(),
                "lto": self.lto_check.isChecked(),
                "strip": self.strip_check.isChecked()
            }
            
            self.config_data["debug"] = {
                "console_log": self.console_log_check.isChecked(),
                "serial_log": self.serial_log_check.isChecked(),
                "panic_on_bug": self.panic_on_bug_check.isChecked(),
                "stack_canary": self.stack_canary_check.isChecked(),
                "bounds_check": self.bounds_check_check.isChecked()
            }
            
            self.config_data["system_limits"] = {
                "max_domains": self.max_domains_spin.value(),
                "max_capabilities": self.max_capabilities_spin.value(),
                "max_threads": self.max_threads_spin.value(),
                "capabilities_per_domain": self.cap_per_domain_spin.value(),
                "threads_per_domain": self.thread_per_domain_spin.value()
            }
            
            self.config_data["features"] = {
                "smp": self.smp_check.isChecked(),
                "apic": self.apic_check.isChecked(),
                "acpi": self.acpi_check.isChecked(),
                "pci": self.pci_check.isChecked(),
                "ahci": self.ahci_check.isChecked(),
                "usb": self.usb_check.isChecked(),
                "virtio": self.virtio_check.isChecked(),
                "efi": self.efi_check.isChecked()
            }
            
            with open(PLATFORM_YAML, 'w', encoding='utf-8') as f:
                yaml.dump(self.config_data, f, default_flow_style=False, allow_unicode=True)
            
            return True
        except Exception as e:
            self.append_log(f"保存配置失败: {str(e)}")
            return False
    
    def start_build(self):
        """开始构建"""
        if self.build_thread and self.build_thread.isRunning():
            return
        
        # 保存配置
        if not self.save_config_to_file():
            QMessageBox.warning(self, "警告", "保存配置失败，但将继续构建")
        
        # 禁用构建按钮，启用停止按钮
        self.build_action.setEnabled(False)
        self.stop_action.setEnabled(True)
        
        # 清空输出
        self.build_output.clear()
        self.build_log.clear()
        
        # 更新状态
        self.update_status(self._("building"))
        self.progress_bar.setValue(0)
        
        # 创建构建线程
        command = ["make", "kernel"]
        self.build_thread = BuildThread(command, ROOT_DIR)
        self.build_thread.update_signal.connect(self.append_output)
        self.build_thread.progress_signal.connect(self.update_progress)
        self.build_thread.finished_signal.connect(self.on_build_finished)
        self.build_thread.start()
    
    def stop_build(self):
        """停止构建"""
        if self.build_thread and self.build_thread.isRunning():
            self.build_thread.stop()
            self.build_thread.wait()
            self.append_output(self._("build_stopped"))
    
    def clean(self):
        """清理"""
        self.append_output("开始清理...")
        try:
            subprocess.run(["make", "clean"], cwd=ROOT_DIR, check=True)
            self.append_output("清理完成")
        except subprocess.CalledProcessError as e:
            self.append_output(f"清理失败: {str(e)}")
    
    def install(self):
        """安装"""
        self.append_output("开始安装...")
        try:
            subprocess.run(["make", "install"], cwd=ROOT_DIR, check=True)
            self.append_output("安装完成")
        except subprocess.CalledProcessError as e:
            self.append_output(f"安装失败: {str(e)}")
    
    def append_output(self, text: str):
        """添加输出文本"""
        self.build_output.append(text)
        self.build_log.append(f"[{datetime.now().strftime('%H:%M:%S')}] {text}")
    
    def append_log(self, text: str):
        """添加日志文本"""
        self.build_log.append(f"[{datetime.now().strftime('%H:%M:%S')}] {text}")
    
    def update_progress(self, value: int):
        """更新进度条"""
        self.progress_bar.setValue(value)
    
    def on_build_finished(self, exit_code: int, message: str):
        """构建完成"""
        self.build_action.setEnabled(True)
        self.stop_action.setEnabled(False)
        self.update_status(message)
        self.progress_bar.setValue(100)
        
        if exit_code == 0:
            self.append_output(f"\n{message}")
        else:
            self.append_output(f"\n{message} (退出码: {exit_code})")
            QMessageBox.critical(self, self._("build_failed"), f"{message}\n退出码: {exit_code}")
    
    def new_profile(self):
        """新建配置文件"""
        QMessageBox.information(self, "提示", "新建配置文件功能待实现")
    
    def open_profile(self):
        """打开配置文件"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, self._("open_profile"), str(PLATFORM_YAML.parent), "YAML files (*.yaml);;All files (*)"
        )
        if file_path:
            # 加载配置文件
            pass
    
    def save_profile(self):
        """保存配置文件"""
        if self.save_config_to_file():
            QMessageBox.information(self, "成功", "配置保存成功")
    
    def export_config(self):
        """导出配置"""
        file_path, _ = QFileDialog.getSaveFileName(
            self, self._("export_config"), "", "YAML files (*.yaml);;JSON files (*.json)"
        )
        if file_path:
            try:
                import yaml
                with open(file_path, 'w', encoding='utf-8') as f:
                    yaml.dump(self.config_data, f, default_flow_style=False, allow_unicode=True)
                QMessageBox.information(self, "成功", "配置导出成功")
            except Exception as e:
                QMessageBox.critical(self, "错误", f"导出失败: {str(e)}")
    
    def import_config(self):
        """导入配置"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, self._("import_config"), "", "YAML files (*.yaml);;JSON files (*.json)"
        )
        if file_path:
            try:
                import yaml
                with open(file_path, 'r', encoding='utf-8') as f:
                    self.config_data = yaml.safe_load(f)
                self.apply_config_to_ui()
                QMessageBox.information(self, "成功", "配置导入成功")
            except Exception as e:
                QMessageBox.critical(self, "错误", f"导入失败: {str(e)}")
    
    def preferences(self):
        """首选项"""
        dialog = QDialog(self)
        dialog.setWindowTitle(self._("preferences"))
        dialog.setMinimumSize(400, 300)
        
        layout = QFormLayout(dialog)
        
        # 语言选择
        language_combo = QComboBox()
        languages = I18N["zh_CN"]["languages"]
        for code, name in languages.items():
            language_combo.addItem(name, code)
        language_combo.setCurrentText(languages[self.current_language])
        layout.addRow(self._("language"), language_combo)
        
        # 主题选择
        theme_combo = QComboBox()
        theme_combo.addItem(self._("dark_theme"), "dark")
        theme_combo.addItem(self._("light_theme"), "light")
        theme_combo.setCurrentIndex(0 if self.current_theme == "dark" else 1)
        layout.addRow(self._("theme"), theme_combo)
        
        # 按钮
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addRow(buttons)
        
        if dialog.exec() == QDialog.DialogCode.Accepted:
            self.current_language = language_combo.currentData()
            self.current_theme = theme_combo.currentData()
            self.apply_theme()
            self.retranslate_ui()
    
    def show_about(self):
        """显示关于对话框"""
        QMessageBox.about(
            self,
            self._("about"),
            f"{PROJECT} {VERSION}\n\n"
            f"HIC (Hierarchical Isolation Core) 构建系统\n"
            f"支持多种构建界面：Qt GUI, GTK GUI, TUI, CLI\n\n"
            f"© 2026 DslsDZC"
        )


def main():
    """主函数"""
    app = QApplication(sys.argv)
    app.setApplicationName(PROJECT)
    app.setApplicationVersion(VERSION)
    
    window = HICBuildGUI()
    window.show()
    
    sys.exit(app.exec())


if __name__ == "__main__":
    main()