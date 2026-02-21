# HIC 构建系统指南

## 概述

HIC 构建系统是一个现代化的、可扩展的构建工具，支持多种用户界面（GUI、TUI、CLI），具备多语言支持、预设配置和自动降级功能。

## 主要特性

- **多界面支持**: Qt GUI、GTK GUI、TUI（文本界面）、交互式CLI（数字菜单选择）、CLI（命令行）
- **自动降级**: 智能检测并自动选择最佳可用界面
- **多语言支持**: 支持简体中文、英语、日语、德语
- **预设配置**: 提供多种预设配置（balanced、release、debug、minimal、performance）
- **YAML配置**: 通过YAML文件灵活配置所有构建选项
- **配置文件管理**: 支持导入/导出配置文件
- **主题切换**: 支持深色和浅色主题（Qt GUI）
- **依赖自动处理**: 自动检测并提示缺少的依赖

## 快速开始

### 基本使用

```bash
# 自动选择最佳界面（推荐）
make build

# 强制使用特定界面
make build-qt      # Qt GUI
make build-gtk     # GTK GUI
make build-tui     # TUI（文本界面）
make build-cli     # CLI（命令行）

# 使用预设配置
make build-balanced      # 平衡配置
make build-release       # 发布配置
make build-debug         # 调试配置
make build-minimal       # 最小配置
make build-performance   # 性能配置
```

### 传统构建方式

```bash
# 构建所有组件
make bootloader kernel

# 仅构建引导程序
make bootloader

# 仅构建内核
make kernel

# 清理构建文件
make clean

# 安装构建产物
make install
```

## 界面详解

### 1. Qt GUI（推荐）

Qt GUI 是一个现代化的图形用户界面，提供以下功能：

- 可视化配置编辑器
- 实时构建输出显示
- 配置预设快速切换
- 多语言支持
- 主题切换（深色/浅色）
- 配置文件导入/导出

**启动方式**:
```bash
make build-qt
# 或
python3 scripts/build_system.py --interface qt
```

**依赖安装**:
```bash
# Arch Linux
sudo pacman -S python-pyqt6

# Ubuntu/Debian
sudo apt-get install python3-pyqt6

# Fedora
sudo dnf install python3-pyqt6

# pip
pip install PyQt6
```

### 2. GTK GUI

GTK GUI 是一个跨平台的图形用户界面，提供类似Qt GUI的功能。

**启动方式**:
```bash
make build-gtk
# 或
python3 scripts/build_system.py --interface gtk
```

**依赖安装**:
```bash
# Arch Linux
sudo pacman -S gtk3 python-gobject

# Ubuntu/Debian
sudo apt-get install python3-gi gir1.2-gtk-3.0

# Fedora
sudo dnf install python3-gobject gtk3
```

### 3. TUI（文本界面）

TUI 是一个基于curses的文本用户界面，适合在终端中使用。

**启动方式**:
```bash
make build-tui
# 或
python3 scripts/build_system.py --interface tui
```

**依赖**: TUI 使用Python内置的curses库，无需额外安装。

**交互方式**: 使用上下箭头键导航，回车键选择。

### 4. 交互式CLI（Interactive CLI）

交互式CLI是一个基于数字菜单的命令行界面，适合在没有图形界面的情况下使用。

**启动方式**:
```bash
make build
# 或（自动检测）
python3 scripts/build_system.py --interface auto
# 或（强制使用交互式CLI）
python3 scripts/build_system.py --interface interactive
```

**功能**:
- 数字输入选择菜单选项
- 配置编译选项
- 配置运行时选项
- 查看当前配置
- 选择预设配置
- 开始构建
- 清理构建文件
- 帮助信息

**交互方式**: 输入数字（0-7）选择菜单选项，回车确认。

**菜单示例**:
```
╔══════════════════════════════════════════════════╗
║  HIC System v0.1.0 - 交互式构建系统  ║
╚══════════════════════════════════════════════════╝

请选择操作：

  1. 配置编译选项
  2. 配置运行时选项
  3. 查看当前配置
  4. 选择预设配置
  5. 开始构建
  6. 清理构建文件
  7. 帮助
  0. 退出

请输入选项 (0-7):
```

### 5. CLI（命令行）

CLI 是一个命令行界面，适合脚本和自动化。

**启动方式**:
```bash
make build-cli
# 或
python3 scripts/build_system.py --interface cli
```

**常用命令**:
```bash
# 构建所有组件
python3 scripts/build_system.py --target uefi bios kernel

# 清理构建文件
python3 scripts/build_system.py --clean

# 显示当前配置
python3 scripts/build_system.py --config

# 显示运行时配置说明
python3 scripts/build_system.py --config-runtime
```

## 配置系统

### YAML 配置文件

构建系统的配置存储在 `src/bootloader/platform.yaml` 文件中。

#### 构建系统配置

