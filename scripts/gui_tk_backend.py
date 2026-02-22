#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统 Tkinter GUI 后端
"""

from typing import Dict, List, Optional, Any, Callable
from .gui_base import GUIBackend
from style_manager import get_style_manager

try:
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox, scrolledtext
except ImportError:
    raise ImportError("Tkinter not available")


class TkBackend(GUIBackend):
    """Tkinter GUI 后端"""

    def __init__(self):
        self.root = None
        self.current_frame = None
        self.style_manager = get_style_manager()

    def create_window(self, title: str, width: int, height: int) -> tk.Tk:
        self.root = tk.Tk()
        self.root.title(title)
        self.root.geometry(f"{width}x{height}")
        
        # 应用 Qt 风格样式
        self._apply_tk_styles()
        
        return self.root

    def _apply_tk_styles(self):
        """应用 Tkinter 样式（模拟 Qt 风格）"""
        style = ttk.Style()
        style.theme_use('clam')
        
        # 获取颜色配置
        c = self.style_manager.qt_style.color_scheme
        
        # 配置样式
        style.configure('TButton',
                       background=c.primary,
                       foreground='white',
                       borderwidth=0,
                       relief='flat')
        style.map('TButton',
                 background=[('active', c.primary_hover),
                           ('pressed', c.primary_pressed)])
        
        style.configure('TLabel',
                       background=c.background,
                       foreground=c.text)
        
        style.configure('TFrame',
                       background=c.background)
        
        style.configure('TNotebook',
                       background=c.background)
        
        style.configure('TNotebook.Tab',
                       background=c.background_tertiary,
                       foreground=c.text,
                       padding=[8, 12])
        style.map('TNotebook.Tab',
                 background=[('selected', c.primary)],
                 foreground=[('selected', 'white')])

    def create_button(self, text: str, callback: Callable) -> ttk.Button:
        btn = ttk.Button(text=text, command=callback)
        return btn

    def create_label(self, text: str) -> ttk.Label:
        return ttk.Label(text=text)

    def create_text_area(self, readonly: bool = True) -> scrolledtext.ScrolledText:
        text_area = scrolledtext.ScrolledText(wrap=tk.WORD)
        if readonly:
            text_area.config(state='disabled')
        return text_area

    def create_progress_bar(self) -> ttk.Progressbar:
        return ttk.Progressbar(mode='determinate')

    def create_combo_box(self, items: List[str], callback: Callable) -> ttk.Combobox:
        combo = ttk.Combobox(values=items, state='readonly')
        combo.set(items[0] if items else "")
        combo.bind('<<ComboboxSelected>>', lambda e: callback(combo.get()))
        return combo

    def create_checkbox(self, text: str, callback: Callable) -> ttk.Checkbutton:
        var = tk.BooleanVar()
        checkbox = ttk.Checkbutton(text=text, variable=var, command=lambda: callback(var.get()))
        return checkbox

    def create_spin_box(self, min_val: int, max_val: int, value: int, callback: Callable) -> ttk.Spinbox:
        spin = ttk.Spinbox(from_=min_val, to=max_val, value=value, command=lambda: callback(int(spin.get())))
        return spin

    def create_group_box(self, title: str) -> ttk.LabelFrame:
        return ttk.LabelFrame(text=title)

    def create_tab_widget(self) -> ttk.Notebook:
        return ttk.Notebook()

    def create_splitter(self, horizontal: bool = True) -> ttk.PanedWindow:
        orient = tk.HORIZONTAL if horizontal else tk.VERTICAL
        return ttk.PanedWindow(orient=orient)

    def add_widget_to_layout(self, parent: Any, widget: Any, expand: bool = False) -> None:
        if isinstance(parent, ttk.Frame) or isinstance(parent, tk.Frame):
            widget.pack(fill=tk.BOTH, expand=expand, padx=5, pady=5)
        elif isinstance(parent, ttk.LabelFrame):
            widget.pack(fill=tk.BOTH, expand=expand, padx=5, pady=5)
        elif isinstance(parent, ttk.Notebook):
            parent.add(widget, text=widget.winfo_name() or "Tab")
        elif isinstance(parent, ttk.PanedWindow):
            parent.add(widget)

    def set_widget_text(self, widget: Any, text: str) -> None:
        if isinstance(widget, (ttk.Button, ttk.Label, ttk.Checkbutton)):
            widget.config(text=text)
        elif isinstance(widget, scrolledtext.ScrolledText):
            widget.config(state='normal')
            widget.delete(1.0, tk.END)
            widget.insert(tk.END, text)
            widget.config(state='disabled')

    def get_widget_text(self, widget: Any) -> str:
        if isinstance(widget, (ttk.Button, ttk.Label, ttk.Checkbutton)):
            return widget.cget('text')
        elif isinstance(widget, scrolledtext.ScrolledText):
            widget.config(state='normal')
            text = widget.get(1.0, tk.END)
            widget.config(state='disabled')
            return text
        return ""

    def set_progress_value(self, widget: Any, value: int) -> None:
        if isinstance(widget, ttk.Progressbar):
            widget['value'] = value

    def set_checkbox_state(self, widget: Any, checked: bool) -> None:
        if isinstance(widget, ttk.Checkbutton):
            variable = widget.tk.call(widget, 'cget', '-variable')
            self.root.globalsetvar(variable, checked)

    def get_checkbox_state(self, widget: Any) -> bool:
        if isinstance(widget, ttk.Checkbutton):
            variable = widget.tk.call(widget, 'cget', '-variable')
            return self.root.globalgetvar(variable)
        return False

    def get_combo_box_value(self, widget: Any) -> str:
        if isinstance(widget, ttk.Combobox):
            return widget.get()
        return ""

    def set_combo_box_value(self, widget: Any, value: str) -> None:
        if isinstance(widget, ttk.Combobox):
            widget.set(value)

    def append_text(self, widget: Any, text: str) -> None:
        if isinstance(widget, scrolledtext.ScrolledText):
            widget.config(state='normal')
            widget.insert(tk.END, text)
            widget.see(tk.END)
            widget.config(state='disabled')

    def clear_text(self, widget: Any) -> None:
        if isinstance(widget, scrolledtext.ScrolledText):
            widget.config(state='normal')
            widget.delete(1.0, tk.END)
            widget.config(state='disabled')

    def show_message(self, title: str, message: str, msg_type: str = "info") -> None:
        if msg_type == "info":
            messagebox.showinfo(title, message)
        elif msg_type == "warning":
            messagebox.showwarning(title, message)
        elif msg_type == "error":
            messagebox.showerror(title, message)
        elif msg_type == "question":
            messagebox.askyesno(title, message)

    def show_file_dialog(self, title: str, mode: str = "open", file_filter: str = "") -> Optional[str]:
        if mode == "open":
            file_path = filedialog.askopenfilename(title=title, filetypes=[("All Files", "*.*")])
        else:
            file_path = filedialog.asksaveasfilename(title=title, filetypes=[("All Files", "*.*")])
        return file_path if file_path else None

    def run(self) -> int:
        if self.root:
            self.root.mainloop()
        return 0

    def exit(self) -> None:
        if self.root:
            self.root.quit()