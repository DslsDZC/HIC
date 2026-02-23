#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统统一 GUI 入口
自动检测并选择最佳 GUI 后端
支持 Qt, GTK, Tkinter, Kivy, Dear PyGui
"""

import sys
from pathlib import Path
from typing import Optional

# 修复相对导入问题
import gui_base
from gui_base import detect_available_backends, create_gui_backend, HICBuildGUIBase

# 项目信息
PROJECT = "HIC"
VERSION = "0.1.0"


class UnifiedHICBuildGUI(HICBuildGUIBase):
    """统一的 HIC 构建系统 GUI"""

    def __init__(self, backend_type: str = None):
        if backend_type is None:
            backends = detect_available_backends()
            if not backends:
                print("错误: 没有可用的 GUI 后端")
                sys.exit(1)
            backend_type = backends[0]
            print(f"使用 {backend_type.upper()} 后端")

        backend = create_gui_backend(backend_type)
        if backend is None:
            print(f"错误: 无法创建 {backend_type} 后端")
            sys.exit(1)

        super().__init__(backend)
        self.backend_type = backend_type

    def _init_ui(self) -> None:
        self.window = self.backend.create_window(f"{PROJECT} v{VERSION}", 1200, 800)

        main_widget = self._create_main_widget()
        if main_widget:
            if self.backend_type == "qt":
                self.window.setCentralWidget(main_widget)
            elif self.backend_type in ["gtk", "tk"]:
                # GTK 和 Tkinter 在回调中处理
                pass

    def _create_main_widget(self) -> Optional[object]:
        """创建主组件"""
        if self.backend_type == "qt":
            from PyQt6.QtWidgets import QWidget, QVBoxLayout
            widget = QWidget()
            layout = QVBoxLayout()
            widget.setLayout(layout)
            return widget
        elif self.backend_type == "gtk":
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
            self.window.add(box)
            return box
        elif self.backend_type == "tk":
            frame = ttk.Frame(self.window.root)
            frame.pack(fill=tk.BOTH, expand=True)
            return frame
        return None

    def _create_menu_bar(self) -> object:
        """创建菜单栏"""
        if self.backend_type == "qt":
            from PyQt6.QtWidgets import QMenuBar, QMenu
            menubar = self.window.menuBar()
            
            file_menu = menubar.addMenu("文件")
            edit_menu = menubar.addMenu("编辑")
            view_menu = menubar.addMenu("视图")
            help_menu = menubar.addMenu("帮助")
            
            return menubar
        elif self.backend_type == "gtk":
            from gi.repository import Gtk
            menubar = Gtk.MenuBar()
            self.window.add(menubar)
            return menubar
        elif self.backend_type == "tk":
            menubar = tk.Menu(self.window.root)
            self.window.root.config(menu=menubar)
            return menubar
        return None

    def _create_toolbar(self) -> object:
        """创建工具栏"""
        if self.backend_type == "qt":
            from PyQt6.QtWidgets import QToolBar
            toolbar = QToolBar("工具栏")
            self.window.addToolBar(toolbar)
            return toolbar
        return None

    def _create_main_area(self) -> object:
        """创建主区域"""
        if self.backend_type == "qt":
            from PyQt6.QtWidgets import QSplitter
            splitter = QSplitter()
            return splitter
        elif self.backend_type == "gtk":
            from gi.repository import Gtk
            paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
            return paned
        elif self.backend_type == "tk":
            from tkinter import ttk
            paned = ttk.PanedWindow(orient=tk.HORIZONTAL)
            return paned
        return None

    def _create_status_bar(self) -> object:
        """创建状态栏"""
        if self.backend_type == "qt":
            from PyQt6.QtWidgets import QStatusBar
            statusbar = QStatusBar()
            self.window.setStatusBar(statusbar)
            return statusbar
        elif self.backend_type == "gtk":
            from gi.repository import Gtk
            statusbar = Gtk.Statusbar()
            self.window.add(statusbar)
            return statusbar
        elif self.backend_type == "tk":
            from tkinter import ttk
            statusbar = ttk.Label(self.window.root, text="就绪", relief=tk.SUNKEN, anchor=tk.W)
            statusbar.pack(side=tk.BOTTOM, fill=tk.X)
            return statusbar
        return None

    def _reload_ui(self) -> None:
        """重新加载界面"""
        pass

    def _apply_theme(self, theme: str) -> None:
        """应用主题"""
        if self.backend_type == "qt":
            from PyQt6.QtGui import QPalette, QColor
            app = self.window.windowHandle().window().findChild(type(self.window)).parent()
            if theme == "dark":
                palette = QPalette()
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
                app.setPalette(palette)
        elif self.backend_type == "gtk":
            from gi.repository import Gtk
            settings = Gtk.Settings.get_default()
            settings.set_property("gtk-application-prefer-dark-theme", theme == "dark")


def main():
    """主函数"""
    import argparse

    parser = argparse.ArgumentParser(description="HIC 构建系统 GUI")
    parser.add_argument(
        "--backend",
        choices=["qt", "gtk", "tk", "web", "kivy", "dearpygui", "auto"],
        default="auto",
        help="选择 GUI 后端 (默认: auto)"
    )

    args = parser.parse_args()

    backend_type = None if args.backend == "auto" else args.backend

    try:
        gui = UnifiedHICBuildGUI(backend_type)
        return gui.backend.run()
    except KeyboardInterrupt:
        print("\n退出")
        return 0
    except Exception as e:
        print(f"错误: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())