#!/usr/bin/env python3
"""
HIK系统构建系统 - 文本GUI模式
使用curses库实现文本用户界面
遵循TD/滚动更新.md文档
"""

import curses
import subprocess
import os
import sys
from typing import List, Callable, Optional

# 颜色对
COLOR_PAIR_DEFAULT = 1
COLOR_PAIR_TITLE = 2
COLOR_PAIR_MENU = 3
COLOR_PAIR_SELECTED = 4
COLOR_PAIR_SUCCESS = 5
COLOR_PAIR_ERROR = 6
COLOR_PAIR_WARNING = 7

# 项目信息
PROJECT = "HIK System"
VERSION = "0.1.0"
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT_DIR, "build")
OUTPUT_DIR = os.environ.get("OUTPUT_DIR", os.path.join(ROOT_DIR, "output"))


class MenuItem:
    """菜单项"""
    def __init__(self, name: str, action: Callable, description: str = ""):
        self.name = name
        self.action = action
        self.description = description


class BuildTUI:
    """文本GUI构建系统"""
    
    def __init__(self, stdscr):
        self.stdscr = stdscr
        self.current_menu: List[MenuItem] = []
        self.selected_index = 0
        self.log_messages: List[str] = []
        self.status_message = ""
        
        # 初始化颜色
        self.init_colors()
        
        # 创建主菜单
        self.create_main_menu()
    
    def init_colors(self):
        """初始化颜色"""
        curses.start_color()
        curses.use_default_colors()
        
        # 初始化颜色对
        curses.init_pair(COLOR_PAIR_DEFAULT, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(COLOR_PAIR_TITLE, curses.COLOR_MAGENTA, curses.COLOR_BLACK)
        curses.init_pair(COLOR_PAIR_MENU, curses.COLOR_CYAN, curses.COLOR_BLACK)
        curses.init_pair(COLOR_PAIR_SELECTED, curses.COLOR_BLACK, curses.COLOR_CYAN)
        curses.init_pair(COLOR_PAIR_SUCCESS, curses.COLOR_GREEN, curses.COLOR_BLACK)
        curses.init_pair(COLOR_PAIR_ERROR, curses.COLOR_RED, curses.COLOR_BLACK)
        curses.init_pair(COLOR_PAIR_WARNING, curses.COLOR_YELLOW, curses.COLOR_BLACK)
    
    def create_main_menu(self):
        """创建主菜单"""
        self.current_menu = [
            MenuItem("命令行模式构建", self.build_console, "使用命令行模式编译整个系统"),
            MenuItem("文本GUI模式构建", self.build_tui, "使用文本GUI模式编译系统"),
            MenuItem("图形化GUI模式构建", self.build_gui, "使用图形化GUI模式编译系统"),
            MenuItem("清理构建文件", self.clean_build, "清理所有构建生成的文件"),
            MenuItem("安装依赖 (Arch Linux)", self.install_deps, "安装编译所需的依赖包"),
            MenuItem("显示帮助", self.show_help, "显示构建系统帮助信息"),
            MenuItem("退出", self.exit_program, "退出构建系统"),
        ]
    
    def log(self, message: str, level: str = "info"):
        """添加日志消息"""
        self.log_messages.append(f"[{level.upper()}] {message}")
        # 限制日志数量
        if len(self.log_messages) > 100:
            self.log_messages = self.log_messages[-100:]
    
    def run_command(self, command: List[str], cwd: Optional[str] = None) -> bool:
        """运行命令并返回成功状态"""
        try:
            self.log(f"执行命令: {' '.join(command)}")
            process = subprocess.Popen(
                command,
                cwd=cwd or ROOT_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True
            )
            
            # 实时显示输出
            for line in process.stdout:
                self.log(line.strip(), "info")
            
            # 等待命令完成
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
        self.log("开始命令行模式构建...")
        self.status_message = "正在构建..."
        
        # 清理
        if not self.run_command(["make", "clean"]):
            self.status_message = "清理失败"
            return
        
        # 构建
        if self.run_command(["make", "BUILD_TYPE=console"]):
            self.status_message = "构建成功!"
            self.log(f"输出目录: {OUTPUT_DIR}", "success")
        else:
            self.status_message = "构建失败"
    
    def build_tui(self):
        """文本GUI模式构建"""
        self.log("开始文本GUI模式构建...")
        self.status_message = "正在构建..."
        
        # 检查ncurses
        if not self.run_command(["which", "ncurses6-config"]):
            self.log("错误: 需要安装 ncurses", "error")
            self.status_message = "缺少依赖"
            return
        
        # 清理
        if not self.run_command(["make", "clean"]):
            self.status_message = "清理失败"
            return
        
        # 构建
        if self.run_command(["make", "BUILD_TYPE=tui"]):
            self.status_message = "构建成功!"
        else:
            self.status_message = "构建失败"
    
    def build_gui(self):
        """图形化GUI模式构建"""
        self.log("开始图形化GUI模式构建...")
        self.status_message = "正在构建..."
        
        # 检查gtk3
        if not self.run_command(["which", "gtk3"]):
            self.log("错误: 需要安装 gtk3", "error")
            self.status_message = "缺少依赖"
            return
        
        # 清理
        if not self.run_command(["make", "clean"]):
            self.status_message = "清理失败"
            return
        
        # 构建
        if self.run_command(["make", "BUILD_TYPE=gui"]):
            self.status_message = "构建成功!"
        else:
            self.status_message = "构建失败"
    
    def clean_build(self):
        """清理构建"""
        self.log("清理构建文件...")
        self.status_message = "正在清理..."
        
        if self.run_command(["make", "clean"]):
            self.status_message = "清理完成!"
        else:
            self.status_message = "清理失败"
    
    def install_deps(self):
        """安装依赖"""
        self.log("安装依赖 (Arch Linux)...")
        self.status_message = "正在安装..."
        
        # 检查是否为Arch Linux
        if not os.path.exists("/etc/arch-release"):
            self.log("错误: 此脚本仅适用于 Arch Linux", "error")
            self.status_message = "不支持的系统"
            return
        
        if self.run_command(["sudo", "pacman", "-S", "--needed", "base-devel", "git", 
                           "mingw-w64-gcc", "gnu-efi", "ncurses", "gtk3"]):
            self.status_message = "安装成功!"
            self.log("注意: 内核交叉编译工具链需要手动安装", "warning")
        else:
            self.status_message = "安装失败"
    
    def show_help(self):
        """显示帮助"""
        self.log("=== HIK系统构建系统帮助 ===")
        self.log("命令行模式: make 或 make console")
        self.log("文本GUI模式: make tui 或 ./build_tui.py")
        self.log("图形化GUI模式: make gui")
        self.log("清理: make clean")
        self.log("安装依赖: make deps-arch")
        self.log("帮助: make help")
        self.status_message = "帮助已显示"
    
    def exit_program(self):
        """退出程序"""
        self.log("退出构建系统...")
        sys.exit(0)
    
    def draw(self):
        """绘制界面"""
        self.stdscr.clear()
        height, width = self.stdscr.getmaxyx()
        
        # 绘制标题
        title = f"=== {PROJECT} 构建系统 v{VERSION} ==="
        title_pos = (width - len(title)) // 2
        self.stdscr.addstr(2, title_pos, title, curses.color_pair(COLOR_PAIR_TITLE))
        
        # 绘制菜单
        menu_start_y = 6
        for i, item in enumerate(self.current_menu):
            if i == self.selected_index:
                # 选中的菜单项
                menu_text = f"> {item.name}"
                self.stdscr.addstr(menu_start_y + i, 4, menu_text, curses.color_pair(COLOR_PAIR_SELECTED))
                # 显示描述
                if item.description:
                    desc_text = f"  {item.description}"
                    self.stdscr.addstr(menu_start_y + i, 4 + len(menu_text), desc_text, curses.color_pair(COLOR_PAIR_WARNING))
            else:
                # 普通菜单项
                menu_text = f"  {item.name}"
                self.stdscr.addstr(menu_start_y + i, 4, menu_text, curses.color_pair(COLOR_PAIR_MENU))
        
        # 绘制状态
        if self.status_message:
            status_y = menu_start_y + len(self.current_menu) + 2
            self.stdscr.addstr(status_y, 4, f"状态: {self.status_message}", curses.color_pair(COLOR_PAIR_DEFAULT))
        
        # 绘制日志区域
        log_start_y = menu_start_y + len(self.current_menu) + 4
        log_height = height - log_start_y - 2
        
        if log_height > 0:
            # 绘制日志标题
            self.stdscr.addstr(log_start_y, 4, "构建日志:", curses.color_pair(COLOR_PAIR_TITLE))
            
            # 绘制日志内容
            log_lines = self.log_messages[-(log_height - 1):]
            for i, log_line in enumerate(log_lines):
                log_y = log_start_y + 1 + i
                
                # 根据日志级别选择颜色
                if "[ERROR]" in log_line:
                    color = curses.color_pair(COLOR_PAIR_ERROR)
                elif "[SUCCESS]" in log_line:
                    color = curses.color_pair(COLOR_PAIR_SUCCESS)
                elif "[WARNING]" in log_line:
                    color = curses.color_pair(COLOR_PAIR_WARNING)
                else:
                    color = curses.color_pair(COLOR_PAIR_DEFAULT)
                
                # 截断过长的日志
                if len(log_line) > width - 6:
                    log_line = log_line[:width - 9] + "..."
                
                self.stdscr.addstr(log_y, 4, log_line, color)
        
        # 绘制底部提示
        footer = "↑↓ 选择 | Enter 执行 | q 退出"
        footer_pos = (width - len(footer)) // 2
        self.stdscr.addstr(height - 1, footer_pos, footer, curses.color_pair(COLOR_PAIR_MENU))
        
        self.stdscr.refresh()
    
    def run(self):
        """运行TUI"""
        self.stdscr.keypad(True)
        curses.curs_set(0)  # 隐藏光标
        
        while True:
            self.draw()
            
            key = self.stdscr.getch()
            
            if key == curses.KEY_UP:
                self.selected_index = (self.selected_index - 1) % len(self.current_menu)
            elif key == curses.KEY_DOWN:
                self.selected_index = (self.selected_index + 1) % len(self.current_menu)
            elif key == ord('\n') or key == ord('\r'):
                # 执行选中的菜单项
                self.current_menu[self.selected_index].action()
            elif key == ord('q'):
                self.exit_program()


def main(stdscr):
    """主函数"""
    tui = BuildTUI(stdscr)
    tui.run()


if __name__ == "__main__":
    try:
        curses.wrapper(main)
    except KeyboardInterrupt:
        print("\n程序已中断")
        sys.exit(0)
    except Exception as e:
        print(f"错误: {str(e)}")
        sys.exit(1)
