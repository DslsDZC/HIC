#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统 GTK GUI 后端
"""

from typing import Dict, List, Optional, Any, Callable
from .gui_base import GUIBackend
from style_manager import get_style_manager

try:
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk, GLib, Gdk
except ImportError:
    raise ImportError("GTK3 not installed")


class GtkBackend(GUIBackend):
    """GTK GUI 后端"""

    def __init__(self):
        self.window = None
        self.current_container = None
        self.style_manager = get_style_manager()

    def create_window(self, title: str, width: int, height: int) -> Gtk.ApplicationWindow:
        app = Gtk.Application(application_id='com.hic.build')
        app.connect('activate', self._on_activate)
        self.window_title = title
        self.window_size = (width, height)
        self.app = app
        return None  # 窗口在 activate 回调中创建

    def _on_activate(self, app):
        window = Gtk.ApplicationWindow(application=app)
        window.set_title(self.window_title)
        window.set_default_size(*self.window_size)
        window.show_all()
        self.window = window
        
        # 应用 GTK 样式
        screen = Gdk.Screen.get_default()
        provider = Gtk.CssProvider()
        css = self.style_manager.get_css('gtk')
        provider.load_from_data(css.encode())
        Gtk.StyleContext.add_provider_for_screen(
            screen, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    def create_button(self, text: str, callback: Callable) -> Gtk.Button:
        btn = Gtk.Button(label=text)
        btn.connect('clicked', lambda w: callback())
        return btn

    def create_label(self, text: str) -> Gtk.Label:
        return Gtk.Label(label=text)

    def create_text_area(self, readonly: bool = True) -> Gtk.TextView:
        text_view = Gtk.TextView()
        text_view.set_editable(not readonly)
        text_buffer = text_view.get_buffer()
        text_buffer.set_text("", 0)
        return text_view

    def create_progress_bar(self) -> Gtk.ProgressBar:
        return Gtk.ProgressBar()

    def create_combo_box(self, items: List[str], callback: Callable) -> Gtk.ComboBoxText:
        combo = Gtk.ComboBoxText()
        for item in items:
            combo.append_text(item)
        combo.set_active(0)
        combo.connect('changed', lambda w: callback(combo.get_active_text()))
        return combo

    def create_checkbox(self, text: str, callback: Callable) -> Gtk.CheckButton:
        checkbox = Gtk.CheckButton(label=text)
        checkbox.connect('toggled', lambda w: callback(checkbox.get_active()))
        return checkbox

    def create_spin_box(self, min_val: int, max_val: int, value: int, callback: Callable) -> Gtk.SpinButton:
        spin = Gtk.SpinButton()
        adjustment = Gtk.Adjustment(value=value, lower=min_val, upper=max_val, step_increment=1)
        spin.set_adjustment(adjustment)
        spin.connect('value-changed', lambda w: callback(int(w.get_value())))
        return spin

    def create_group_box(self, title: str) -> Gtk.Frame:
        frame = Gtk.Frame(label=title)
        frame.get_label_widget().get_style_context().add_class('title')
        return frame

    def create_tab_widget(self) -> Gtk.Notebook:
        return Gtk.Notebook()

    def create_splitter(self, horizontal: bool = True) -> Gtk.Paned:
        paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL if horizontal else Gtk.Orientation.VERTICAL)
        return paned

    def add_widget_to_layout(self, parent: Any, widget: Any, expand: bool = False) -> None:
        if isinstance(parent, Gtk.Box):
            parent.pack_start(widget, True, True, 0)
        elif isinstance(parent, Gtk.Frame):
            child_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
            child_box.pack_start(widget, True, True, 0)
            parent.add(child_box)
        elif isinstance(parent, Gtk.Notebook):
            page_num = parent.append_page(widget, Gtk.Label(label=widget.get_name() or "Tab"))
        elif isinstance(parent, Gtk.Paned):
            parent.add2(widget)
        elif isinstance(parent, (Gtk.Grid, Gtk.ListBox)):
            parent.add(widget)

    def set_widget_text(self, widget: Any, text: str) -> None:
        if isinstance(widget, (Gtk.Button, Gtk.Label, Gtk.CheckButton)):
            widget.set_label(text)
        elif isinstance(widget, Gtk.TextView):
            buffer = widget.get_buffer()
            buffer.set_text(text, len(text))

    def get_widget_text(self, widget: Any) -> str:
        if isinstance(widget, (Gtk.Button, Gtk.Label)):
            return widget.get_label()
        elif isinstance(widget, Gtk.CheckButton):
            return widget.get_label()
        elif isinstance(widget, Gtk.TextView):
            buffer = widget.get_buffer()
            start, end = buffer.get_bounds()
            return buffer.get_text(start, end, True)
        return ""

    def set_progress_value(self, widget: Any, value: int) -> None:
        if isinstance(widget, Gtk.ProgressBar):
            widget.set_fraction(value / 100.0)

    def set_checkbox_state(self, widget: Any, checked: bool) -> None:
        if isinstance(widget, Gtk.CheckButton):
            widget.set_active(checked)

    def get_checkbox_state(self, widget: Any) -> bool:
        if isinstance(widget, Gtk.CheckButton):
            return widget.get_active()
        return False

    def get_combo_box_value(self, widget: Any) -> str:
        if isinstance(widget, Gtk.ComboBoxText):
            return widget.get_active_text()
        return ""

    def set_combo_box_value(self, widget: Any, value: str) -> None:
        if isinstance(widget, Gtk.ComboBoxText):
            for i in range(widget.get_model().iter_n_children(None)):
                tree_iter = widget.get_model().iter_nth_child(None, i)
                if widget.get_model()[tree_iter][0] == value:
                    widget.set_active(i)
                    break

    def append_text(self, widget: Any, text: str) -> None:
        if isinstance(widget, Gtk.TextView):
            buffer = widget.get_buffer()
            end_iter = buffer.get_end_iter()
            buffer.insert(end_iter, text)
            widget.scroll_to_mark(buffer.get_insert(), 0.0, False, 0.0, 0.0)

    def clear_text(self, widget: Any) -> None:
        if isinstance(widget, Gtk.TextView):
            buffer = widget.get_buffer()
            buffer.set_text("", 0)

    def show_message(self, title: str, message: str, msg_type: str = "info") -> None:
        dialog = Gtk.MessageDialog(
            transient_for=self.window,
            flags=0,
            message_type=Gtk.MessageType.INFO if msg_type == "info" else (
                Gtk.MessageType.WARNING if msg_type == "warning" else (
                    Gtk.MessageType.ERROR if msg_type == "error" else Gtk.MessageType.QUESTION
                )
            ),
            buttons=Gtk.ButtonsType.OK,
            text=title,
        )
        dialog.format_secondary_text(message)
        dialog.run()
        dialog.destroy()

    def show_file_dialog(self, title: str, mode: str = "open", file_filter: str = "") -> Optional[str]:
        action = Gtk.FileChooserAction.OPEN if mode == "open" else Gtk.FileChooserAction.SAVE
        dialog = Gtk.FileChooserDialog(
            title=title,
            parent=self.window,
            action=action
        )
        dialog.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_OPEN if mode == "open" else Gtk.STOCK_SAVE, Gtk.ResponseType.OK
        )

        response = dialog.run()
        file_path = dialog.get_filename()
        dialog.destroy()

        if response == Gtk.ResponseType.OK:
            return file_path
        return None

    def run(self) -> int:
        if self.app:
            return self.app.run()
        return 0

    def exit(self) -> None:
        if self.window:
            self.window.close()
        if self.app:
            self.app.quit()