#!/usr/bin/env python3
"""
HIC 串口监控 GUI
读取 QEMU 串口输出并显示在图形界面中
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import threading
import queue


class SerialMonitorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("HIC 串口监控器")
        self.root.geometry("800x600")
        
        # 配置队列用于线程间通信
        self.data_queue = queue.Queue()
        
        # 串口连接状态
        self.serial_connected = False
        self.serial_port = None
        
        # 创建界面
        self.create_widgets()
        
        # 启动数据接收线程
        self.running = True
        self.receive_thread = threading.Thread(target=self.receive_data, daemon=True)
        self.receive_thread.start()
        
        # 启动界面更新
        self.update_display()
    
    def create_widgets(self):
        """创建 GUI 组件"""
        # 顶部工具栏
        toolbar = ttk.Frame(self.root)
        toolbar.pack(fill=tk.X, padx=5, pady=5)
        
        # 串口选择
        ttk.Label(toolbar, text="串口:").pack(side=tk.LEFT, padx=5)
        self.port_combo = ttk.Combobox(toolbar, values=["/dev/ttyS0", "/dev/ttyS1", "/dev/ttyUSB0"])
        self.port_combo.set("/dev/ttyS0")
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        # 波特率选择
        ttk.Label(toolbar, text="波特率:").pack(side=tk.LEFT, padx=5)
        self.baud_combo = ttk.Combobox(toolbar, values=["115200", "57600", "38400", "19200", "9600"])
        self.baud_combo.set("115200")
        self.baud_combo.pack(side=tk.LEFT, padx=5)
        
        # 连接按钮
        self.connect_btn = ttk.Button(toolbar, text="连接", command=self.toggle_connection)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        # 清空按钮
        ttk.Button(toolbar, text="清空", command=self.clear_output).pack(side=tk.LEFT, padx=5)
        
        # 命令输入区
        input_frame = ttk.Frame(self.root)
        input_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(input_frame, text="命令:").pack(side=tk.LEFT, padx=5)
        self.cmd_entry = ttk.Entry(input_frame)
        self.cmd_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.cmd_entry.bind("<Return>", self.send_command)
        
        ttk.Button(input_frame, text="发送", command=self.send_command).pack(side=tk.LEFT, padx=5)
        
        # 输出显示区
        output_frame = ttk.Frame(self.root)
        output_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.output_text = scrolledtext.ScrolledText(
            output_frame,
            wrap=tk.WORD,
            font=("Courier", 10)
        )
        self.output_text.pack(fill=tk.BOTH, expand=True)
        
        # 配置颜色标签
        self.output_text.tag_config("info", foreground="blue")
        self.output_text.tag_config("error", foreground="red")
        self.output_text.tag_config("success", foreground="green")
        
        # 状态栏
        self.status_var = tk.StringVar()
        self.status_var.set("就绪")
        status_bar = ttk.Label(self.root, textvariable=self.status_var, relief=tk.SUNKEN)
        status_bar.pack(fill=tk.X, side=tk.BOTTOM)
    
    def toggle_connection(self):
        """切换串口连接状态"""
        if not self.serial_connected:
            self.connect_serial()
        else:
            self.disconnect_serial()
    
    def connect_serial(self):
        """连接串口"""
        port = self.port_combo.get()
        baudrate = int(self.baud_combo.get())
        
        try:
            self.serial_port = serial.Serial(
                port=port,
                baudrate=baudrate,
                timeout=0.1
            )
            self.serial_connected = True
            self.connect_btn.config(text="断开")
            self.status_var.set(f"已连接到 {port}")
            self.append_output(f"[INFO] 已连接到 {port} @ {baudrate}\n", "info")
        except serial.SerialException as e:
            self.append_output(f"[ERROR] 无法连接到 {port}: {e}\n", "error")
            self.status_var.set("连接失败")
    
    def disconnect_serial(self):
        """断开串口连接"""
        if self.serial_port:
            self.serial_port.close()
            self.serial_port = None
            self.serial_connected = False
            self.connect_btn.config(text="连接")
            self.status_var.set("已断开")
            self.append_output("[INFO] 已断开连接\n", "info")
    
    def receive_data(self):
        """接收串口数据线程"""
        while self.running:
            if self.serial_connected and self.serial_port:
                try:
                    data = self.serial_port.read(1024)
                    if data:
                        self.data_queue.put(data)
                except serial.SerialException:
                    self.data_queue.put(b"[ERROR] 串口读取错误\n")
            else:
                # 如果未连接，模拟一些数据用于测试
                import time
                time.sleep(1)
    
    def update_display(self):
        """更新显示（主线程）"""
        try:
            while not self.data_queue.empty():
                data = self.data_queue.get_nowait()
                text = data.decode('utf-8', errors='ignore')
                self.append_output(text)
        except queue.Empty:
            pass
        
        # 继续更新
        self.root.after(100, self.update_display)
    
    def append_output(self, text, tag=None):
        """添加输出文本"""
        self.output_text.insert(tk.END, text, tag)
        self.output_text.see(tk.END)
    
    def clear_output(self):
        """清空输出"""
        self.output_text.delete(1.0, tk.END)
    
    def send_command(self, event=None):
        """发送命令"""
        cmd = self.cmd_entry.get()
        if not cmd:
            return
        
        self.append_output(f"> {cmd}\n", "info")
        
        if self.serial_connected and self.serial_port:
            try:
                self.serial_port.write(cmd.encode('utf-8') + b'\n')
            except serial.SerialException:
                self.append_output("[ERROR] 发送命令失败\n", "error")
        
        self.cmd_entry.delete(0, tk.END)
    
    def on_closing(self):
        """关闭窗口"""
        self.running = False
        if self.receive_thread.is_alive():
            self.receive_thread.join(timeout=1)
        self.disconnect_serial()
        self.root.destroy()


def main():
    """主函数"""
    root = tk.Tk()
    app = SerialMonitorGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()