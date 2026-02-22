#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HIC 构建系统 - 样式管理器
统一管理所有界面的样式，遵循 Qt 风格
"""

import os
import configparser
from pathlib import Path
from typing import Dict, Any, Optional
from dataclasses import dataclass


@dataclass
class ColorScheme:
    """颜色方案"""
    primary: str
    primary_hover: str
    primary_pressed: str
    background: str
    background_secondary: str
    background_tertiary: str
    text: str
    text_secondary: str
    text_disabled: str
    border: str
    border_focus: str
    success: str
    warning: str
    error: str
    info: str


@dataclass
class Typography:
    """排版设置"""
    font_family: str
    font_size_small: int
    font_size_normal: int
    font_size_large: int
    font_size_title: int
    font_size_header: int


@dataclass
class Layout:
    """布局设置"""
    spacing_xs: int
    spacing_sm: int
    spacing_md: int
    spacing_lg: int
    spacing_xl: int
    padding_xs: int
    padding_sm: int
    padding_md: int
    padding_lg: int
    padding_xl: int
    radius_sm: int
    radius_md: int
    radius_lg: int


@dataclass
class Animations:
    """动画设置"""
    duration_fast: int
    duration_normal: int
    duration_slow: int


@dataclass
class QtStyleConfig:
    """Qt 风格配置"""
    color_scheme: ColorScheme
    typography: Typography
    layout: Layout
    animations: Animations
    
    # 其他配置
    window_width: int
    window_height: int
    theme: str


class StyleManager:
    """样式管理器"""

    def __init__(self, config_path: Optional[Path] = None):
        if config_path is None:
            config_path = Path(__file__).parent / 'qt_style.conf'
        
        self.config_path = config_path
        self.config = self._load_config()
        self.qt_style = self._parse_config()

    def _load_config(self) -> configparser.ConfigParser:
        """加载配置文件"""
        config = configparser.ConfigParser()
        
        if self.config_path.exists():
            config.read(self.config_path, encoding='utf-8')
        else:
            # 使用默认配置
            self._create_default_config()
            config.read(self.config_path, encoding='utf-8')
        
        return config

    def _create_default_config(self) -> None:
        """创建默认配置文件"""
        pass  # 配置文件已经存在

    def _parse_config(self) -> QtStyleConfig:
        """解析配置"""
        # 解析颜色方案
        colors = self.config['Colors']
        color_scheme = ColorScheme(
            primary=colors.get('primary', '#4a9eff'),
            primary_hover=colors.get('primary_hover', '#3a8eef'),
            primary_pressed=colors.get('primary_pressed', '#2a7edf'),
            background=colors.get('background', '#1a1a1a'),
            background_secondary=colors.get('background_secondary', '#2d2d2d'),
            background_tertiary=colors.get('background_tertiary', '#3d3d3d'),
            text=colors.get('text', '#e0e0e0'),
            text_secondary=colors.get('text_secondary', '#b0b0b0'),
            text_disabled=colors.get('text_disabled', '#666666'),
            border=colors.get('border', '#3d3d3d'),
            border_focus=colors.get('border_focus', '#4a9eff'),
            success=colors.get('success', '#4caf50'),
            warning=colors.get('warning', '#ff9800'),
            error=colors.get('error', '#f44336'),
            info=colors.get('info', '#2196f3'),
        )

        # 解析排版
        typography = self.config['Typography']
        typo = Typography(
            font_family=typography.get('font_family', 'Microsoft YaHei'),
            font_size_small=typography.getint('font_size_small', 8),
            font_size_normal=typography.getint('font_size_normal', 10),
            font_size_large=typography.getint('font_size_large', 12),
            font_size_title=typography.getint('font_size_title', 14),
            font_size_header=typography.getint('font_size_header', 16),
        )

        # 解析布局
        layout = self.config['Layout']
        lay = Layout(
            spacing_xs=layout.getint('spacing_xs', 4),
            spacing_sm=layout.getint('spacing_sm', 8),
            spacing_md=layout.getint('spacing_md', 12),
            spacing_lg=layout.getint('spacing_lg', 16),
            spacing_xl=layout.getint('spacing_xl', 24),
            padding_xs=layout.getint('padding_xs', 4),
            padding_sm=layout.getint('padding_sm', 8),
            padding_md=layout.getint('padding_md', 12),
            padding_lg=layout.getint('padding_lg', 16),
            padding_xl=layout.getint('padding_xl', 24),
            radius_sm=layout.getint('radius_sm', 4),
            radius_md=layout.getint('radius_md', 8),
            radius_lg=layout.getint('radius_lg', 12),
        )

        # 解析动画
        animations = self.config['Animations']
        anim = Animations(
            duration_fast=animations.getint('duration_fast', 150),
            duration_normal=animations.getint('duration_normal', 300),
            duration_slow=animations.getint('duration_slow', 500),
        )

        # 窗口设置
        appearance = self.config['Appearance']
        window_width = appearance.getint('window_width', 1200)
        window_height = appearance.getint('window_height', 800)
        theme = appearance.get('theme', 'dark')

        return QtStyleConfig(
            color_scheme=color_scheme,
            typography=typo,
            layout=lay,
            animations=anim,
            window_width=window_width,
            window_height=window_height,
            theme=theme,
        )

    def get_css(self, backend: str = 'qt') -> str:
        """获取 CSS 样式"""
        style = self.qt_style
        
        if backend == 'qt':
            return self._generate_qt_css(style)
        elif backend == 'web':
            return self._generate_web_css(style)
        elif backend == 'gtk':
            return self._generate_gtk_css(style)
        else:
            return ""

    def _generate_qt_css(self, style: QtStyleConfig) -> str:
        """生成 Qt CSS"""
        c = style.color_scheme
        l = style.layout
        
        css = f"""
