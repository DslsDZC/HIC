# HIK构建系统 - Python虚拟环境

## 概述

HIK构建系统使用独立的Python虚拟环境来管理依赖，确保构建系统在不同系统上都能正常工作。

## 虚拟环境管理

### 自动创建

当你第一次运行以下任何命令时，虚拟环境会自动创建：
- `./build.sh gui`
- `./build.sh tui`
- `./build.sh cli`

### 手动管理

```bash
# 设置虚拟环境
./build.sh setup-venv

# 清理虚拟环境
./build.sh clean-venv
```

## 依赖管理

### Python依赖

所有Python依赖都在 `scripts/requirements.txt` 中定义：

```
pygobject>=3.42.0
```

### 系统依赖

在使用GUI界面之前，需要安装系统级的GTK3开发库：

**Arch Linux:**
```bash
sudo pacman -S gtk3 python-gobject
```

**Ubuntu/Debian:**
```bash
sudo apt-get install python3-gi gir1.2-gtk-3.0
```

**Fedora:**
```bash
sudo dnf install python3-gobject gtk3
```

## 虚拟环境位置

虚拟环境位于：`scripts/venv/`

## 故障排除

### 问题：GUI界面无法启动

**解决方案：**
1. 确保系统GTK3库已安装
2. 重新创建虚拟环境：
   ```bash
   ./build.sh clean-venv
   ./build.sh setup-venv
   ```

### 问题：依赖安装失败

**解决方案：**
1. 检查网络连接
2. 手动安装依赖：
   ```bash
   scripts/venv/bin/pip install -r scripts/requirements.txt
   ```

### 问题：Python版本不兼容

**解决方案：**
虚拟环境基于系统Python创建。确保系统Python版本>=3.8：
```bash
python3 --version
```

## 高级使用

### 直接使用虚拟环境Python

```bash
# 使用虚拟环境的Python
scripts/venv/bin/python scripts/build_system.py --config

# 使用虚拟环境的pip
scripts/venv/bin/pip list
```

### 添加新的Python依赖

1. 编辑 `scripts/requirements.txt`
2. 重新设置虚拟环境：
   ```bash
   ./build.sh clean-venv
   ./build.sh setup-venv
   ```

## 优势

使用虚拟环境的优势：

1. **独立性**: 不依赖系统Python或conda环境
2. **一致性**: 确保所有开发者使用相同的依赖版本
3. **可移植性**: 虚拟环境可以被复制和迁移
4. **隔离性**: 避免与系统包冲突
5. **易维护**: 依赖版本明确记录在requirements.txt中

## 文件结构

```
scripts/
├── venv/              # 虚拟环境（自动生成，已忽略）
├── requirements.txt   # Python依赖定义
├── build_gui.py       # GUI构建界面
├── build_tui.py       # TUI构建界面
├── build_system.py    # CLI构建界面
└── README.md          # 本文件
```