```yaml
# 构建系统配置
build_system:
  # 界面选择
  interface:
    primary: "auto"           # 首选界面: auto, qt, gtk, tui, interactive, cli
    fallback_chain:           # 降级链
      - "qt"                  # 首选: Qt GUI
      - "gtk"                 # 次选: GTK GUI
      - "tui"                 # 三选: TUI
      - "interactive"         # 四选: 交互式CLI（数字菜单）
      - "cli"                 # 保底: 纯CLI（命令行参数）
    auto_detect: true         # 自动检测可用界面
  
  # 语言设置
  localization:
    language: "zh_CN"         # 默认语言: zh_CN, en_US, ja_JP, de_DE
    auto_detect: true         # 自动检测系统语言
    fallback_language: "en_US"  # 回退语言
  
  # 预设配置
  presets:
    default: "balanced"       # 默认预设: balanced, release, debug, minimal, performance
```

#### 构建配置

```yaml
# 编译选项
build:
  mode: "dynamic"                  # dynamic=通用计算模式，static=嵌入式静态模式
  optimize_level: 2                # 优化级别: 0=无, 1=基础, 2=标准, 3=激进
  debug_symbols: true              # 是否包含调试符号
  lto: false                       # 链接时优化（影响调试）
  strip: false                     # 是否剥离符号表
```

#### 系统限制配置

```yaml
# 系统限制配置
system_limits:
  max_domains: 256               # 最大域数量
  max_capabilities: 2048         # 全局能力表大小
  max_threads: 256               # 最大线程数
  capabilities_per_domain: 128   # 每个域的最大能力数
  threads_per_domain: 16         # 每个域的最大线程数
  default_stack_size: 16384      # 默认栈大小
```

#### 功能配置

```yaml
# 编译时启用的内核功能
features:
  smp: true                       # 对称多处理器支持
  apic: true                      # 本地APIC/IOAPIC支持
  acpi: true                      # ACPI支持
  pci: true                       # PCI总线支持
  ahci: true                      # AHCI磁盘支持
  usb: true                       # USB支持
  virtio: true                    # VirtIO支持
  efi: true                       # UEFI支持
```

### 预设配置

构建系统提供了5种预设配置：

#### 1. Balanced（平衡）

平衡配置，适合日常开发。

```yaml
build:
  optimize_level: 2
  debug_symbols: true
  lto: false
  strip: false

debug:
  bounds_check: false
```

#### 2. Release（发布）

发布配置，优化性能和大小。

```yaml
build:
  optimize_level: 3
  debug_symbols: false
  lto: true
  strip: true

debug:
  bounds_check: false
```

#### 3. Debug（调试）

调试配置，包含完整调试信息。

```yaml
build:
  optimize_level: 0
  debug_symbols: true
  lto: false
  strip: false

debug:
  bounds_check: true
  panic_on_bug: true
```

#### 4. Minimal（最小）

最小配置，适合嵌入式系统。

```yaml
build:
  optimize_level: 2
  debug_symbols: false
  lto: false
  strip: true

features:
  smp: false
  acpi: false
```

#### 5. Performance（性能）

性能配置，启用所有优化。

```yaml
build:
  optimize_level: 3
  debug_symbols: false
  lto: true
  strip: true
```

## 自动降级机制

构建系统实现了智能的自动降级机制：

1. **首选界面**: 尝试使用配置中指定的首选界面
2. **降级链**: 如果首选界面不可用，按降级链依次尝试
3. **保底方案**: 最终降级到交互式CLI，确保构建系统始终可用

示例流程：

```
用户运行: make build
  ↓
检测到 PyQt6 可用
  ↓
启动 Qt GUI
  ↓
如果 PyQt6 不可用
  ↓
检测到 GTK3 可用
  ↓
启动 GTK GUI
  ↓
如果 GTK3 不可用
  ↓
检测到 curses 可用
  ↓
启动 TUI（文本界面，上下键导航）
  ↓
如果 curses 不可用
  ↓
使用交互式CLI（数字菜单选择）
  ↓
如果交互式CLI不可用
  ↓
使用纯CLI（命令行参数，保底方案）
```

**界面特点**:

- **Qt GUI**: 现代化图形界面，鼠标操作
- **GTK GUI**: 跨平台图形界面，鼠标操作
- **TUI**: 文本界面，上下箭头键导航
- **交互式CLI**: 数字菜单选择，适合命令行环境
- **纯CLI**: 命令行参数，适合脚本和自动化

## 多语言支持

构建系统支持以下语言：

- **简体中文** (zh_CN)
- **英语** (en_US)
- **日语** (ja_JP)
- **德语** (de_DE)

### 语言选择方式

1. **自动检测**: 根据系统语言自动选择
2. **YAML配置**: 在 `platform.yaml` 中指定默认语言
3. **命令行参数**: 使用 `--language` 参数指定
4. **GUI界面**: 在Qt/GTK GUI的菜单中选择

示例：