/* HIC 构建系统 - Qt 样式 */

QWidget {{
    background-color: {c.background};
    color: {c.text};
    font-family: {style.typography.font_family};
    font-size: {style.typography.font_size_normal}pt;
}}

QMainWindow {{
    background-color: {c.background};
}}

QPushButton {{
    background-color: {c.primary};
    color: white;
    border: none;
    border-radius: {l.radius_md}px;
    padding: {l.padding_sm}px {l.padding_lg}px;
    font-size: {style.typography.font_size_normal}pt;
}}

QPushButton:hover {{
    background-color: {c.primary_hover};
}}

QPushButton:pressed {{
    background-color: {c.primary_pressed};
}}

QPushButton:disabled {{
    background-color: {c.text_disabled};
    color: {c.background};
}}

QLabel {{
    color: {c.text};
    background-color: transparent;
}}

QLineEdit, QTextEdit, QPlainTextEdit {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
    border-radius: {l.radius_sm}px;
    padding: {l.padding_sm}px;
}}

QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {{
    border: 1px solid {c.border_focus};
}}

QComboBox {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
    border-radius: {l.radius_sm}px;
    padding: {l.padding_sm}px;
}}

QComboBox:hover {{
    border: 1px solid {c.border_focus};
}}

QComboBox::drop-down {{
    border: none;
}}

QProgressBar {{
    background-color: {c.background_secondary};
    border: none;
    border-radius: {l.radius_sm}px;
    height: 8px;
}}

QProgressBar::chunk {{
    background-color: {c.primary};
    border-radius: {l.radius_sm}px;
}}

QGroupBox {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
    border-radius: {l.radius_md}px;
    margin-top: {l.spacing_md}px;
    padding: {l.padding_md}px;
}}

QGroupBox::title {{
    color: {c.primary};
    font-weight: bold;
}}

QTabWidget::pane {{
    background-color: {c.background_secondary};
    border: 1px solid {c.border};
    border-radius: {l.radius_md}px;
}}

