#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统 Qt GUI 后端
"""

from typing import Dict, List, Optional, Any, Callable
from .gui_base import GUIBackend
from style_manager import get_style_manager

try:
    from PyQt6.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QPushButton, QLabel, QTextEdit, QProgressBar, QTabWidget,
        QGroupBox, QFormLayout, QComboBox, QSpinBox, QCheckBox,
        QSplitter, QFileDialog, QMessageBox
    )
    from PyQt6.QtCore import Qt, QObject, pyqtSignal as Signal
    from PyQt6.QtGui import QPalette, QColor
except ImportError:
    raise ImportError("PyQt6 not installed")


class QtBackend(GUIBackend):
    """Qt GUI 后端"""

    def __init__(self):
        self.app = None
        self.window = None
        self.current_layout = None
        self.style_manager = get_style_manager()

    def create_window(self, title: str, width: int, height: int) -> QMainWindow:
        self.app = QApplication([])
        self.window = QMainWindow()
        self.window.setWindowTitle(title)
        self.window.resize(width, height)
        
        # 应用 Qt 样式
        self.app.setStyleSheet(self.style_manager.get_css('qt'))
        
        return self.window

    def create_button(self, text: str, callback: Callable) -> QPushButton:
        btn = QPushButton(text)
        btn.clicked.connect(callback)
        return btn

    def create_label(self, text: str) -> QLabel:
        return QLabel(text)

    def create_text_area(self, readonly: bool = True) -> QTextEdit:
        text_area = QTextEdit()
        text_area.setReadOnly(readonly)
        return text_area

    def create_progress_bar(self) -> QProgressBar:
        return QProgressBar()

    def create_combo_box(self, items: List[str], callback: Callable) -> QComboBox:
        combo = QComboBox()
        combo.addItems(items)
        combo.currentTextChanged.connect(callback)
        return combo

    def create_checkbox(self, text: str, callback: Callable) -> QCheckBox:
        checkbox = QCheckBox(text)
        checkbox.stateChanged.connect(callback)
        return checkbox

    def create_spin_box(self, min_val: int, max_val: int, value: int, callback: Callable) -> QSpinBox:
        spin = QSpinBox()
        spin.setMinimum(min_val)
        spin.setMaximum(max_val)
        spin.setValue(value)
        spin.valueChanged.connect(callback)
        return spin

    def create_group_box(self, title: str) -> QGroupBox:
        return QGroupBox(title)

    def create_tab_widget(self) -> QTabWidget:
        return QTabWidget()

    def create_splitter(self, horizontal: bool = True) -> QSplitter:
        splitter = QSplitter(Qt.Orientation.Horizontal if horizontal else Qt.Orientation.Vertical)
        return splitter

    def add_widget_to_layout(self, parent: Any, widget: Any, expand: bool = False) -> None:
        if isinstance(parent, QVBoxLayout):
            parent.addWidget(widget)
            if expand:
                parent.addStretch()
        elif isinstance(parent, QHBoxLayout):
            parent.addWidget(widget)
            if expand:
                parent.addStretch()
        elif isinstance(parent, QGroupBox):
            if parent.layout() is None:
                parent.setLayout(QVBoxLayout())
            parent.layout().addWidget(widget)
        elif isinstance(parent, QTabWidget):
            parent.addTab(widget, widget.objectName() or "Tab")
        elif isinstance(parent, QSplitter):
            parent.addWidget(widget)

    def set_widget_text(self, widget: Any, text: str) -> None:
        if isinstance(widget, (QPushButton, QLabel, QCheckBox)):
            widget.setText(text)
        elif isinstance(widget, QTextEdit):
            widget.setPlainText(text)

    def get_widget_text(self, widget: Any) -> str:
        if isinstance(widget, (QPushButton, QLabel, QCheckBox)):
            return widget.text()
        elif isinstance(widget, QTextEdit):
            return widget.toPlainText()
        return ""

    def set_progress_value(self, widget: Any, value: int) -> None:
        if isinstance(widget, QProgressBar):
            widget.setValue(value)

    def set_checkbox_state(self, widget: Any, checked: bool) -> None:
        if isinstance(widget, QCheckBox):
            widget.setChecked(checked)

    def get_checkbox_state(self, widget: Any) -> bool:
        if isinstance(widget, QCheckBox):
            return widget.isChecked()
        return False

    def get_combo_box_value(self, widget: Any) -> str:
        if isinstance(widget, QComboBox):
            return widget.currentText()
        return ""

    def set_combo_box_value(self, widget: Any, value: str) -> None:
        if isinstance(widget, QComboBox):
            index = widget.findText(value)
            if index >= 0:
                widget.setCurrentIndex(index)

    def append_text(self, widget: Any, text: str) -> None:
        if isinstance(widget, QTextEdit):
            widget.append(text)

    def clear_text(self, widget: Any) -> None:
        if isinstance(widget, QTextEdit):
            widget.clear()

    def show_message(self, title: str, message: str, msg_type: str = "info") -> None:
        if msg_type == "info":
            QMessageBox.information(self.window, title, message)
        elif msg_type == "warning":
            QMessageBox.warning(self.window, title, message)
        elif msg_type == "error":
            QMessageBox.critical(self.window, title, message)
        elif msg_type == "question":
            QMessageBox.question(self.window, title, message)

    def show_file_dialog(self, title: str, mode: str = "open", file_filter: str = "") -> Optional[str]:
        if mode == "open":
            file_path, _ = QFileDialog.getOpenFileName(self.window, title, "", file_filter)
        else:
            file_path, _ = QFileDialog.getSaveFileName(self.window, title, "", file_filter)
        return file_path if file_path else None

    def run(self) -> int:
        if self.app:
            return self.app.exec()
        return 0

    def exit(self) -> None:
        if self.app:
            self.app.quit()