```bash
# 使用英语启动
python3 scripts/build_system.py --language en_US

# 使用日语启动
python3 scripts/build_system.py --language ja_JP
```

## 配置文件管理

### 导出配置

在Qt/GTK GUI中，可以通过菜单导出配置：

```bash
# 文件 → 导出配置
```

或使用命令行：

```bash
python3 scripts/build_system.py --interface qt --export-config
```

### 导入配置

在Qt/GTK GUI中，可以通过菜单导入配置：

```bash
# 文件 → 导入配置
```

或使用命令行：

```bash
python3 scripts/build_system.py --interface qt --import-config config.yaml
```

## 主题切换

Qt GUI 支持深色和浅色主题：

- **深色主题**: 适合夜间使用
- **浅色主题**: 适合日间使用

切换方式：

1. 在GUI菜单中选择：视图 → 深色主题/浅色主题
2. 在首选项对话框中设置

## 调试

### 查看构建日志

构建日志实时显示在所有界面的输出窗口中。

### 详细输出

```bash
# 使用Make的详细输出模式
make kernel V=1

# 或在构建系统中启用详细模式
python3 scripts/build_system.py --verbose
```

### GDB调试

```bash
# 使用GDB调试内核
make debug

# 快速启动测试
make test
```

## 故障排除

### 问题：Qt GUI无法启动

**解决方案**：
```bash
# 检查PyQt6是否安装
python3 -c "import PyQt6; print('PyQt6 installed')"

# 如果未安装，使用以下命令安装
pip install PyQt6

# 或使用系统包管理器
# Arch Linux
sudo pacman -S python-pyqt6

# Ubuntu/Debian
sudo apt-get install python3-pyqt6
```

### 问题：GTK GUI无法启动

**解决方案**：
```bash
# 检查GTK3是否安装
python3 -c "import gi; gi.require_version('Gtk', '3.0')"

# 如果未安装，使用以下命令安装
# Ubuntu/Debian
sudo apt-get install python3-gi gir1.2-gtk-3.0

# Arch Linux
sudo pacman -S gtk3 python-gobject
```

### 问题：TUI无法启动

**解决方案**：
```bash
# 检查终端是否支持curses
echo $TERM

# 如果未设置，设置TERM变量
export TERM=xterm-256color

# 在某些系统中，可能需要安装ncurses
# Ubuntu/Debian
sudo apt-get install libncurses5-dev

# Arch Linux
sudo pacman -S ncurses
```

### 问题：配置文件无效

**解决方案**：
```bash
# 验证YAML语法
python3 -c "import yaml; yaml.safe_load(open('src/bootloader/platform.yaml'))"

# 如果语法错误，检查YAML文件格式
# 确保使用正确的缩进（2个空格）
# 确保引号、冒号、破折号使用正确
```

## 高级用法

### 自定义降级链

在 `platform.yaml` 中自定义降级顺序：

```yaml
build_system:
  interface:
    primary: "gtk"           # 首选GTK
    fallback_chain:
      - "gtk"                # 先尝试GTK
      - "qt"                 # 再尝试Qt
      - "cli"                # 直接降级到CLI
```

### 创建自定义预设

在 `platform.yaml` 中添加自定义预设：

```yaml
build_system:
  presets:
    custom_preset:
      build:
        optimize_level: 2
        debug_symbols: true
        lto: false
      features:
        smp: true
        apic: true
```

### 批量构建

```bash
# 使用脚本批量构建
for preset in balanced release debug; do
    echo "Building with preset: $preset"
    make build-$preset
done
```

## 性能优化

### 并行构建

```bash
# 使用Make的并行构建
make -j$(nproc)

# 或在构建系统中指定并行数
python3 scripts/build_system.py --parallel 4
```

### 增量构建

构建系统自动支持增量构建，只重新编译修改过的文件。

### 缓存管理

```bash
# 清理构建缓存
make clean

# 重新构建
make build-balanced
```

## 最佳实践

1. **日常开发**: 使用 `make build-balanced`
2. **发布版本**: 使用 `make build-release`
3. **调试问题**: 使用 `make build-debug`
4. **嵌入式系统**: 使用 `make build-minimal`
5. **性能测试**: 使用 `make build-performance`

## 参考资料

- [HIC 架构文档](../Wiki/02-Architecture.md)
- [构建系统详解](../Wiki/04-BuildSystem.md)
- [开发环境配置](../Wiki/05-DevelopmentEnvironment.md)
- [代码规范](../Wiki/06-CodingStandards.md)

## 贡献

如果您想为构建系统做出贡献，请遵循以下步骤：

1. Fork 项目
2. 创建特性分支
3. 提交更改
4. 推送到分支
5. 创建 Pull Request

## 许可证

本项目采用 GPL-2.0 许可证。

## 联系方式

- 作者: DslsDZC
- 邮箱: dsls.dzc@gmail.com
- 项目主页: https://github.com/DslsDZC/HIC

---

**最后更新**: 2026-02-21