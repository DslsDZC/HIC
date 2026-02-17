#!/usr/bin/python3
"""
HIC系统构建系统 - 图形化GUI模式
使用GTK3库实现图形用户界面
遵循TD/滚动更新.md文档
"""

import sys
import subprocess
import os
import threading
from typing import Optional, List

try:
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk, GLib, Pango
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
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT_DIR, "build")
OUTPUT_DIR = os.environ.get("OUTPUT_DIR", os.path.join(ROOT_DIR, "output"))


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
        
        notebook.append_page(box, Gtk.Label.new("调试"))
    
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
        
        notebook.append_page(box, Gtk.Label.new("安全"))
    
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
        
        notebook.append_page(box, Gtk.Label.new("性能"))
    
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
        
        notebook.append_page(box, Gtk.Label.new("内存"))
    
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
        label = Gtk.Label.new("调度策略:")
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
        
        notebook.append_page(box, Gtk.Label.new("调度器"))
    
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