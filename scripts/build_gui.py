#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
HIC系统构建系统 - GTK GUI模式
使用GTK3库实现图形用户界面
与Qt界面保持功能一致
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

try:
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk, GLib, Pango, Gdk
except ImportError:
    print("错误: 缺少 GTK3 库")
    print("请安装以下依赖:")
    print("  Arch Linux: sudo pacman -S gtk3 python-gobject")
    print("  Ubuntu/Debian: sudo apt-get install python3-gi gir1.2-gtk-3.0")
    print("  Fedora: sudo dnf install python3-gobject gtk3")
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

class HICBuildGUI(Gtk.ApplicationWindow):
    """HIC构建系统GTK GUI主窗口"""
    
    def __init__(self, app):
        super().__init__(application=app)
        self.current_language = "zh_CN"
        self.current_theme = "dark"
        self.current_preset = "balanced"
        self.is_building = False
        self.build_thread = None
        
        # 保存UI元素引用
        self.ui_elements = {}
        
        # 加载配置
        self.load_config()
        
        # 初始化UI
        self.init_ui()
        self.apply_theme()
        self.retranslate_ui()
    
    def _(self, key: str) -> str:
        """翻译函数"""
        if self.current_language in I18N:
            return I18N[self.current_language].get(key, key)
        return key

    def update_language_label(self):
        """更新语言标签，显示当前选择的语言名称"""
        languages = I18N["zh_CN"]["languages"]
        if self.current_language in languages:
            current_language_name = languages[self.current_language]
            self.language_label.set_text(current_language_name + ":")

    def load_config(self):
        """从YAML加载配置"""
        try:
            import yaml
            if PLATFORM_YAML.exists():
                with open(PLATFORM_YAML, 'r', encoding='utf-8') as f:
                    data = yaml.safe_load(f)
                    if 'build_system' in data:
                        bs = data['build_system']
                        if 'localization' in bs:
                            self.current_language = bs['localization'].get('language', 'zh_CN')
                        if 'presets' in bs:
                            self.current_preset = bs['presets'].get('default', 'balanced')
        except:
            pass
    
    def init_ui(self):
        """初始化UI"""
        self.set_default_size(1200, 800)
        self.set_title(self._("title"))
        
        # 创建主容器
        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(main_box)
        
        # 创建菜单栏
        self.create_menu_bar()
        main_box.pack_start(self.menu_bar, False, False, 0)
        
        # 创建工具栏
        self.create_tool_bar()
        main_box.pack_start(self.toolbar, False, False, 0)
        
        # 创建中央部件
        self.create_central_widget()
        main_box.pack_start(self.central_paned, True, True, 0)
        
        # 创建状态栏
        self.create_status_bar()
        main_box.pack_start(self.status_bar, False, False, 0)
        
        self.show_all()
    
    def create_menu_bar(self):
        """创建菜单栏"""
        self.menu_bar = Gtk.MenuBar()
        
        # 文件菜单
        file_menu = Gtk.MenuItem.new_with_label(self._("file"))
        file_submenu = Gtk.Menu()
        
        new_profile_item = Gtk.MenuItem.new_with_label(self._("new_profile"))
        new_profile_item.connect("activate", self.new_profile)
        file_submenu.append(new_profile_item)
        
        open_profile_item = Gtk.MenuItem.new_with_label(self._("open_profile"))
        open_profile_item.connect("activate", self.open_profile)
        file_submenu.append(open_profile_item)
        
        save_profile_item = Gtk.MenuItem.new_with_label(self._("save_profile"))
        save_profile_item.connect("activate", self.save_profile)
        file_submenu.append(save_profile_item)
        
        file_submenu.append(Gtk.SeparatorMenuItem())
        
        export_config_item = Gtk.MenuItem.new_with_label(self._("export_config"))
        export_config_item.connect("activate", self.export_config)
        file_submenu.append(export_config_item)
        
        import_config_item = Gtk.MenuItem.new_with_label(self._("import_config"))
        import_config_item.connect("activate", self.import_config)
        file_submenu.append(import_config_item)
        
        file_submenu.append(Gtk.SeparatorMenuItem())
        
        preferences_item = Gtk.MenuItem.new_with_label(self._("preferences"))
        preferences_item.connect("activate", self.preferences)
        file_submenu.append(preferences_item)
        
        file_submenu.append(Gtk.SeparatorMenuItem())
        
        exit_item = Gtk.MenuItem.new_with_label(self._("exit"))
        exit_item.connect("activate", self.close)
        file_submenu.append(exit_item)
        
        file_menu.set_submenu(file_submenu)
        self.menu_bar.append(file_menu)
        
        # 视图菜单
        view_menu = Gtk.MenuItem.new_with_label(self._("view"))
        view_submenu = Gtk.Menu()
        
        dark_theme_item = Gtk.MenuItem.new_with_label(self._("dark_theme"))
        dark_theme_item.connect("activate", lambda x: self.set_theme("dark"))
        view_submenu.append(dark_theme_item)
        
        light_theme_item = Gtk.MenuItem.new_with_label(self._("light_theme"))
        light_theme_item.connect("activate", lambda x: self.set_theme("light"))
        view_submenu.append(light_theme_item)
        
        view_menu.set_submenu(view_submenu)
        self.menu_bar.append(view_menu)
        
        # 构建菜单
        build_menu = Gtk.MenuItem.new_with_label(self._("build"))
        build_submenu = Gtk.Menu()
        
        start_build_item = Gtk.MenuItem.new_with_label(self._("start_build"))
        start_build_item.connect("activate", self.start_build)
        build_submenu.append(start_build_item)
        
        stop_build_item = Gtk.MenuItem.new_with_label(self._("stop_build"))
        stop_build_item.connect("activate", self.stop_build)
        build_submenu.append(stop_build_item)
        
        build_submenu.append(Gtk.SeparatorMenuItem())
        
        clean_item = Gtk.MenuItem.new_with_label(self._("clean"))
        clean_item.connect("activate", self.clean)
        build_submenu.append(clean_item)
        
        install_item = Gtk.MenuItem.new_with_label(self._("install"))
        install_item.connect("activate", self.install)
        build_submenu.append(install_item)
        
        build_menu.set_submenu(build_submenu)
        self.menu_bar.append(build_menu)
        
        # 帮助菜单
        help_menu = Gtk.MenuItem.new_with_label(self._("help"))
        help_submenu = Gtk.Menu()
        
        documentation_item = Gtk.MenuItem.new_with_label(self._("documentation"))
        documentation_item.connect("activate", self.show_documentation)
        help_submenu.append(documentation_item)
        
        about_item = Gtk.MenuItem.new_with_label(self._("about"))
        about_item.connect("activate", self.show_about)
        help_submenu.append(about_item)
        
        help_menu.set_submenu(help_submenu)
        self.menu_bar.append(help_menu)
    
    def create_tool_bar(self):
        """创建工具栏"""
        self.toolbar = Gtk.Toolbar()
        self.toolbar.set_style(Gtk.ToolbarStyle.BOTH_HORIZ)
        
        # 构建按钮
        self.start_build_btn = Gtk.ToolButton.new_from_stock(Gtk.STOCK_EXECUTE)
        self.start_build_btn.set_label(self._("start_build"))
        self.start_build_btn.connect("clicked", self.start_build)
        self.toolbar.insert(self.start_build_btn, 0)
        
        # 停止按钮
        self.stop_build_btn = Gtk.ToolButton.new_from_stock(Gtk.STOCK_STOP)
        self.stop_build_btn.set_label(self._("stop_build"))
        self.stop_build_btn.connect("clicked", self.stop_build)
        self.stop_build_btn.set_sensitive(False)
        self.toolbar.insert(self.stop_build_btn, 1)
        
        self.toolbar.insert(Gtk.SeparatorToolItem(), 2)
        
        # 清理按钮
        clean_btn = Gtk.ToolButton.new_from_stock(Gtk.STOCK_CLEAR)
        clean_btn.set_label(self._("clean"))
        clean_btn.connect("clicked", self.clean)
        self.toolbar.insert(clean_btn, 3)
        
        # 安装按钮
        install_btn = Gtk.ToolButton.new_from_stock(Gtk.STOCK_APPLY)
        install_btn.set_label(self._("install"))
        install_btn.connect("clicked", self.install)
        self.toolbar.insert(install_btn, 4)
        
        self.toolbar.insert(Gtk.SeparatorToolItem(), 5)
        
        # 预设标签
        preset_label = Gtk.Label()
        preset_label.set_text(self._("preset") + ":")
        preset_item = Gtk.ToolItem()
        preset_item.add(preset_label)
        self.toolbar.insert(preset_item, 6)
        
        # 预设下拉框
        self.preset_combo = Gtk.ComboBoxText()
        presets = ["balanced", "release", "debug", "minimal", "performance"]
        for preset in presets:
            self.preset_combo.append_text(self._(preset))
        self.preset_combo.set_active(presets.index(self.current_preset))
        self.preset_combo.connect("changed", self.on_preset_changed)
        preset_item = Gtk.ToolItem()
        preset_item.add(self.preset_combo)
        self.toolbar.insert(preset_item, 7)
        
        self.toolbar.insert(Gtk.SeparatorToolItem(), 8)

        # 语言标签 - 显示当前选择的语言名称
        self.language_label = Gtk.Label()
        self.update_language_label()
        language_item = Gtk.ToolItem()
        language_item.add(self.language_label)
        self.toolbar.insert(language_item, 9)

        # 语言下拉框
        self.language_combo = Gtk.ComboBoxText()
        languages = I18N["zh_CN"]["languages"]
        # 格式: "简体中文" 或 "English"
        for code, name in languages.items():
            self.language_combo.append_text(name)
        self.language_combo.set_active(list(languages.keys()).index(self.current_language))
        self.language_combo.connect("changed", self.on_language_changed)
        language_item = Gtk.ToolItem()
        language_item.add(self.language_combo)
        self.toolbar.insert(language_item, 10)
    
    def create_central_widget(self):
        """创建中央部件"""
        self.central_paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        
        # 左侧：配置选项卡
        left_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        
        # 创建配置选项卡
        self.create_config_tabs()
        left_box.pack_start(self.notebook, True, True, 0)
        
        self.central_paned.pack1(left_box, True, False)
        
        # 右侧：输出和日志
        right_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        
        # 进度条
        self.progress_bar = Gtk.ProgressBar()
        self.progress_bar.set_show_text(True)
        right_box.pack_start(self.progress_bar, False, False, 0)
        
        # 输出和日志标签页
        self.output_notebook = Gtk.Notebook()
        
        # 构建输出
        output_scrolled = Gtk.ScrolledWindow()
        output_scrolled.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        self.output_text = Gtk.TextView()
        self.output_text.set_editable(False)
        self.output_text.set_monospace(True)
        output_scrolled.add(self.output_text)
        self.output_notebook.append_page(output_scrolled, Gtk.Label.new(self._("output")))
        
        # 构建日志
        log_scrolled = Gtk.ScrolledWindow()
        log_scrolled.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        self.log_text = Gtk.TextView()
        self.log_text.set_editable(False)
        self.log_text.set_monospace(True)
        log_scrolled.add(self.log_text)
        self.output_notebook.append_page(log_scrolled, Gtk.Label.new(self._("log")))
        
        right_box.pack_start(self.output_notebook, True, True, 0)
        
        self.central_paned.pack2(right_box, True, False)
        
        # 设置分割比例
        self.central_paned.set_position(500)
    
    def create_config_tabs(self):
        """创建配置选项卡"""
        # 构建配置页
        self.create_build_config_tab()
        
        # 运行时配置页
        self.create_runtime_config_tab()
        
        # 系统限制页
        self.create_system_limits_tab()
        
        # 功能特性页
        self.create_features_tab()
        
        # CPU特性页
        self.create_cpu_features_tab()
        
        # 调度器页
        self.create_scheduler_tab()
        
        # 安全配置页
        self.create_security_tab()
        
        # 内存配置页
        self.create_memory_tab()
        
        # 调试选项页
        self.create_debug_tab()
        
        # 驱动配置页
        self.create_drivers_tab()
        
        # 性能配置页
        self.create_performance_tab()
    
    def create_build_config_tab(self):
        """创建构建配置选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(10)
        
        # 优化级别
        row = 0
        grid.attach(Gtk.Label.new(self._("optimize_level") + ":"), 0, row, 1, 1)
        self.optimize_spin = Gtk.SpinButton()
        self.optimize_spin.set_range(0, 3)
        self.optimize_spin.set_value(2)
        grid.attach(self.optimize_spin, 1, row, 1, 1)
        
        # 调试符号
        row += 1
        self.debug_symbols_check = Gtk.CheckButton.new_with_label(self._("debug_symbols"))
        grid.attach(self.debug_symbols_check, 0, row, 2, 1)
        
        # LTO
        row += 1
        self.lto_check = Gtk.CheckButton.new_with_label(self._("lto"))
        grid.attach(self.lto_check, 0, row, 2, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("build_config")))
    
    def create_runtime_config_tab(self):
        """创建运行时配置选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        label = Gtk.Label()
        label.set_markup("<i>运行时配置通过platform.yaml传递给内核</i>")
        label.set_halign(Gtk.Align.START)
        box.pack_start(label, False, False, 0)
        
        self.notebook.append_page(box, Gtk.Label.new(self._("runtime_config")))
    
    def create_system_limits_tab(self):
        """创建系统限制选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(10)
        
        # 最大域数
        row = 0
        grid.attach(Gtk.Label.new(self._("max_domains") + ":"), 0, row, 1, 1)
        self.max_domains_spin = Gtk.SpinButton()
        self.max_domains_spin.set_range(1, 512)
        self.max_domains_spin.set_value(256)
        grid.attach(self.max_domains_spin, 1, row, 1, 1)
        
        # 最大能力数
        row += 1
        grid.attach(Gtk.Label.new(self._("max_capabilities") + ":"), 0, row, 1, 1)
        self.max_capabilities_spin = Gtk.SpinButton()
        self.max_capabilities_spin.set_range(512, 4096)
        self.max_capabilities_spin.set_value(2048)
        grid.attach(self.max_capabilities_spin, 1, row, 1, 1)
        
        # 最大线程数
        row += 1
        grid.attach(Gtk.Label.new(self._("max_threads") + ":"), 0, row, 1, 1)
        self.max_threads_spin = Gtk.SpinButton()
        self.max_threads_spin.set_range(1, 512)
        self.max_threads_spin.set_value(256)
        grid.attach(self.max_threads_spin, 1, row, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("system_limits")))
    
    def create_features_tab(self):
        """创建功能特性选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(5)
        
        # 功能列表
        self.feature_checks = {}
        features = ["smp", "acpi", "pci", "usb", "virtio", "efi"]
        
        for i, feature in enumerate(features):
            self.feature_checks[feature] = Gtk.CheckButton.new_with_label(self._(feature))
            grid.attach(self.feature_checks[feature], 0, i, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("features")))
    
    def create_cpu_features_tab(self):
        """创建CPU特性选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(5)
        
        self.cpu_feature_checks = {}
        cpu_features = ["MMX", "SSE", "SSE2", "SSE3", "SSSE3", "SSE4.1", "SSE4.2", "AVX", "AVX2", "AES-NI", "RDRAND"]
        
        for i, feature in enumerate(cpu_features):
            self.cpu_feature_checks[feature] = Gtk.CheckButton.new_with_label(feature)
            self.cpu_feature_checks[feature].set_active(True)
            grid.attach(self.cpu_feature_checks[feature], 0, i, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("cpu_features")))
    
    def create_scheduler_tab(self):
        """创建调度器选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(10)
        
        # 调度策略
        row = 0
        grid.attach(Gtk.Label.new(self._("scheduler_policy") + ":"), 0, row, 1, 1)
        self.scheduler_policy_combo = Gtk.ComboBoxText()
        self.scheduler_policy_combo.append_text("priority_rr")
        self.scheduler_policy_combo.append_text("round_robin")
        self.scheduler_policy_combo.append_text("fifo")
        self.scheduler_policy_combo.set_active(0)
        grid.attach(self.scheduler_policy_combo, 1, row, 1, 1)
        
        # 时间片
        row += 1
        grid.attach(Gtk.Label.new(self._("time_slice") + "(ms):"), 0, row, 1, 1)
        self.time_slice_spin = Gtk.SpinButton()
        self.time_slice_spin.set_range(1, 1000)
        self.time_slice_spin.set_value(10)
        grid.attach(self.time_slice_spin, 1, row, 1, 1)
        
        # 抢占式调度
        row += 1
        self.preemptive_check = Gtk.CheckButton.new_with_label(self._("preemptive"))
        self.preemptive_check.set_active(True)
        grid.attach(self.preemptive_check, 0, row, 2, 1)
        
        # 负载均衡
        row += 1
        self.load_balancing_check = Gtk.CheckButton.new_with_label(self._("load_balancing"))
        self.load_balancing_check.set_active(True)
        grid.attach(self.load_balancing_check, 0, row, 2, 1)
        
        # 负载阈值
        row += 1
        grid.attach(Gtk.Label.new(self._("load_threshold") + "(%):"), 0, row, 1, 1)
        self.load_balance_threshold_spin = Gtk.SpinButton()
        self.load_balance_threshold_spin.set_range(1, 100)
        self.load_balance_threshold_spin.set_value(80)
        grid.attach(self.load_balance_threshold_spin, 1, row, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("scheduler")))
    
    def create_security_tab(self):
        """创建安全配置选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(5)
        
        self.security_checks = {}
        security_features = [
            ("KASLR", False),
            ("SMEP", False),
            ("SMAP", False),
            ("audit", True),
            ("form_verification", True),
            ("cap_verify", True),
            ("guard_pages", True),
            ("zero_on_free", True)
        ]
        
        for i, (feature, default) in enumerate(security_features):
            self.security_checks[feature] = Gtk.CheckButton.new_with_label(self._(feature))
            self.security_checks[feature].set_active(default)
            grid.attach(self.security_checks[feature], 0, i, 1, 1)
        
        # 隔离模式
        row = len(security_features)
        grid.attach(Gtk.Label.new(self._("isolation_mode") + ":"), 0, row, 1, 1)
        self.isolation_mode_combo = Gtk.ComboBoxText()
        self.isolation_mode_combo.append_text("strict")
        self.isolation_mode_combo.append_text("permissive")
        self.isolation_mode_combo.set_active(0)
        grid.attach(self.isolation_mode_combo, 1, row, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("security")))
    
    def create_memory_tab(self):
        """创建内存配置选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(10)
        
        # 堆大小
        row = 0
        grid.attach(Gtk.Label.new(self._("heap_size") + "(MB):"), 0, row, 1, 1)
        self.heap_size_spin = Gtk.SpinButton()
        self.heap_size_spin.set_range(16, 4096)
        self.heap_size_spin.set_value(128)
        grid.attach(self.heap_size_spin, 1, row, 1, 1)
        
        # 栈大小
        row += 1
        grid.attach(Gtk.Label.new(self._("stack_size") + "(KB):"), 0, row, 1, 1)
        self.stack_size_spin = Gtk.SpinButton()
        self.stack_size_spin.set_range(4, 64)
        self.stack_size_spin.set_value(8)
        grid.attach(self.stack_size_spin, 1, row, 1, 1)
        
        # 页面缓存
        row += 1
        grid.attach(Gtk.Label.new(self._("page_cache") + "(%):"), 0, row, 1, 1)
        self.page_cache_spin = Gtk.SpinButton()
        self.page_cache_spin.set_range(0, 50)
        self.page_cache_spin.set_value(20)
        grid.attach(self.page_cache_spin, 1, row, 1, 1)
        
        # 缓冲区缓存
        row += 1
        grid.attach(Gtk.Label.new(self._("buffer_cache") + "(KB):"), 0, row, 1, 1)
        self.buffer_cache_spin = Gtk.SpinButton()
        self.buffer_cache_spin.set_range(256, 16384)
        self.buffer_cache_spin.set_value(1024)
        grid.attach(self.buffer_cache_spin, 1, row, 1, 1)
        
        # 最大页表数
        row += 1
        grid.attach(Gtk.Label.new(self._("max_page_tables") + ":"), 0, row, 1, 1)
        self.max_page_tables_spin = Gtk.SpinButton()
        self.max_page_tables_spin.set_range(64, 1024)
        self.max_page_tables_spin.set_value(256)
        grid.attach(self.max_page_tables_spin, 1, row, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("memory")))
    
    def create_debug_tab(self):
        """创建调试选项选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(5)
        
        self.debug_checks = {}
        debug_features = [
            ("console_log", True),
            ("serial_log", True),
            ("panic_on_bug", True),
            ("stack_canary", True),
            ("bounds_check", False),
            ("verbose", False),
            ("trace", False)
        ]
        
        for i, (feature, default) in enumerate(debug_features):
            self.debug_checks[feature] = Gtk.CheckButton.new_with_label(feature)
            self.debug_checks[feature].set_active(default)
            grid.attach(self.debug_checks[feature], 0, i, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("debug_tab")))
    
    def create_drivers_tab(self):
        """创建驱动配置选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(10)
        
        # 驱动列表
        self.driver_checks = {}
        drivers = ["console_driver", "keyboard_driver", "ps2_mouse", "uart_driver"]
        
        for i, driver in enumerate(drivers):
            self.driver_checks[driver] = Gtk.CheckButton.new_with_label(driver)
            self.driver_checks[driver].set_active(True)
            grid.attach(self.driver_checks[driver], 0, i, 1, 1)
        
        # 波特率
        row = len(drivers)
        grid.attach(Gtk.Label.new(self._("baud_rate") + ":"), 0, row, 1, 1)
        self.baud_rate_combo = Gtk.ComboBoxText()
        for rate in ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]:
            self.baud_rate_combo.append_text(rate)
        self.baud_rate_combo.set_active(4)
        grid.attach(self.baud_rate_combo, 1, row, 1, 1)
        
        # 数据位
        row += 1
        grid.attach(Gtk.Label.new(self._("data_bits") + ":"), 0, row, 1, 1)
        self.data_bits_combo = Gtk.ComboBoxText()
        for bits in ["5", "6", "7", "8"]:
            self.data_bits_combo.append_text(bits)
        self.data_bits_combo.set_active(3)
        grid.attach(self.data_bits_combo, 1, row, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("drivers")))
    
    def create_performance_tab(self):
        """创建性能配置选项卡"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(5)
        
        self.performance_checks = {}
        performance_features = [
            ("fast_path", True),
            ("perf_counter", False),
            ("cpu_affinity", True),
            ("numa_opt", False),
            ("latency_opt", False),
            ("throughput_opt", False)
        ]
        
        for i, (feature, default) in enumerate(performance_features):
            self.performance_checks[feature] = Gtk.CheckButton.new_with_label(feature)
            self.performance_checks[feature].set_active(default)
            grid.attach(self.performance_checks[feature], 0, i, 1, 1)
        
        box.pack_start(grid, False, False, 0)
        self.notebook.append_page(box, Gtk.Label.new(self._("performance_tab")))
    
    def create_status_bar(self):
        """创建状态栏"""
        self.status_bar = Gtk.Statusbar()
        self.status_bar_context = self.status_bar.get_context_id("main")
    
    def set_theme(self, theme: str):
        """设置主题"""
        self.current_theme = theme
        self.apply_theme()
    
    def apply_theme(self):
        """应用主题"""
        settings = Gtk.Settings.get_default()
        
        if self.current_theme == "dark":
            settings.set_property("gtk-application-prefer-dark-theme", True)
        else:
            settings.set_property("gtk-application-prefer-dark-theme", False)
    
    def on_language_changed(self, combo):
        """语言改变事件"""
        index = combo.get_active()
        languages = I18N["zh_CN"]["languages"]
        self.current_language = list(languages.keys())[index]

        # 更新语言标签
        self.update_language_label()

        # 重新翻译语言下拉框的选项
        combo.block_handlers()
        combo.remove_all()
        for code, name in languages.items():
            combo.append_text(name)
        combo.set_active(index)
        combo.unblock_handlers()

        self.retranslate_ui()
    
    def on_preset_changed(self, combo):
        """预设改变事件"""
        self.current_preset = combo.get_active_text()
        self.apply_preset()
    
    def apply_preset(self):
        """应用预设配置"""
        preset_map = {
            self._("balanced"): "balanced",
            self._("release"): "release",
            self._("debug"): "debug",
            self._("minimal"): "minimal",
            self._("performance"): "performance"
        }
        
        preset_code = preset_map.get(self.current_preset, "balanced")
        # 这里应该根据预设更新配置
        # 暂时只打印消息
        self.log(f"应用预设: {preset_code}")
    
    def retranslate_ui(self):
        """重新翻译UI"""
        self.set_title(self._("title"))
        
        # 重新翻译菜单
        # GTK的菜单需要重新创建或更新
        # 这里简化处理，只更新状态栏
        self.update_status(self._("ready"))
        
        # 重新翻译预设下拉框
        active = self.preset_combo.get_active()
        
        # 阻止信号触发以避免递归
        self.preset_combo.block_handlers()
        self.preset_combo.remove_all()
        presets = ["balanced", "release", "debug", "minimal", "performance"]
        for preset in presets:
            self.preset_combo.append_text(self._(preset))
        self.preset_combo.set_active(active)
        self.preset_combo.unblock_handlers()
        
        # 重新翻译语言下拉框
        self.update_language_label()
        current_lang_code = self.current_language  # 保存当前语言代码
        active_lang = self.language_combo.get_active()
        
        # 阻止信号触发以避免递归
        combo.block_handlers()
        combo.remove_all()
        languages = I18N.get("zh_CN", {}).get("languages", {})
        lang_code_list = list(languages.keys())
        for code in lang_code_list:
            name = languages[code]
            display_text = f"{name} ({name})"
            combo.append_text(display_text)
        
        # 根据语言代码恢复选择
        if current_lang_code in lang_code_list:
            combo.set_active(lang_code_list.index(current_lang_code))
        elif active_lang < len(lang_code_list):
            combo.set_active(active_lang)
        
        combo.unblock_handlers()
        
        # 确保current_language有效
        if self.current_language not in I18N:
            self.current_language = "zh_CN"  # 默认语言
        
        # 重新翻译标签页
        tab_names = [self._("build_config"), self._("runtime_config"), 
                    self._("system_limits"), self._("features"),
                    self._("cpu_features"), self._("scheduler"), self._("security"), 
                    self._("memory"), self._("debug_tab"), self._("drivers"), 
                    self._("performance_tab")]
        for i, name in enumerate(tab_names):
            if i < self.notebook.get_n_pages():
                page = self.notebook.get_nth_page(i)
                label = self.notebook.get_tab_label(page)
                label.set_text(name)
        
        # 重新翻译输出标签页
        for i in range(self.output_notebook.get_n_pages()):
            page = self.output_notebook.get_nth_page(i)
            label = self.output_notebook.get_tab_label(page)
            tab_names = [self._("output"), self._("log")]
            if i < len(tab_names):
                label.set_text(tab_names[i])
    
    def update_status(self, message: str):
        """更新状态栏"""
        self.status_bar.push(self.status_bar_context, message)
    
    def log(self, message: str, level: str = "info"):
        """添加日志"""
        buffer = self.log_text.get_buffer()
        end_iter = buffer.get_end_iter()
        buffer.insert(end_iter, f"[{level.upper()}] {message}\n")
    
    def append_output(self, text: str):
        """添加输出"""
        buffer = self.output_text.get_buffer()
        end_iter = buffer.get_end_iter()
        buffer.insert(end_iter, text)
    
    def start_build(self):
        """开始构建"""
        if self.is_building:
            return
        
        self.is_building = True
        self.start_build_btn.set_sensitive(False)
        self.stop_build_btn.set_sensitive(True)
        self.update_status(self._("building"))
        
        # 清空输出
        output_buffer = self.output_text.get_buffer()
        output_buffer.set_text("")
        
        log_buffer = self.log_text.get_buffer()
        log_buffer.set_text("")
        
        self.log("开始构建...")
        
        # 在新线程中运行构建
        self.build_thread = threading.Thread(target=self.run_build_thread)
        self.build_thread.start()
    
    def run_build_thread(self):
        """运行构建线程"""
        try:
            process = subprocess.Popen(
                ["make", "all"],
                cwd=str(ROOT_DIR),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1
            )
            
            for line in process.stdout:
                GLib.idle_add(self.append_output, line)
            
            return_code = process.wait()
            
            if return_code == 0:
                GLib.idle_add(self.on_build_success)
            else:
                GLib.idle_add(self.on_build_failed)
                
        except Exception as e:
            GLib.idle_add(self.on_build_error, str(e))
    
    def on_build_success(self):
        """构建成功"""
        self.is_building = False
        self.start_build_btn.set_sensitive(True)
        self.stop_build_btn.set_sensitive(False)
        self.update_status(self._("build_success"))
        self.log("构建成功！", "success")
        self.progress_bar.set_fraction(1.0)
    
    def on_build_failed(self):
        """构建失败"""
        self.is_building = False
        self.start_build_btn.set_sensitive(True)
        self.stop_build_btn.set_sensitive(False)
        self.update_status(self._("build_failed"))
        self.log("构建失败！", "error")
        self.progress_bar.set_fraction(0.0)
    
    def on_build_error(self, error: str):
        """构建错误"""
        self.is_building = False
        self.start_build_btn.set_sensitive(True)
        self.stop_build_btn.set_sensitive(False)
        self.update_status(self._("build_failed"))
        self.log(f"构建错误: {error}", "error")
        self.progress_bar.set_fraction(0.0)
    
    def stop_build(self):
        """停止构建"""
        if self.build_thread and self.build_thread.is_alive():
            self.is_building = False
            self.log("正在停止构建...", "warning")
            # 这里应该终止构建进程
            # 暂时只设置状态
            self.start_build_btn.set_sensitive(True)
            self.stop_build_btn.set_sensitive(False)
            self.update_status(self._("build_stopped"))
    
    def clean(self):
        """清理构建"""
        self.log("清理构建文件...")
        try:
            subprocess.run(["make", "clean"], cwd=str(ROOT_DIR), check=True)
            self.log("清理完成", "success")
        except subprocess.CalledProcessError as e:
            self.log(f"清理失败: {e}", "error")
    
    def install(self):
        """安装"""
        self.log("安装构建产物...")
        try:
            subprocess.run(["make", "install"], cwd=str(ROOT_DIR), check=True)
            self.log("安装完成", "success")
        except subprocess.CalledProcessError as e:
            self.log(f"安装失败: {e}", "error")
    
    # 以下为占位函数
    def new_profile(self, widget):
        pass
    
    def open_profile(self, widget):
        pass
    
    def save_profile(self, widget):
        pass
    
    def export_config(self, widget):
        pass
    
    def import_config(self, widget):
        pass
    
    def preferences(self, widget):
        pass
    
    def show_documentation(self, widget):
        pass
    
    def show_about(self, widget):
        dialog = Gtk.AboutDialog()
        dialog.set_program_name(PROJECT)
        dialog.set_version(VERSION)
        dialog.set_comments("HIC内核构建系统")
        dialog.run()
        dialog.destroy()


class HICBuildApp(Gtk.Application):
    """HIC构建系统应用"""
    
    def __init__(self):
        super().__init__(application_id="com.hic.buildsystem")
    
    def do_activate(self):
        win = HICBuildGUI(self)
        win.present()


def main():
    """主函数"""
    app = HICBuildApp()
    app.run(sys.argv)


if __name__ == "__main__":
    main()


class BuildConfigDialog(Gtk.Dialog):
    """构建配置对话框"""
    
    def __init__(self, parent, build_system):
        super().__init__(
            title="HIC内核配置",
            transient_for=parent,
            flags=0
        )
        self.build_system = build_system
        self.config_vars = {}
        
        self.add_button("取消", Gtk.ResponseType.CANCEL)
        self.add_button("应用", Gtk.ResponseType.APPLY)
        self.add_button("确定", Gtk.ResponseType.OK)
        
        self.set_default_size(700, 550)
        self.set_border_width(10)
        
        # 创建配置界面
        self.create_config_ui()
        
        # 加载当前配置
        self.load_config()
        
        # 显示欢迎信息
        self.show_welcome_info()
    
    def create_config_ui(self):
        """创建配置界面"""
        # 主容器
        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.get_content_area().pack_start(main_box, True, True, 0)
        
        # 欢迎信息区域
        self.welcome_frame = Gtk.Frame()
        welcome_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        welcome_box.set_margin_top(10)
        welcome_box.set_margin_bottom(10)
        welcome_box.set_margin_start(10)
        welcome_box.set_margin_end(10)
        
        welcome_label = Gtk.Label()
        welcome_label.set_markup("<b>🎯 欢迎使用HIC内核配置工具</b>")
        welcome_label.set_halign(Gtk.Align.START)
        welcome_box.pack_start(welcome_label, False, False, 0)
        
        info_label = Gtk.Label()
        info_label.set_markup("<small>通过这些选项来自定义内核的行为和特性。修改后需要重新编译才能生效。</small>")
        info_label.set_halign(Gtk.Align.START)
        info_label.set_wrap(True)
        welcome_box.pack_start(info_label, False, False, 0)
        
        self.welcome_frame.add(welcome_box)
        main_box.pack_start(self.welcome_frame, False, False, 0)
        
        # 创建笔记本（标签页）
        notebook = Gtk.Notebook()
        main_box.pack_start(notebook, True, True, 0)
        
        # 创建各个配置页
        self.create_debug_page(notebook)
        self.create_security_page(notebook)
        self.create_performance_page(notebook)
        self.create_memory_page(notebook)
        self.create_scheduler_page(notebook)
        self.create_feature_page(notebook)
        self.create_capability_page(notebook)
        self.create_domain_page(notebook)
        self.create_interrupt_page(notebook)
        self.create_module_page(notebook)
    
    def show_welcome_info(self):
        """显示欢迎信息"""
    
    def show_welcome_info(self):
        """显示欢迎信息"""
        pass  # 欢迎信息已在create_config_ui中添加
    
    def create_check_button(self, parent, label_text, tooltip_text):
        """创建复选框"""
        check = Gtk.CheckButton.new_with_label(label_text)
        check.set_tooltip_text(tooltip_text)
        parent.pack_start(check, False, False, 0)
        return check
    
    def create_spin_button(self, parent, label_text, min_val, max_val, default_val):
        """创建数字输入框"""
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        
        label = Gtk.Label.new(label_text)
        label.set_halign(Gtk.Align.START)
        hbox.pack_start(label, False, False, 0)
        
        spin = Gtk.SpinButton()
        spin.set_range(min_val, max_val)
        spin.set_value(default_val)
        spin.set_numeric(True)
        hbox.pack_start(spin, True, True, 0)
        
        parent.pack_start(hbox, False, False, 0)
        return spin
    
    def create_debug_page(self, notebook):
        """创建调试配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        # 标题和描述
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        title_label = Gtk.Label()
        title_label.set_markup("<b>调试配置</b>")
        title_label.set_halign(Gtk.Align.START)
        title_box.pack_start(title_label, False, False, 0)
        
        desc_label = Gtk.Label()
        desc_label.set_markup("<small>调试功能和日志输出</small>")
        desc_label.set_halign(Gtk.Align.START)
        title_box.pack_start(desc_label, True, True, 0)
        box.pack_start(title_box, False, False, 0)
        
        # 分隔线
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(separator, False, False, 5)
        
        # 调试支持
        self.config_vars['CONFIG_DEBUG'] = self.create_check_button(
            box, "启用调试支持", "添加调试符号和调试信息，方便使用调试器"
        )
        
        # 跟踪功能
        self.config_vars['CONFIG_TRACE'] = self.create_check_button(
            box, "启用跟踪功能", "记录函数调用跟踪信息，用于性能分析"
        )
        
        # 详细输出
        self.config_vars['CONFIG_VERBOSE'] = self.create_check_button(
            box, "启用详细输出", "显示详细的编译和运行信息"
        )
        
        notebook.append_page(box, Gtk.Label.new(self._("debug_tab")))
    
    def create_security_page(self, notebook):
        """创建安全配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        # 标题和描述
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        title_label = Gtk.Label()
        title_label.set_markup("<b>安全配置</b>")
        title_label.set_halign(Gtk.Align.START)
        title_box.pack_start(title_label, False, False, 0)
        
        desc_label = Gtk.Label()
        desc_label.set_markup("<small>内核安全防护机制和访问控制</small>")
        desc_label.set_halign(Gtk.Align.START)
        title_box.pack_start(desc_label, True, True, 0)
        box.pack_start(title_box, False, False, 0)
        
        # 分隔线
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(separator, False, False, 5)
        
        # KASLR
        self.config_vars['CONFIG_KASLR'] = self.create_check_button(
            box, "启用KASLR", "内核地址空间布局随机化，增加攻击难度"
        )
        
        # SMEP
        self.config_vars['CONFIG_SMEP'] = self.create_check_button(
            box, "启用SMEP", "禁止从用户态执行内核代码，防止权限提升"
        )
        
        # SMAP
        self.config_vars['CONFIG_SMAP'] = self.create_check_button(
            box, "启用SMAP", "禁止内核访问用户态内存，防止数据泄露"
        )
        
        # 审计日志
        self.config_vars['CONFIG_AUDIT'] = self.create_check_button(
            box, "启用审计日志", "记录安全相关事件，便于安全审计"
        )
        
        # 安全级别
        security_levels = Gtk.ListStore(str)
        level_descriptions = {
            "minimal": "最低安全",
            "standard": "标准安全",
            "strict": "严格安全"
        }
        for level in ["minimal", "standard", "strict"]:
            security_levels.append([level_descriptions.get(level, level)])
        
        combo = Gtk.ComboBox.new_with_model(security_levels)
        renderer = Gtk.CellRendererText()
        combo.pack_start(renderer, True)
        combo.add_attribute(renderer, "text", 0)
        
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        label = Gtk.Label.new("安全级别:")
        label.set_halign(Gtk.Align.START)
        hbox.pack_start(label, False, False, 0)
        hbox.pack_start(combo, True, True, 0)
        box.pack_start(hbox, False, False, 0)
        
        self.config_vars['CONFIG_SECURITY_LEVEL'] = combo
        
        notebook.append_page(box, Gtk.Label.new(self._("security")))
    
    def create_performance_page(self, notebook):
        """创建性能配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        # 标题和描述
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        title_label = Gtk.Label()
        title_label.set_markup("<b>性能配置</b>")
        title_label.set_halign(Gtk.Align.START)
        title_box.pack_start(title_label, False, False, 0)
        
        desc_label = Gtk.Label()
        desc_label.set_markup("<small>性能优化和监控选项</small>")
        desc_label.set_halign(Gtk.Align.START)
        title_box.pack_start(desc_label, True, True, 0)
        box.pack_start(title_box, False, False, 0)
        
        # 分隔线
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(separator, False, False, 5)
        
        # 性能计数器
        self.config_vars['CONFIG_PERF'] = self.create_check_button(
            box, "启用性能计数器", "启用CPU性能计数器，用于性能分析"
        )
        
        # 快速路径
        self.config_vars['CONFIG_FAST_PATH'] = self.create_check_button(
            box, "启用快速路径", "优化常见操作路径，提升响应速度"
        )
        
        notebook.append_page(box, Gtk.Label.new(self._("performance_tab")))
    
    def create_memory_page(self, notebook):
        """创建内存配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        # 标题和描述
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        title_label = Gtk.Label()
        title_label.set_markup("<b>内存配置</b>")
        title_label.set_halign(Gtk.Align.START)
        title_box.pack_start(title_label, False, False, 0)
        
        desc_label = Gtk.Label()
        desc_label.set_markup("<small>内存分配和管理策略</small>")
        desc_label.set_halign(Gtk.Align.START)
        title_box.pack_start(desc_label, True, True, 0)
        box.pack_start(title_box, False, False, 0)
        
        # 分隔线
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(separator, False, False, 5)
        
        # 堆大小
        self.config_vars['CONFIG_HEAP_SIZE_MB'] = self.create_spin_button_with_hint(
            box, "堆大小 (MB):", 16, 4096, 128, "建议值: 128-512MB，根据可用内存调整"
        )
        
        # 栈大小
        self.config_vars['CONFIG_STACK_SIZE_KB'] = self.create_spin_button_with_hint(
            box, "栈大小 (KB):", 4, 64, 8, "建议值: 8-16KB，大多数应用足够"
        )
        
        # 页面缓存
        self.config_vars['CONFIG_PAGE_CACHE_PERCENT'] = self.create_spin_button_with_hint(
            box, "页面缓存 (%):", 0, 50, 20, "建议值: 20-30%，提升文件系统性能"
        )
        
        notebook.append_page(box, Gtk.Label.new(self._("memory")))
    
    def create_spin_button_with_hint(self, parent, label_text, min_val, max_val, default_val, hint_text):
        """创建带提示的数字输入框"""
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        label = Gtk.Label.new(label_text)
        label.set_halign(Gtk.Align.START)
        hbox.pack_start(label, False, False, 0)
        
        spin = Gtk.SpinButton()
        spin.set_range(min_val, max_val)
        spin.set_value(default_val)
        spin.set_numeric(True)
        hbox.pack_start(spin, True, True, 0)
        
        vbox.pack_start(hbox, False, False, 0)
        
        # 添加提示文本
        hint_label = Gtk.Label()
        hint_label.set_markup(f"<small><i>{hint_text}</i></small>")
        hint_label.set_halign(Gtk.Align.START)
        hint_label.set_wrap(True)
        vbox.pack_start(hint_label, False, False, 0)
        
        parent.pack_start(vbox, False, False, 0)
        return spin
    
    def create_feature_page(self, notebook):
        """创建功能配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        # 标题和描述
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        title_label = Gtk.Label()
        title_label.set_markup("<b>功能配置</b>")
        title_label.set_halign(Gtk.Align.START)
        title_box.pack_start(title_label, False, False, 0)
        
        desc_label = Gtk.Label()
        desc_label.set_markup("<small>硬件支持和功能模块</small>")
        desc_label.set_halign(Gtk.Align.START)
        title_box.pack_start(desc_label, True, True, 0)
        box.pack_start(title_box, False, False, 0)
        
        # 分隔线
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(separator, False, False, 5)
        
        # PCI支持
        self.config_vars['CONFIG_PCI'] = self.create_check_button(
            box, "启用PCI支持", "支持PCI设备，如网卡、显卡等"
        )
        
        # ACPI支持
        self.config_vars['CONFIG_ACPI'] = self.create_check_button(
            box, "启用ACPI支持", "支持ACPI电源管理和硬件配置"
        )
        
        # 串口支持
        self.config_vars['CONFIG_SERIAL'] = self.create_check_button(
            box, "启用串口支持", "支持串口控制台输出，便于调试"
        )
        
        notebook.append_page(box, Gtk.Label.new("功能"))
    
    def create_capability_page(self, notebook):
        """创建能力系统配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        label = Gtk.Label()
        label.set_markup("<b>能力系统配置</b>")
        label.set_halign(Gtk.Align.START)
        box.pack_start(label, False, False, 0)
        
        # 最大能力数量
        self.config_vars['CONFIG_MAX_CAPABILITIES'] = self.create_spin_button(
            box, "最大能力数量:", 1024, 1048576, 65536
        )
        
        # 能力派生
        self.config_vars['CONFIG_CAPABILITY_DERIVATION'] = self.create_check_button(
            box, "启用能力派生", "允许从现有能力派生新能力"
        )
        
        notebook.append_page(box, Gtk.Label.new("能力系统"))
    
    def create_domain_page(self, notebook):
        """创建域配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        label = Gtk.Label()
        label.set_markup("<b>域配置</b>")
        label.set_halign(Gtk.Align.START)
        box.pack_start(label, False, False, 0)
        
        # 最大域数量
        self.config_vars['CONFIG_MAX_DOMAINS'] = self.create_spin_button(
            box, "最大域数量:", 1, 128, 16
        )
        
        # 域栈大小
        self.config_vars['CONFIG_DOMAIN_STACK_SIZE_KB'] = self.create_spin_button(
            box, "域栈大小 (KB):", 8, 64, 16
        )
        
        notebook.append_page(box, Gtk.Label.new("域"))
    
    def create_interrupt_page(self, notebook):
        """创建中断配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        label = Gtk.Label()
        label.set_markup("<b>中断配置</b>")
        label.set_halign(Gtk.Align.START)
        box.pack_start(label, False, False, 0)
        
        # 最大中断数
        self.config_vars['CONFIG_MAX_IRQS'] = self.create_spin_button(
            box, "最大中断数:", 64, 1024, 256
        )
        
        # 中断公平性
        self.config_vars['CONFIG_IRQ_FAIRNESS'] = self.create_check_button(
            box, "启用中断公平性", "确保中断处理的公平性"
        )
        
        notebook.append_page(box, Gtk.Label.new("中断"))
    
    def create_module_page(self, notebook):
        """创建模块配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        # 标题和描述
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        title_label = Gtk.Label()
        title_label.set_markup("<b>模块配置</b>")
        title_label.set_halign(Gtk.Align.START)
        title_box.pack_start(title_label, False, False, 0)
        
        desc_label = Gtk.Label()
        desc_label.set_markup("<small>内核模块加载和管理</small>")
        desc_label.set_halign(Gtk.Align.START)
        title_box.pack_start(desc_label, True, True, 0)
        box.pack_start(title_box, False, False, 0)
        
        # 分隔线
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(separator, False, False, 5)
        
        # 模块加载
        self.config_vars['CONFIG_MODULE_LOADING'] = self.create_check_button(
            box, "启用模块加载", "允许在运行时加载内核模块"
        )
        
        # 最大模块数
        self.config_vars['CONFIG_MAX_MODULES'] = self.create_spin_button_with_hint(
            box, "最大模块数:", 0, 256, 32, "建议值: 16-64，根据需求调整"
        )
        
        notebook.append_page(box, Gtk.Label.new("模块"))
        
    def create_scheduler_page(self, notebook):
        """创建调度器配置页"""
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(10)
        box.set_margin_bottom(10)
        box.set_margin_start(10)
        box.set_margin_end(10)
        
        # 标题和描述
        title_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        title_label = Gtk.Label()
        title_label.set_markup("<b>调度器配置</b>")
        title_label.set_halign(Gtk.Align.START)
        title_box.pack_start(title_label, False, False, 0)
        
        desc_label = Gtk.Label()
        desc_label.set_markup("<small>线程调度和任务管理策略</small>")
        desc_label.set_halign(Gtk.Align.START)
        title_box.pack_start(desc_label, True, True, 0)
        box.pack_start(title_box, False, False, 0)
        
        # 分隔线
        separator = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(separator, False, False, 5)
        
        # 调度策略
        policies = Gtk.ListStore(str)
        policy_descriptions = {
            "fifo": "FIFO - 先进先出",
            "rr": "轮转调度",
            "priority": "优先级调度"
        }
        for policy in ["fifo", "rr", "priority"]:
            policies.append([policy_descriptions.get(policy, policy)])
        
        combo = Gtk.ComboBox.new_with_model(policies)
        renderer = Gtk.CellRendererText()
        combo.pack_start(renderer, True)
        combo.add_attribute(renderer, "text", 0)
        
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        label = Gtk.Label.new(self._("scheduler_policy") + ":")
        label.set_halign(Gtk.Align.START)
        hbox.pack_start(label, False, False, 0)
        hbox.pack_start(combo, True, True, 0)
        box.pack_start(hbox, False, False, 0)
        
        self.config_vars['CONFIG_SCHEDULER_POLICY'] = combo
        
        # 时间片
        self.config_vars['CONFIG_TIME_SLICE_MS'] = self.create_spin_button_with_hint(
            box, "时间片 (毫秒):", 1, 1000, 10, "建议值: 10-50ms，影响响应速度"
        )
        
        # 最大线程数
        self.config_vars['CONFIG_MAX_THREADS'] = self.create_spin_button_with_hint(
            box, "最大线程数:", 1, 1024, 256, "建议值: 128-512，根据CPU核心数调整"
        )
        
        notebook.append_page(box, Gtk.Label.new(self._("scheduler")))
    
    def load_config(self):
        """加载当前配置"""
        config_file = os.path.join(ROOT_DIR, "..", "build_config.mk")
        
        if not os.path.exists(config_file):
            return
        
        with open(config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith('CONFIG_') and '=' in line:
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip().rstrip('?')
                    
                    if key in self.config_vars:
                        widget = self.config_vars[key]
                        
                        if isinstance(widget, Gtk.CheckButton):
                            widget.set_active(value == '1')
                        elif isinstance(widget, Gtk.SpinButton):
                            try:
                                widget.set_value(int(value))
                            except ValueError:
                                pass
                        elif isinstance(widget, Gtk.ComboBox):
                            # 设置组合框的值
                            pass  # 需要实现
    
    def save_config(self):
        """保存配置"""
        config_file = os.path.join(ROOT_DIR, "..", "build_config.mk")
        
        # 读取原文件
        lines = []
        if os.path.exists(config_file):
            with open(config_file, 'r') as f:
                lines = f.readlines()
        
        # 更新配置值
        config_values = {}
        for key, widget in self.config_vars.items():
            if isinstance(widget, Gtk.CheckButton):
                config_values[key] = '1' if widget.get_active() else '0'
            elif isinstance(widget, Gtk.SpinButton):
                config_values[key] = str(int(widget.get_value()))
            elif isinstance(widget, Gtk.ComboBox):
                # 获取组合框的值
                pass  # 需要实现
        
        # 更新文件内容
        new_lines = []
        for line in lines:
            stripped = line.strip()
            if stripped.startswith('CONFIG_') and '=' in stripped:
                key = stripped.split('=', 1)[0].strip()
                if key in config_values:
                    new_lines.append(f"{key} ?= {config_values[key]}\n")
                    continue
            new_lines.append(line)
        
        # 写回文件
        with open(config_file, 'w') as f:
            f.writelines(new_lines)
        
        return True
    
    def run(self):
        """运行对话框"""
        response = super().run()
        
        if response in [Gtk.ResponseType.APPLY, Gtk.ResponseType.OK]:
            self.save_config()
        
        self.destroy()
        return response


class BuildLogTextView(Gtk.TextView):
    """构建日志文本视图"""
    
    def __init__(self):
        super().__init__()
        self.set_editable(False)
        self.set_wrap_mode(Gtk.WrapMode.WORD)
        self.set_left_margin(10)
        self.set_right_margin(10)
        self.set_top_margin(10)
        self.set_bottom_margin(10)
        
        # 创建标签
        self.buffer = self.get_buffer()
        self.tag_default = self.buffer.create_tag("default")
        self.tag_default.set_property("foreground", "black")
        
        self.tag_error = self.buffer.create_tag("error")
        self.tag_error.set_property("foreground", "red")
        
        self.tag_success = self.buffer.create_tag("success")
        self.tag_success.set_property("foreground", "green")
        
        self.tag_warning = self.buffer.create_tag("warning")
        self.tag_warning.set_property("foreground", "orange")
        
        self.tag_info = self.buffer.create_tag("info")
        self.tag_info.set_property("foreground", "blue")
    
    def log(self, message: str, level: str = "info"):
        """添加日志消息"""
        end_iter = self.buffer.get_end_iter()
        
        # 选择标签
        if level == "error":
            tag = self.tag_error
        elif level == "success":
            tag = self.tag_success
        elif level == "warning":
            tag = self.tag_warning
        elif level == "info":
            tag = self.tag_info
        else:
            tag = self.tag_default
        
        # 添加文本
        self.buffer.insert_with_tags(end_iter, f"[{level.upper()}] {message}\n", tag)
        
        # 自动滚动到底部
        self.scroll_to_mark(self.buffer.get_insert(), 0.25, False, 0.0, 1.0)
    
    def clear(self):
        """清空日志"""
        self.buffer.set_text("")


class BuildButton(Gtk.Button):
    """构建按钮"""
    
    def __init__(self, label: str, icon_name: str, callback):
        super().__init__()
        self.callback = callback
        self.set_sensitive(True)
        
        # 创建图标
        image = Gtk.Image()
        image.set_from_icon_name(icon_name, Gtk.IconSize.BUTTON)
        
        # 创建标签
        label_widget = Gtk.Label.new(label)
        
        # 创建水平布局
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        box.pack_start(image, False, False, 0)
        box.pack_start(label_widget, False, False, 0)
        
        self.add(box)
        self.connect("clicked", self.on_clicked)
    
    def on_clicked(self, button):
        """按钮点击事件"""
        self.callback()
    
    def set_building(self, building: bool):
        """设置构建状态"""
        self.set_sensitive(not building)


class BuildWindow(Gtk.Window):
    """构建系统主窗口"""
    
    def __init__(self):
        super().__init__(title=f"{PROJECT} 构建系统 v{VERSION}")
        self.set_border_width(10)
        self.set_default_size(800, 600)
        self.set_position(Gtk.WindowPosition.CENTER)
        
        self.is_building = False
        self.build_thread: Optional[threading.Thread] = None
        
        # 创建UI
        self.create_ui()
    
    def create_ui(self):
        """创建用户界面"""
        # 主垂直布局
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.add(vbox)
        
        # 标题
        title_label = Gtk.Label()
        title_label.set_markup(f"<big><b>{PROJECT} 构建系统 v{VERSION}</b></big>")
        title_label.set_margin_top(10)
        title_label.set_margin_bottom(10)
        vbox.pack_start(title_label, False, False, 0)
        
        # 按钮网格
        button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        button_box.set_homogeneous(True)
        vbox.pack_start(button_box, False, False, 0)
        
        # 创建按钮
        self.btn_config = BuildButton("配置选项", "preferences-system", self.show_config_dialog)
        button_box.pack_start(self.btn_config, True, True, 0)
        
        self.btn_console = BuildButton("命令行构建", "system-run", self.build_console)
        button_box.pack_start(self.btn_console, True, True, 0)
        
        self.btn_tui = BuildButton("文本GUI构建", "terminal", self.build_tui)
        button_box.pack_start(self.btn_tui, True, True, 0)
        
        self.btn_gui = BuildButton("图形化GUI构建", "video-display", self.build_gui)
        button_box.pack_start(self.btn_gui, True, True, 0)
        
        # 第二行按钮
        button_box2 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        button_box2.set_homogeneous(True)
        vbox.pack_start(button_box2, False, False, 0)
        
        self.btn_clean = BuildButton("清理构建", "edit-clear", self.clean_build)
        button_box2.pack_start(self.btn_clean, True, True, 0)
        
        self.btn_deps = BuildButton("安装依赖", "package-x-generic", self.install_deps)
        button_box2.pack_start(self.btn_deps, True, True, 0)
        
        self.btn_help = BuildButton("帮助", "help-browser", self.show_help)
        button_box2.pack_start(self.btn_help, True, True, 0)
        
        # 状态栏
        self.status_bar = Gtk.Statusbar()
        self.status_bar.set_margin_top(10)
        self.status_bar.set_margin_bottom(10)
        vbox.pack_start(self.status_bar, False, False, 0)
        
        # 日志区域
        log_frame = Gtk.Frame(label="构建日志")
        log_frame.set_margin_top(10)
        vbox.pack_start(log_frame, True, True, 0)
        
        # 滚动窗口
        scrolled_window = Gtk.ScrolledWindow()
        scrolled_window.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        scrolled_window.set_min_content_height(300)
        log_frame.add(scrolled_window)
        
        # 日志文本视图
        self.log_view = BuildLogTextView()
        scrolled_window.add(self.log_view)
        
        # 显示所有组件
        self.show_all()
    
    def set_building_state(self, building: bool):
        """设置构建状态"""
        self.is_building = building
        
        # 更新按钮状态
        self.btn_console.set_building(building)
        self.btn_tui.set_building(building)
        self.btn_gui.set_building(building)
        self.btn_clean.set_building(building)
        self.btn_deps.set_building(building)
        self.btn_help.set_building(building)
        
        # 更新状态栏
        if building:
            self.update_status("正在构建中...", "warning")
        else:
            self.update_status("就绪", "default")
    
    def update_status(self, message: str, level: str = "default"):
        """更新状态栏"""
        context_id = self.status_bar.get_context_id("build-status")
        self.status_bar.push(context_id, message)
    
    def log(self, message: str, level: str = "info"):
        """添加日志"""
        GLib.idle_add(self.log_view.log, message, level)
    
    def run_command(self, command: List[str]) -> bool:
        """运行命令"""
        try:
            self.log(f"执行命令: {' '.join(command)}", "info")
            
            process = subprocess.Popen(
                command,
                cwd=ROOT_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True
            )
            
            # 实时读取输出
            while True:
                output = process.stdout.readline()
                if output == '' and process.poll() is not None:
                    break
                if output:
                    self.log(output.strip(), "info")
            
            return_code = process.wait()
            
            if return_code == 0:
                self.log("命令执行成功", "success")
                return True
            else:
                self.log(f"命令执行失败，返回码: {return_code}", "error")
                return False
                
        except Exception as e:
            self.log(f"执行命令时出错: {str(e)}", "error")
            return False
    
    def build_console(self):
        """命令行模式构建"""
        self.run_build_task("命令行模式构建", ["make", "clean"], ["make", "BUILD_TYPE=console"])
    
    def build_tui(self):
        """文本GUI模式构建"""
        # 检查ncurses支持
        try:
            import curses
            # 测试curses是否可用
            curses.initscr()
            curses.endwin()
        except Exception as e:
            self.log(f"错误: ncurses支持不可用 - {e}", "error")
            self.update_status("缺少依赖", "error")
            return
        
        self.run_build_task("文本GUI模式构建", ["make", "clean"], ["make", "BUILD_TYPE=tui"])
    
    def build_gui(self):
        """图形化GUI模式构建"""
        # GTK3依赖已在文件导入时检查，这里直接构建
        self.run_build_task("图形化GUI模式构建", ["make", "clean"], ["make", "BUILD_TYPE=gui"])
    
    def clean_build(self):
        """清理构建"""
        self.run_build_task("清理构建", ["make", "clean"])
    
    def install_deps(self):
        """安装依赖"""
        if not os.path.exists("/etc/arch-release"):
            self.log("错误: 此脚本仅适用于 Arch Linux", "error")
            self.update_status("不支持的系统", "error")
            return
        
        self.run_build_task("安装依赖", ["sudo", "pacman", "-S", "--needed", "base-devel", 
                          "git", "mingw-w64-gcc", "gnu-efi", "ncurses", "gtk3"])
    
    def show_config_dialog(self):
        """显示配置对话框"""
        if self.is_building:
            self.log("正在构建中，无法修改配置", "warning")
            return
        
        dialog = BuildConfigDialog(self, self)
        response = dialog.run()
        
        if response in [Gtk.ResponseType.APPLY, Gtk.ResponseType.OK]:
            self.log("配置已保存", "success")
            self.log("需要重新编译才能使配置生效", "info")
    
    def show_help(self):
        """显示帮助"""
        help_dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="HIC系统构建系统帮助"
        )
        help_dialog.format_secondary_text(
            "命令行模式: make 或 make console\n"
            "文本GUI模式: make tui 或 ./build_tui.py\n"
            "图形化GUI模式: make gui 或 ./build_gui.py\n"
            "清理: make clean\n"
            "安装依赖: make deps-arch\n"
            "帮助: make help"
        )
        help_dialog.run()
        help_dialog.destroy()
    
    def run_build_task(self, task_name: str, *commands: List[List[str]]):
        """运行构建任务"""
        if self.is_building:
            self.log("正在构建中，请等待", "warning")
            return
        
        self.set_building_state(True)
        self.log_view.clear()
        self.log(f"开始{task_name}...", "info")
        
        # 在后台线程中运行构建
        def build_thread():
            success = True
            for cmd in commands:
                if not self.run_command(cmd):
                    success = False
                    break
            
            # 恢复UI状态
            GLib.idle_add(self.on_build_complete, success, task_name)
        
        self.build_thread = threading.Thread(target=build_thread)
        self.build_thread.start()
    
    def on_build_complete(self, success: bool, task_name: str):
        """构建完成回调"""
        if success:
            self.log(f"{task_name}完成!", "success")
            self.update_status(f"{task_name}成功", "success")
        else:
            self.log(f"{task_name}失败!", "error")
            self.update_status(f"{task_name}失败", "error")
        
        self.set_building_state(False)


def main():
    """主函数"""
    app = BuildWindow()
    app.connect("destroy", Gtk.main_quit)
    Gtk.main()


if __name__ == "__main__":
    main()