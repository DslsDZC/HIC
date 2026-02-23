#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统 Web GUI 后端
使用 Flask + WebSocket 实现浏览器界面
"""

from typing import Dict, List, Optional, Any, Callable
import gui_base
from gui_base import GUIBackend

try:
    from flask import Flask, render_template, request, jsonify
    from flask_socketio import SocketIO, emit
    import threading
    import uuid
except ImportError:
    raise ImportError("Flask and Flask-SocketIO not installed. Install with: pip install flask flask-socketio")


class WebBackend(GUIBackend):
    """Web GUI 后端"""

    def __init__(self):
        self.app = Flask(__name__)
        self.app.config['SECRET_KEY'] = 'hic-secret-key'
        self.socketio = SocketIO(self.app, async_mode='threading')
        self.window = None
        self.widgets = {}
        self.widget_callbacks = {}

        # 设置路由
        self._setup_routes()
        self._setup_socket_events()

    def _setup_routes(self):
        """设置 Flask 路由"""

        @self.app.route('/')
        def index():
            return render_template('hic_gui.html')

        @self.app.route('/api/command', methods=['POST'])
        def handle_command():
            data = request.json
            widget_id = data.get('widget_id')
            event_type = data.get('event_type')
            value = data.get('value')

            if widget_id in self.widget_callbacks:
                callback = self.widget_callbacks[widget_id]
                callback(value)

            return jsonify({'status': 'success'})

    def _setup_socket_events(self):
        """设置 Socket.IO 事件"""

        @self.socketio.on('connect')
        def handle_connect():
            print('客户端已连接')

        @self.socketio.on('disconnect')
        def handle_disconnect():
            print('客户端已断开')

    def create_window(self, title: str, width: int, height: int) -> Any:
        self.window = {
            'title': title,
            'width': width,
            'height': height
        }
        return self.window

    def create_button(self, text: str, callback: Callable) -> str:
        widget_id = str(uuid.uuid4())
        self.widget_callbacks[widget_id] = callback
        self.widgets[widget_id] = {
            'type': 'button',
            'text': text,
            'id': widget_id
        }
        return widget_id

    def create_label(self, text: str) -> str:
        widget_id = str(uuid.uuid4())
        self.widgets[widget_id] = {
            'type': 'label',
            'text': text,
            'id': widget_id
        }
        return widget_id

    def create_text_area(self, readonly: bool = True) -> str:
        widget_id = str(uuid.uuid4())
        self.widgets[widget_id] = {
            'type': 'textarea',
            'readonly': readonly,
            'id': widget_id,
            'content': ''
        }
        return widget_id

    def create_progress_bar(self) -> str:
        widget_id = str(uuid.uuid4())
        self.widgets[widget_id] = {
            'type': 'progress',
            'value': 0,
            'id': widget_id
        }
        return widget_id

    def create_combo_box(self, items: List[str], callback: Callable) -> str:
        widget_id = str(uuid.uuid4())
        self.widget_callbacks[widget_id] = callback
        self.widgets[widget_id] = {
            'type': 'combobox',
            'items': items,
            'selected': items[0] if items else '',
            'id': widget_id
        }
        return widget_id

    def create_checkbox(self, text: str, callback: Callable) -> str:
        widget_id = str(uuid.uuid4())
        self.widget_callbacks[widget_id] = callback
        self.widgets[widget_id] = {
            'type': 'checkbox',
            'text': text,
            'checked': False,
            'id': widget_id
        }
        return widget_id

    def create_spin_box(self, min_val: int, max_val: int, value: int, callback: Callable) -> str:
        widget_id = str(uuid.uuid4())
        self.widget_callbacks[widget_id] = callback
        self.widgets[widget_id] = {
            'type': 'spinbox',
            'min': min_val,
            'max': max_val,
            'value': value,
            'id': widget_id
        }
        return widget_id

    def create_group_box(self, title: str) -> str:
        widget_id = str(uuid.uuid4())
        self.widgets[widget_id] = {
            'type': 'groupbox',
            'title': title,
            'children': [],
            'id': widget_id
        }
        return widget_id

    def create_tab_widget(self) -> str:
        widget_id = str(uuid.uuid4())
        self.widgets[widget_id] = {
            'type': 'tabwidget',
            'tabs': [],
            'id': widget_id
        }
        return widget_id

    def create_splitter(self, horizontal: bool = True) -> str:
        widget_id = str(uuid.uuid4())
        self.widgets[widget_id] = {
            'type': 'splitter',
            'horizontal': horizontal,
            'children': [],
            'id': widget_id
        }
        return widget_id

    def add_widget_to_layout(self, parent: Any, widget: Any, expand: bool = False) -> None:
        if parent in self.widgets:
            parent_widget = self.widgets[parent]
            if 'children' in parent_widget:
                parent_widget['children'].append({
                    'widget': widget,
                    'expand': expand
                })

    def set_widget_text(self, widget: Any, text: str) -> None:
        if widget in self.widgets:
            self.widgets[widget]['text'] = text
            self._emit_widget_update(widget)

    def get_widget_text(self, widget: Any) -> str:
        if widget in self.widgets:
            return self.widgets[widget].get('text', '')
        return ""

    def set_progress_value(self, widget: Any, value: int) -> None:
        if widget in self.widgets:
            self.widgets[widget]['value'] = value
            self._emit_widget_update(widget)

    def set_checkbox_state(self, widget: Any, checked: bool) -> None:
        if widget in self.widgets:
            self.widgets[widget]['checked'] = checked
            self._emit_widget_update(widget)

    def get_checkbox_state(self, widget: Any) -> bool:
        if widget in self.widgets:
            return self.widgets[widget].get('checked', False)
        return False

    def get_combo_box_value(self, widget: Any) -> str:
        if widget in self.widgets:
            return self.widgets[widget].get('selected', '')
        return ""

    def set_combo_box_value(self, widget: Any, value: str) -> None:
        if widget in self.widgets:
            self.widgets[widget]['selected'] = value
            self._emit_widget_update(widget)

    def append_text(self, widget: Any, text: str) -> None:
        if widget in self.widgets:
            self.widgets[widget]['content'] += text
            self._emit_widget_update(widget)

    def clear_text(self, widget: Any) -> None:
        if widget in self.widgets:
            self.widgets[widget]['content'] = ''
            self._emit_widget_update(widget)

    def show_message(self, title: str, message: str, msg_type: str = "info") -> None:
        self.socketio.emit('message', {
            'title': title,
            'message': message,
            'type': msg_type
        })

    def show_file_dialog(self, title: str, mode: str = "open", file_filter: str = "") -> Optional[str]:
        # 文件对话框需要客户端实现
        self.socketio.emit('file_dialog', {
            'title': title,
            'mode': mode,
            'filter': file_filter
        })
        # 这里需要等待客户端响应
        return None

    def _emit_widget_update(self, widget_id: str):
        """发送组件更新到客户端"""
        if widget_id in self.widgets:
            self.socketio.emit('widget_update', {
                'id': widget_id,
                'data': self.widgets[widget_id]
            })

    def run(self):
        """运行 Web 应用"""
        # 注入 CSS 样式
        try:
            import style_manager
            css = style_manager.get_css('web') if hasattr(style_manager, 'get_css') else ""
        except (ImportError, AttributeError):
            css = ""  # 如果没有样式管理器，使用默认样式

        # 修改模板以注入 CSS
        original_render = self.app.template_folder

        self.app.run(host='0.0.0.0', port=5000, debug=False)

    def exit(self) -> None:
        self.socketio.stop()