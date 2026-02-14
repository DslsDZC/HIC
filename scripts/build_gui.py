#!/usr/bin/env python3
"""
HIK系统构建系统 - 图形化GUI模式
使用GTK3库实现图形用户界面
遵循TD/滚动更新.md文档
"""

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib, Pango
import subprocess
import os
import sys
import threading
from typing import Optional, List

# 项目信息
PROJECT = "HIK System"
VERSION = "0.1.0"
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT_DIR, "build")
OUTPUT_DIR = os.environ.get("OUTPUT_DIR", os.path.join(ROOT_DIR, "output"))


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
        label_widget = Gtk.Label(label)
        
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
        # 检查ncurses
        if not os.path.exists("/usr/bin/ncurses6-config") and not os.path.exists("/usr/bin/ncursesw6-config"):
            self.log("错误: 需要安装 ncurses", "error")
            self.update_status("缺少依赖", "error")
            return
        
        self.run_build_task("文本GUI模式构建", ["make", "clean"], ["make", "BUILD_TYPE=tui"])
    
    def build_gui(self):
        """图形化GUI模式构建"""
        # 检查gtk3
        if not os.path.exists("/usr/bin/gtk3"):
            self.log("错误: 需要安装 gtk3", "error")
            self.update_status("缺少依赖", "error")
            return
        
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
    
    def show_help(self):
        """显示帮助"""
        help_dialog = Gtk.MessageDialog(
            transient_for=self,
            flags=0,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="HIK系统构建系统帮助"
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