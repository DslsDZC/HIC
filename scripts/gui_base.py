#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统 GUI 抽象基类
提供统一的接口，支持多种 GUI 框架
"""

import sys
import os
import subprocess
import threading
import json
from pathlib import Path
from typing import Dict, List, Optional, Any, Callable
from datetime import datetime
from abc import ABC, abstractmethod

# 项目信息
PROJECT = "HIC System"
VERSION = "0.1.0"
ROOT_DIR = Path(__file__).parent.parent
BUILD_DIR = ROOT_DIR / "build"
OUTPUT_DIR = ROOT_DIR / "output"
PLATFORM_YAML = ROOT_DIR / "src" / "bootloader" / "platform.yaml"

# 导入样式管理器
from style_manager import get_style_manager

# 加载翻译
def load_translations():
    """从translations文件夹加载翻译"""
    translations_dir = Path(__file__).parent / "translations"
    
    try:
        import yaml
        
        I18N = {}
        language_keys = {}
        language_display_names = {}
        
        # 加载翻译键
        keys_file = translations_dir / "_keys.yaml"
        if keys_file.exists():
            with open(keys_file, 'r', encoding='utf-8') as f:
                keys_data = yaml.safe_load(f)
                for key in keys_data.get('language_keys', []):
                    language_keys[key] = key
        
        # 加载语言显示名称
        display_names_file = translations_dir / "_display_names.yaml"
        if display_names_file.exists():
            with open(display_names_file, 'r', encoding='utf-8') as f:
                display_names_data = yaml.safe_load(f)
                language_display_names = display_names_data.get('language_display_names', {})
        
        # 加载所有语言文件
        for lang_file in translations_dir.glob("*.yaml"):
            if lang_file.name.startswith('_'):
                continue
            
            lang_code = lang_file.stem
            with open(lang_file, 'r', encoding='utf-8') as f:
                translations = yaml.safe_load(f)
                I18N[lang_code] = translations
        
        if I18N:
            return I18N, language_keys, language_display_names
            
    except Exception as e:
        print(f"警告: 加载翻译文件失败 ({e}), 使用默认英语翻译")
    
    return {"en_US": {}}, {}, {}

I18N, LANGUAGE_KEYS, LANGUAGE_DISPLAY_NAMES = load_translations()


class GUIBackend(ABC):
    """GUI 后端抽象基类"""

    @abstractmethod
    def create_window(self, title: str, width: int, height: int) -> Any:
        """创建主窗口"""
        pass

    @abstractmethod
    def create_button(self, text: str, callback: Callable) -> Any:
        """创建按钮"""
        pass

    @abstractmethod
    def create_label(self, text: str) -> Any:
        """创建标签"""
        pass

    @abstractmethod
    def create_text_area(self, readonly: bool = True) -> Any:
        """创建文本区域"""
        pass

    @abstractmethod
    def create_progress_bar(self) -> Any:
        """创建进度条"""
        pass

    @abstractmethod
    def create_combo_box(self, items: List[str], callback: Callable) -> Any:
        """创建下拉框"""
        pass

    @abstractmethod
    def create_checkbox(self, text: str, callback: Callable) -> Any:
        """创建复选框"""
        pass

    @abstractmethod
    def create_spin_box(self, min_val: int, max_val: int, value: int, callback: Callable) -> Any:
        """创建数字输入框"""
        pass

    @abstractmethod
    def create_group_box(self, title: str) -> Any:
        """创建分组框"""
        pass

    @abstractmethod
    def create_tab_widget(self) -> Any:
        """创建选项卡"""
        pass

    @abstractmethod
    def create_splitter(self, horizontal: bool = True) -> Any:
        """创建分割器"""
        pass

    @abstractmethod
    def add_widget_to_layout(self, parent: Any, widget: Any, expand: bool = False) -> None:
        """添加组件到布局"""
        pass

    @abstractmethod
    def set_widget_text(self, widget: Any, text: str) -> None:
        """设置组件文本"""
        pass

    @abstractmethod
    def get_widget_text(self, widget: Any) -> str:
        """获取组件文本"""
        pass

    @abstractmethod
    def set_progress_value(self, widget: Any, value: int) -> None:
        """设置进度条值"""
        pass

    @abstractmethod
    def set_checkbox_state(self, widget: Any, checked: bool) -> None:
        """设置复选框状态"""
        pass

    @abstractmethod
    def get_checkbox_state(self, widget: Any) -> bool:
        """获取复选框状态"""
        pass

    @abstractmethod
    def get_combo_box_value(self, widget: Any) -> str:
        """获取下拉框当前值"""
        pass

    @abstractmethod
    def set_combo_box_value(self, widget: Any, value: str) -> None:
        """设置下拉框当前值"""
        pass

    @abstractmethod
    def append_text(self, widget: Any, text: str) -> None:
        """追加文本到文本区域"""
        pass

    @abstractmethod
    def clear_text(self, widget: Any) -> None:
        """清空文本区域"""
        pass

    @abstractmethod
    def show_message(self, title: str, message: str, msg_type: str = "info") -> None:
        """显示消息对话框"""
        pass

    @abstractmethod
    def show_file_dialog(self, title: str, mode: str = "open", file_filter: str = "") -> Optional[str]:
        """显示文件对话框"""
        pass

    @abstractmethod
    def run(self) -> int:
        """运行主循环"""
        pass

    @abstractmethod
    def exit(self) -> None:
        """退出应用"""
        pass


class HICBuildGUIBase(ABC):
    """HIC 构建系统 GUI 抽象基类"""

    def __init__(self, backend: GUIBackend):
        self.backend = backend
        self.backend_type = backend.__class__.__name__.replace("Backend", "").lower() if backend else "unknown"
        self.current_language = "zh_CN"
        self.current_theme = "dark"
        self.current_preset = "balanced"
        self.is_building = False
        self.build_thread = None
        self.widgets = {}
        self.translations = I18N.get("zh_CN", I18N.get("en_US", {}))

        # 初始化界面
        self._init_ui()

    def t(self, key: str) -> str:
        """翻译函数"""
        return self.translations.get(key, key)

    @abstractmethod
    def _init_ui(self) -> None:
        """初始化用户界面"""
        pass

    @abstractmethod
    def _create_menu_bar(self) -> Any:
        """创建菜单栏"""
        pass

    @abstractmethod
    def _create_toolbar(self) -> Any:
        """创建工具栏"""
        pass

    @abstractmethod
    def _create_main_area(self) -> Any:
        """创建主区域"""
        pass

    @abstractmethod
    def _create_status_bar(self) -> Any:
        """创建状态栏"""
        pass

    def _load_config(self) -> Dict[str, Any]:
        """加载配置"""
        config = {}
        if PLATFORM_YAML.exists():
            try:
                import yaml
                with open(PLATFORM_YAML, 'r', encoding='utf-8') as f:
                    data = yaml.safe_load(f)
                    config = data.get('build', {})
            except Exception as e:
                print(f"加载配置失败: {e}")
        return config

    def _save_config(self, config: Dict[str, Any]) -> bool:
        """保存配置"""
        try:
            import yaml
            if PLATFORM_YAML.exists():
                with open(PLATFORM_YAML, 'r', encoding='utf-8') as f:
                    data = yaml.safe_load(f)
            else:
                data = {}
            
            data['build'] = config
            
            with open(PLATFORM_YAML, 'w', encoding='utf-8') as f:
                yaml.dump(data, f, allow_unicode=True, default_flow_style=False)
            return True
        except Exception as e:
            print(f"保存配置失败: {e}")
            return False

    def _apply_preset(self, preset: str) -> None:
        """应用预设配置"""
        presets = {
            'balanced': {
                'optimize_level': 2,
                'debug_symbols': True,
                'lto': False,
                'strip': False
            },
            'release': {
                'optimize_level': 3,
                'debug_symbols': False,
                'lto': True,
                'strip': True
            },
            'debug': {
                'optimize_level': 0,
                'debug_symbols': True,
                'lto': False,
                'strip': False
            },
            'minimal': {
                'optimize_level': 2,
                'debug_symbols': False,
                'lto': False,
                'strip': True
            },
            'performance': {
                'optimize_level': 3,
                'debug_symbols': False,
                'lto': True,
                'strip': True
            }
        }

        if preset in presets:
            self.current_preset = preset
            config = self._load_config()
            config.update(presets[preset])
            self._save_config(config)

    def _start_build(self, targets: List[str] = None) -> None:
        """开始构建"""
        if self.is_building:
            self.backend.show_message("警告", "构建正在进行中")
            return

        self.is_building = True

        def build_thread():
            try:
                if targets is None:
                    targets = ["bootloader", "kernel"]

                for target in targets:
                    self._append_output(f"\n正在构建 {target}...\n")
                    result = subprocess.run(
                        ["make", target],
                        cwd=str(ROOT_DIR),
                        capture_output=True,
                        text=True
                    )

                    if result.stdout:
                        self._append_output(result.stdout)
                    if result.stderr:
                        self._append_output(result.stderr)

                    if result.returncode != 0:
                        self._append_output(f"\n错误: {target} 构建失败\n")
                        break
                    else:
                        self._append_output(f"\n成功: {target} 构建完成\n")

                self._append_output("\n构建完成！\n")
            except Exception as e:
                self._append_output(f"\n构建错误: {e}\n")
            finally:
                self.is_building = False
                self._set_progress(100)

        self.build_thread = threading.Thread(target=build_thread)
        self.build_thread.daemon = True
        self.build_thread.start()

    def _append_output(self, text: str) -> None:
        """追加输出"""
        if 'output' in self.widgets:
            self.backend.append_text(self.widgets['output'], text)

    def _set_progress(self, value: int) -> None:
        """设置进度"""
        if 'progress' in self.widgets:
            self.backend.set_progress_value(self.widgets['progress'], value)

    def _clear_output(self) -> None:
        """清空输出"""
        if 'output' in self.widgets:
            self.backend.clear_text(self.widgets['output'])

    def _change_language(self, language: str) -> None:
        """更改语言"""
        self.current_language = language
        if language in I18N:
            self.translations = I18N[language]
            # 重新加载界面（需要在子类中实现）
            self._reload_ui()

    def _change_theme(self, theme: str) -> None:
        """更改主题"""
        self.current_theme = theme
        # 应用主题（需要在子类中实现）
        self._apply_theme(theme)

    @abstractmethod
    def _reload_ui(self) -> None:
        """重新加载界面"""
        pass

    @abstractmethod
    def _apply_theme(self, theme: str) -> None:
        """应用主题"""
        pass


def detect_available_backends() -> List[str]:
    """检测可用的 GUI 后端"""
    backends = []

    # 检测 Qt
    try:
        from PyQt6.QtWidgets import QApplication
        backends.append("qt")
    except ImportError:
        pass

    # 检测 GTK
    try:
        import gi
        gi.require_version('Gtk', '3.0')
        from gi.repository import Gtk
        backends.append("gtk")
    except ImportError:
        pass

    # 检测 Tkinter（内置）
    try:
        import tkinter
        backends.append("tk")
    except ImportError:
        pass

    # 检测 Web
    try:
        from flask import Flask
        from flask_socketio import SocketIO
        backends.append("web")
    except ImportError:
        pass

    # 检测 Kivy
    try:
        import kivy
        backends.append("kivy")
    except ImportError:
        pass

    # 检测 Dear PyGui
    try:
        import dearpygui
        backends.append("dearpygui")
    except ImportError:
        pass

    return backends


def create_gui_backend(backend_type: str) -> Optional[GUIBackend]:
    """创建 GUI 后端"""
    if backend_type == "qt":
        from gui_qt_backend import QtBackend
        return QtBackend()
    elif backend_type == "gtk":
        from gui_gtk_backend import GtkBackend
        return GtkBackend()
    elif backend_type == "tk":
        from gui_tk_backend import TkBackend
        return TkBackend()
    elif backend_type == "web":
        from gui_web_backend import WebBackend
        return WebBackend()
    elif backend_type == "kivy":
        from gui_kivy_backend import KivyBackend
        return KivyBackend()
    elif backend_type == "dearpygui":
        from gui_dearpygui_backend import DearPyGuiBackend
        return DearPyGuiBackend()
    else:
        return None