QTabBar::tab {{
    background-color: {c.background_tertiary};
    color: {c.text};
    padding: {l.padding_sm}px {l.padding_lg}px;
    border: none;
    border-top-left-radius: {l.radius_sm}px;
    border-top-right-radius: {l.radius_sm}px;
}}

QTabBar::tab:selected {{
    background-color: {c.primary};
    color: white;
}}

QScrollBar:vertical {{
    background-color: {c.background_secondary};
    width: 12px;
    border-radius: {l.radius_sm}px;
}}

QScrollBar::handle:vertical {{
    background-color: {c.text_disabled};
    border-radius: {l.radius_sm}px;
}}

QScrollBar::handle:vertical:hover {{
    background-color: {c.text};
}}

QScrollBar:horizontal {{
    background-color: {c.background_secondary};
    height: 12px;
    border-radius: {l.radius_sm}px;
}}

QScrollBar::handle:horizontal {{
    background-color: {c.text_disabled};
    border-radius: {l.radius_sm}px;
}}

QScrollBar::handle:horizontal:hover {{
    background-color: {c.text};
}}

QStatusBar {{
    background-color: {c.background_secondary};
    color: {c.text_secondary};
    border-top: 1px solid {c.border};
}}

QMenuBar {{
    background-color: {c.background_secondary};
    color: {c.text};
    border-bottom: 1px solid {c.border};
}}

QMenuBar::item {{
    padding: {l.padding_sm}px {l.padding_md}px;
}}

QMenuBar::item:selected {{
    background-color: {c.primary};
    color: white;
}}

QMenu {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
}}

QMenu::item {{
    padding: {l.padding_sm}px {l.padding_lg}px;
}}

QMenu::item:selected {{
    background-color: {c.primary};
    color: white;
}}
"""
        return css

    def _generate_web_css(self, style: QtStyleConfig) -> str:
        """生成 Web CSS"""
        c = style.color_scheme
        l = style.layout
        
        css = f"""
/* HIC 构建系统 - Web 样式 */

* {{
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}}

body {{
    font-family: {style.typography.font_family}, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background-color: {c.background};
    color: {c.text};
    font-size: {style.typography.font_size_normal}px;
    line-height: 1.5;
}}

.container {{
    max-width: {style.window_width}px;
    margin: 0 auto;
    padding: {l.padding_lg}px;
}}

button {{
    background-color: {c.primary};
    color: white;
    border: none;
    border-radius: {l.radius_md}px;
    padding: {l.padding_sm}px {l.padding_lg}px;
    font-size: {style.typography.font_size_normal}px;
    cursor: pointer;
    transition: background-color {style.animations.duration_fast}ms ease;
}}

button:hover {{
    background-color: {c.primary_hover};
}}

button:active {{
    background-color: {c.primary_pressed};
}}

button:disabled {{
    background-color: {c.text_disabled};
    cursor: not-allowed;
}}

input, textarea, select {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
    border-radius: {l.radius_sm}px;
    padding: {l.padding_sm}px;
    font-size: {style.typography.font_size_normal}px;
}}

input:focus, textarea:focus, select:focus {{
    outline: none;
    border-color: {c.border_focus};
}}

.progress-bar {{
    background-color: {c.background_secondary};
    border-radius: {l.radius_sm}px;
    height: 8px;
    overflow: hidden;
}}

.progress-fill {{
    background-color: {c.primary};
    height: 100%;
    border-radius: {l.radius_sm}px;
    transition: width {style.animations.duration_normal}ms ease;
}}

.group-box {{
    background-color: {c.background_secondary};
    border: 1px solid {c.border};
    border-radius: {l.radius_md}px;
    padding: {l.padding_md}px;
    margin-bottom: {l.spacing_lg}px;
}}

.group-box h3 {{
    color: {c.primary};
    font-size: {style.typography.font_size_title}px;
    margin-bottom: {l.spacing_md}px;
}}

.tabs {{
    display: flex;
    gap: {l.spacing_sm}px;
    border-bottom: 1px solid {c.border};
    margin-bottom: {l.spacing_md}px;
}}

