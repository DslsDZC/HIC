#!/usr/bin/python3
"""
HIC系统构建系统 - 文本GUI模式
使用curses库实现文本用户界面
遵循TD/滚动更新.md文档
"""

import subprocess
import os
import sys
from typing import List, Callable, Optional

# Set TERM and TERMINFO environment variables before importing curses
if not os.environ.get('TERM'):
    os.environ['TERM'] = 'xterm-256color'
if not os.environ.get('TERMINFO'):
    os.environ['TERMINFO'] = '/usr/share/terminfo'

import curses

# 颜色对
COLOR_PAIR_DEFAULT = 1
COLOR_PAIR_TITLE = 2
COLOR_PAIR_MENU = 3
COLOR_PAIR_SELECTED = 4
COLOR_PAIR_SUCCESS = 5
COLOR_PAIR_ERROR = 6
COLOR_PAIR_WARNING = 7

# 项目信息
PROJECT = "HIC System"
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
            MenuItem("配置构建选项", self.config_build, "配置编译时参数"),
            MenuItem("配置运行时选项", self.config_runtime, "配置运行时参数"),
            MenuItem("---", self.no_action, ""),
            MenuItem("命令行模式构建", self.build_console, "使用命令行模式编译整个系统"),
            MenuItem("文本GUI模式构建", self.build_tui, "使用文本GUI模式编译系统"),
            MenuItem("图形化GUI模式构建", self.build_gui, "使用图形化GUI模式编译系统"),
            MenuItem("清理构建文件", self.clean_build, "清理所有构建生成的文件"),
            MenuItem("安装依赖 (Arch Linux)", self.install_deps, "安装编译所需的依赖包"),
            MenuItem("---", self.no_action, ""),
            MenuItem("显示帮助", self.show_help, "显示构建系统帮助信息"),
            MenuItem("退出", self.exit_program, "退出构建系统"),
        ]
    
    def no_action(self):
        """无操作（分隔符）"""
        pass
    
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
        
        # 检查ncurses支持（TUI界面本身需要curses，所以如果运行到这里说明curses可用）
        # 但为了构建其他可能需要ncurses的组件，我们还是检查一下
        try:
            import subprocess
            result = subprocess.run(["pkg-config", "--exists", "ncursesw"], 
                                  capture_output=True, timeout=5)
            if result.returncode != 0:
                self.log("警告: ncurses开发库可能未安装", "warning")
        except Exception:
            self.log("警告: 无法检查ncurses开发库", "warning")
        
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
    
    def config_build(self):
        """配置编译时选项"""
        self.log("=== 编译时配置 ===", "info")
        self.log("正在加载编译配置...", "info")
        
        config_file = os.path.join(ROOT_DIR, "..", "build_config.mk")
        
        if not os.path.exists(config_file):
            self.log("错误: 配置文件不存在", "error")
            return
        
        # 读取配置
        config = {}
        with open(config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith('CONFIG_') and '=' in line:
                    key, value = line.split('=', 1)
                    config[key.strip()] = value.strip().rstrip('?')
        
        # 显示关键配置项
        self.log("\n当前编译配置:", "info")
        
        # 调试配置
        self.log(f"  [调试] 调试支持: {'启用' if config.get('CONFIG_DEBUG') == '1' else '禁用'}", "info")
        self.log(f"  [调试] 跟踪功能: {'启用' if config.get('CONFIG_TRACE') == '1' else '禁用'}", "info")
        self.log(f"  [调试] 详细输出: {'启用' if config.get('CONFIG_VERBOSE') == '1' else '禁用'}", "info")
        
        # 安全配置
        self.log(f"  [安全] KASLR: {'启用' if config.get('CONFIG_KASLR') == '1' else '禁用'}", "info")
        self.log(f"  [安全] SMEP: {'启用' if config.get('CONFIG_SMEP') == '1' else '禁用'}", "info")
        self.log(f"  [安全] SMAP: {'启用' if config.get('CONFIG_SMAP') == '1' else '禁用'}", "info")
        self.log(f"  [安全] 审计日志: {'启用' if config.get('CONFIG_AUDIT') == '1' else '禁用'}", "info")
        
        # 性能配置
        self.log(f"  [性能] 性能计数器: {'启用' if config.get('CONFIG_PERF') == '1' else '禁用'}", "info")
        self.log(f"  [性能] 快速路径: {'启用' if config.get('CONFIG_FAST_PATH') == '1' else '禁用'}", "info")
        
        # 内存配置
        self.log(f"  [内存] 堆大小: {config.get('CONFIG_HEAP_SIZE_MB', '128')} MB", "info")
        self.log(f"  [内存] 栈大小: {config.get('CONFIG_STACK_SIZE_KB', '8')} KB", "info")
        self.log(f"  [内存] 页面缓存: {config.get('CONFIG_PAGE_CACHE_PERCENT', '20')}%", "info")
        
        # 调度器配置
        self.log(f"  [调度] 调度策略: {config.get('CONFIG_SCHEDULER_POLICY', 'priority')}", "info")
        self.log(f"  [调度] 时间片: {config.get('CONFIG_TIME_SLICE_MS', '10')} ms", "info")
        self.log(f"  [调度] 最大线程: {config.get('CONFIG_MAX_THREADS', '256')}", "info")
        
        # 功能配置
        self.log(f"\n  [功能] PCI支持: {'启用' if config.get('CONFIG_PCI') == '1' else '禁用'}", "info")
        self.log(f"  [功能] ACPI支持: {'启用' if config.get('CONFIG_ACPI') == '1' else '禁用'}", "info")
        self.log(f"  [功能] 串口支持: {'启用' if config.get('CONFIG_SERIAL') == '1' else '禁用'}", "info")
        
        # 能力系统配置
        self.log(f"\n  [能力系统] 最大能力数: {config.get('CONFIG_MAX_CAPABILITIES', '65536')}", "info")
        self.log(f"  [能力系统] 能力派生: {'启用' if config.get('CONFIG_CAPABILITY_DERIVATION') == '1' else '禁用'}", "info")
        
        # 域配置
        self.log(f"\n  [域] 最大域数: {config.get('CONFIG_MAX_DOMAINS', '16')}", "info")
        self.log(f"  [域] 域栈大小: {config.get('CONFIG_DOMAIN_STACK_SIZE_KB', '16')} KB", "info")
        
        # 中断配置
        self.log(f"\n  [中断] 最大中断数: {config.get('CONFIG_MAX_IRQS', '256')}", "info")
        self.log(f"  [中断] 中断公平性: {'启用' if config.get('CONFIG_IRQ_FAIRNESS') == '1' else '禁用'}", "info")
        
        # 模块配置
        self.log(f"\n  [模块] 模块加载: {'启用' if config.get('CONFIG_MODULE_LOADING') == '1' else '禁用'}", "info")
        self.log(f"  [模块] 最大模块数: {config.get('CONFIG_MAX_MODULES', '32')}", "info")
        
        self.log("\n提示: 编辑 build_config.mk 文件来修改这些配置", "warning")
        self.log("修改后需要重新编译才能生效", "warning")
        self.status_message = "配置已显示"
    
    def config_runtime(self):
        """配置运行时选项"""
        self.log("=== 运行时配置 ===", "info")
        
        runtime_config = os.path.join(ROOT_DIR, "..", "runtime_config.yaml.example")
        
        if not os.path.exists(runtime_config):
            self.log("错误: 运行时配置示例文件不存在", "error")
            return
        
        self.log("运行时配置示例位置: runtime_config.yaml.example", "info")
        self.log("\n主要运行时配置项:", "info")
        self.log("  [系统] 日志级别: error, warn, info, debug, trace", "info")
        self.log("  [系统] 调度策略: fifo, rr, priority", "info")
        self.log("  [系统] 内存策略: firstfit, bestfit, buddy", "info")
        self.log("  [系统] 安全级别: minimal, standard, strict", "info")
        self.log("\n  [调度] 时间片: 毫秒单位", "info")
        self.log("  [调度] 最大线程数", "info")
        self.log("\n  [内存] 堆大小 (MB)", "info")
        self.log("  [内存] 栈大小 (KB)", "info")
        self.log("  [内存] 页面缓存百分比", "info")
        self.log("\n  [安全] 安全启动, KASLR, SMEP, SMAP, 审计", "info")
        self.log("\n  [性能] 性能计数器, 快速路径", "info")
        self.log("\n提示: 复制 runtime_config.yaml.example 为 runtime_config.yaml", "warning")
        self.log("修改后无需重新编译，通过引导层传递给内核", "warning")
        self.status_message = "配置已显示"
    
    def show_help(self):
        """显示帮助"""
        self.log("=== HIC系统构建系统帮助 ===")
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
    # Check for --help argument before initializing curses
    if len(sys.argv) > 1 and '--help' in sys.argv or '-h' in sys.argv:
        print(f"{PROJECT} 构建系统 v{VERSION} - 文本GUI模式")
        print("\n使用说明:")
        print("  ↑↓      选择菜单项")
        print("  Enter   执行选中的操作")
        print("  q       退出程序")
        print("\n构建选项:")
        print("  配置构建选项        - 配置编译时参数")
        print("  配置运行时选项      - 配置运行时参数")
        print("  命令行模式构建      - 使用命令行模式编译整个系统")
        print("  文本GUI模式构建     - 使用文本GUI模式编译系统")
        print("  图形化GUI模式构建   - 使用图形化GUI模式编译系统")
        print("  清理构建文件        - 清理所有构建生成的文件")
        print("  安装依赖            - 安装编译所需的依赖包 (Arch Linux)")
        print("  显示帮助            - 显示构建系统帮助信息")
        print("  退出                - 退出构建系统")
        sys.exit(0)
    
    try:
        # Set TERM environment variable if not set
        if not os.environ.get('TERM'):
            os.environ['TERM'] = 'xterm-256color'
        curses.wrapper(main)
    except KeyboardInterrupt:
        print("\n程序已中断")
        sys.exit(0)
    except Exception as e:
        print(f"错误: {str(e)}")
        sys.exit(1)