.tab {{
    padding: {l.padding_sm}px {l.padding_lg}px;
    background-color: {c.background_tertiary};
    color: {c.text};
    border-radius: {l.radius_sm}px {l.radius_sm}px 0 0;
    cursor: pointer;
    transition: background-color {style.animations.duration_fast}ms ease;
}}

.tab.active {{
    background-color: {c.primary};
    color: white;
}}

.status-bar {{
    background-color: {c.background_secondary};
    color: {c.text_secondary};
    padding: {l.padding_sm}px {l.padding_lg}px;
    border-top: 1px solid {c.border};
}}
"""
        return css

    def _generate_gtk_css(self, style: QtStyleConfig) -> str:
        """生成 GTK CSS"""
        c = style.color_scheme
        l = style.layout
        
        css = f"""
/* HIC 构建系统 - GTK 样式 */

* {{
    background-color: {c.background};
    color: {c.text};
    font-family: {style.typography.font_family};
}}

window {{
    background-color: {c.background};
}}

button {{
    background-color: {c.primary};
    color: white;
    border: none;
    border-radius: {l.radius_md}px;
    padding: {l.padding_sm}px {l.padding_lg}px;
    font-size: {style.typography.font_size_normal}px;
}}

button:hover {{
    background-color: {c.primary_hover};
}}

button:active {{
    background-color: {c.primary_pressed};
}}

button:disabled {{
    background-color: {c.text_disabled};
}}

entry {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
    border-radius: {l.radius_sm}px;
    padding: {l.padding_sm}px;
}}

entry:focus {{
    border-color: {c.border_focus};
}}

textview {{
    background-color: {c.background_secondary};
    color: {c.text};
}}

text {{
    background-color: {c.background_secondary};
    color: {c.text};
}}

combobox {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
    border-radius: {l.radius_sm}px;
    padding: {l.padding_sm}px;
}}

progressbar {{
    background-color: {c.background_secondary};
    border-radius: {l.radius_sm}px;
}}

progressbar progress {{
    background-color: {c.primary};
    border-radius: {l.radius_sm}px;
}}

frame {{
    background-color: {c.background_secondary};
    border: 1px solid {c.border};
    border-radius: {l.radius_md}px;
    padding: {l.padding_md}px;
}}

frame > label {{
    color: {c.primary};
    font-weight: bold;
}}

notebook {{
    background-color: {c.background_secondary};
    border: 1px solid {c.border};
    border-radius: {l.radius_md}px;
}}

notebook tab {{
    background-color: {c.background_tertiary};
    color: {c.text};
    padding: {l.padding_sm}px {l.padding_lg}px;
    border-radius: {l.radius_sm}px {l.radius_sm}px 0 0;
}}

notebook tab:checked {{
    background-color: {c.primary};
    color: white;
}}

statusbar {{
    background-color: {c.background_secondary};
    color: {c.text_secondary};
    border-top: 1px solid {c.border};
}}

menubar {{
    background-color: {c.background_secondary};
    color: {c.text};
    border-bottom: 1px solid {c.border};
}}

menu {{
    background-color: {c.background_secondary};
    color: {c.text};
    border: 1px solid {c.border};
}}

menuitem {{
    padding: {l.padding_sm}px {l.padding_lg}px;
}}

menuitem:hover {{
    background-color: {c.primary};
    color: white;
}}
"""
        return css

    def apply_to_widget(self, widget, backend: str = 'qt') -> None:
        """应用样式到组件"""
        css = self.get_css(backend)
        
        if backend == 'qt':
            widget.setStyleSheet(css)
        elif backend == 'web':
            # Web 样式通过 CSS 注入
            pass
        elif backend == 'gtk':
            widget.get_style_context().add_provider(
                Gtk.CssProvider(),
                Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
            )


# 全局样式管理器实例
_style_manager = None


def get_style_manager() -> StyleManager:
    """获取全局样式管理器实例"""
    global _style_manager
    if _style_manager is None:
        _style_manager = StyleManager()
    return _style_manager