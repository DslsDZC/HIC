# HIC (Hierarchical Isolation Core) - 项目上下文

## 项目概述

HIC (Hierarchical Isolation Core，分级隔离内核) 是一个形式化验证的微内核，采用三级特权架构，具备数学证明的安全属性和运行时不变式检查。该项目旨在通过统一的架构范式，构建一个能适应从资源受限的嵌入式设备到功能复杂的通用计算系统的操作系统内核。

### 核心特性

- **极致性能** - 接近宏内核的性能，避免传统微内核的性能开销
- **强安全隔离** - 基于硬件MMU和软件能力系统的强隔离机制
- **动态可扩展性** - 支持运行时模块加载和热更新
- **形式化验证** - 核心代码具备数学证明，可形式化验证
- **双引导支持** - 同时支持BIOS和UEFI引导

### 架构设计

HIC采用三级特权架构：

| 层级 | 名称 | 特权级 | 角色 |
|------|------|--------|------|
| Core-0 | 内核核心与仲裁者 | Ring 0 | 系统的可信计算基（TCB） |
| Privileged-1 | 特权服务沙箱 | 逻辑Ring 1，物理Ring 0 | 模块化的系统功能提供者 |
| Application-3 | 应用层 | Ring 3 | 不可信用户代码 |

**关键设计原则**：
- 机制与策略的彻底分离：Core-0仅提供原子化的、无策略的执行原语，所有系统策略由Privileged-1服务提供
- 物理内存直接映射：Privileged-1服务采用物理内存直接映射方案，无虚拟地址转换开销
- 能力系统：所有授权通过不可伪造的能力对象实现，支持细粒度权限控制
- 强隔离：通过MMU和能力系统实现域间强隔离

### 部署模式

1. **静态合成模式** - 适用于嵌入式、实时或高安全场景，所有服务模块在构建时编译并链接为单一内核映像
2. **动态模块化模式** - 适用于通用计算、桌面或服务器场景，核心服务静态编译，扩展服务可在运行时安全安装、更新和卸载

## 项目结构

```
HIC/
├── src/                          # 源代码目录
│   ├── Core-0/                   # 内核核心层（Core-0）
│   │   ├── arch/                 # 架构相关代码
│   │   │   └── x86_64/           # x86_64架构实现
│   │   ├── include/              # Core-0头文件
│   │   ├── lib/                  # Core-0库代码
│   │   ├── audit.c/h             # 审计日志系统
│   │   ├── capability.c/h        # 能力系统
│   │   ├── domain.c/h            # 域管理
│   │   ├── exception.c/h         # 异常处理
│   │   ├── formal_verification.c/h # 形式化验证
│   │   ├── hal.c/h               # 硬件抽象层
│   │   ├── irq.c/h               # 中断管理
│   │   ├── kernel_start.c        # 内核启动入口
│   │   ├── pmm.c/h               # 物理内存管理
│   │   ├── scheduler.c/h         # 调度器
│   │   ├── syscall.c/h           # 系统调用
│   │   ├── thread.c/h            # 线程管理
│   │   └── ...                   # 其他核心组件
│   ├── Privileged-1/             # 特权服务层
│   ├── Application-3/            # 应用层
│   └── bootloader/               # 引导程序
│       ├── src/                  # 引导程序源码
│       ├── include/              # 引导程序头文件
│       ├── bin/                  # 编译输出
│       └── Makefile              # 引导程序构建配置
├── build/                        # 内核构建目录
│   ├── Makefile                  # 内核构建配置
│   ├── kernel.ld                 # 内核链接脚本
│   ├── bin/                      # 内核输出文件
│   └── obj/                      # 内核目标文件
├── docs/                         # 文档目录
│   ├── Wiki/                     # 技术文档
│   │   ├── 01-Overview.md        # 项目概述
│   │   ├── 02-Architecture.md    # 架构设计
│   │   ├── 03-QuickStart.md      # 快速开始
│   │   ├── 04-BuildSystem.md     # 构建系统
│   │   ├── 05-DevelopmentEnvironment.md # 开发环境
│   │   ├── 06-CodingStandards.md # 代码规范
│   │   ├── 08-Core0.md           # Core-0层详解
│   │   └── ...                   # 其他文档
│   └── TD/                       # 技术参考文档
│       ├── 三层模型.md           # 核心架构文档
│       ├── bios.md               # BIOS引导文档
│       ├── uefi.md               # UEFI引导文档
│       └── ...                   # 其他技术文档
├── scripts/                      # 构建和测试脚本
│   ├── build_system.py           # Python构建系统（支持GUI/TUI/CLI）
│   ├── build_gui.py              # GUI构建界面
│   ├── build_tui.py              # TUI构建界面
│   └── create_hic_image.py       # HIC镜像创建脚本
├── output/                       # 构建输出目录
│   ├── bootx64.efi               # UEFI引导程序
│   ├── bios.bin                  # BIOS引导程序
│   ├── hic-kernel.bin            # 内核映像
│   └── hic-installer.iso         # ISO安装镜像
├── iso_output/                   # ISO构建目录
├── Makefile                      # 根目录Makefile（快速构建）
├── Makefile.iso                  # ISO镜像创建
├── Makefile.debug                # 调试配置
├── build.sh                      # 构建脚本
├── build_config.mk               # 构建配置
└── README                        # 项目说明

```

## 构建和运行

### 前置要求

**必需工具**：
- GCC (x86_64-elf-gcc)
- Make
- Python 3
- NASM
- QEMU（用于测试）
- OpenSSL（用于签名）

**可选工具**：
- GCC MinGW-w64 (用于UEFI引导程序)
- GDB (用于调试)

### 快速构建

```bash
# 方式1：使用根目录Makefile（推荐）
make all && make install

# 方式2：使用构建脚本
./build.sh

# 方式3：使用Python构建系统（支持GUI/TUI/CLI）
python3 scripts/build_system.py

# 仅构建引导程序
make bootloader

# 仅构建内核
make kernel

# 创建ISO镜像
make -f Makefile.iso iso
```

### 输出文件

构建成功后，输出文件位于 `output/` 目录：

- `bootx64.efi` - UEFI引导程序（PE32+格式）
- `bios.bin` - BIOS引导程序
- `hic-kernel.bin` - 内核映像
- `hic-installer.iso` - ISO安装镜像

### 运行测试

```bash
# 在QEMU中运行（UEFI）
qemu-system-x86_64 \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -drive format=raw,file=output/bootx64.efi \
  -drive format=raw,file=output/hic-kernel.bin \
  -m 512M \
  -serial stdio

# 在QEMU中运行（BIOS）
qemu-system-x86_64 \
  -drive format=raw,file=output/bios.bin \
  -drive format=raw,file=output/hic-kernel.bin \
  -m 512M \
  -serial stdio

# 测试ISO镜像
make -f Makefile.iso test
```

### 调试

```bash
# 使用GDB调试
make debug

# 或手动调试
qemu-system-x86_64 \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -drive format=raw,file=output/bootx64.efi \
  -drive format=raw,file=output/hic-kernel.bin \
  -m 512M \
  -serial stdio \
  -s -S
# 另一个终端
gdb build/bin/hic-kernel.elf
(gdb) target remote :1234
(gdb) break kernel_start
(gdb) continue
```

## 开发规范

### C语言规范

- **命名规范**：
  - 常量：全大写，下划线分隔（如 `MAX_SIZE`）
  - 变量：小写，下划线分隔（如 `local_variable`）
  - 函数：小写，下划线分隔（如 `function_name`）
  - 类型：`_t` 后缀（如 `my_type_t`）
  - 枚举：`_e` 后缀（如 `my_enum_e`）
  - 汇编函数：`_asm` 后缀（如 `interrupt_handler_asm`）

- **格式规范**：
  - 使用4空格缩进，不使用制表符
  - K&R风格大括号
  - 每行不超过80字符（特殊情况除外）
  - 运算符前后加空格
  - 指针声明：`*` 靠近类型（如 `char *ptr`）

- **注释规范**：
  - 文件注释：`@file`, `@brief`, `@details`, `@author`, `@date`
  - 函数注释：`@brief`, `@details`, `@param`, `@return`, `@retval`, `@note`, `@warning`, `@example`
  - 结构体注释：每个字段使用 `/**< 描述 */``

### Git提交规范

提交消息格式：

```
<type>(<scope>): <subject>

<body>

<footer>
```

Type类型：
- `feat`: 新功能
- `fix`: 修复bug
- `docs`: 文档更新
- `style`: 代码格式（不影响功能）
- `refactor`: 重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具相关

示例：

```
feat(core): 添加能力系统实现

- 实现能力创建、传递、派生、撤销
- 添加能力验证机制
- 添加审计日志支持

Closes #123
```

## 关键技术要点

### 形式化验证要求

根据 `docs/TD/三层模型.md` 文档，Core-0层必须满足：

1. **代码规模限制** - 代码规模（不含架构特定汇编及自动生成数据）< 10,000行C代码
2. **内核大小限制** - 内核映像 < 2MB
3. **BSS段限制** - BSS段 < 512KB
4. **数学证明** - 核心不变式有数学证明（见 `src/Core-0/math_proofs.tex`）

### 安全特性

1. **能力系统**：
   - 能力是不可伪造的内核对象
   - 支持细粒度权限（读、写、执行）
   - 支持能力传递、派生、撤销
   - 所有资源访问必须通过能力验证

2. **强隔离机制**：
   - 每个Privileged-1服务运行在独立的物理内存区域
   - 通过MMU强制隔离
   - 故障隔离，单个服务崩溃不影响其他服务
   - 资源配额限制，防止DoS攻击

3. **审计日志**：
   - 记录所有关键安全操作
   - 仅追加审计日志缓冲区，防篡改
   - 持久化加密存储

4. **安全启动**：
   - RSA-3072 + SHA-384签名验证
   - 完整信任链
   - 抗回滚保护

### 性能目标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 系统调用延迟 | 20-30ns | 无特权级切换，无页表切换 |
| 中断处理延迟 | 0.5-1μs | 简化入口，静态路由表直接调用 |
| 线程切换延迟 | 120-150ns | 同特权级切换，无需切换页表 |
| 通信带宽 | 无额外开销 | 零拷贝共享内存通信 |

## 常见任务

### 添加新的系统调用

1. 在 `src/Core-0/syscall.c` 中添加系统调用号定义
2. 在 `g_syscall_table` 中注册处理函数
3. 实现处理函数，遵循能力系统验证
4. 添加审计日志记录
5. 更新文档

### 添加新的Privileged-1服务

1. 在 `src/Privileged-1/` 中创建服务目录
2. 实现服务代码，遵循服务框架
3. 定义服务端点和能力需求
4. 创建服务配置文件
5. 更新构建系统

### 修复引导程序问题

1. 检查 `src/bootloader/src/main.c` 中的内存布局
2. 验证HIC镜像头部解析逻辑
3. 确认入口点地址计算正确
4. 检查页表初始化
5. 使用QEMU调试输出定位问题

### 调试内核崩溃

1. 启用串口输出：在 `build_config.mk` 中设置 `CONFIG_SERIAL=1`
2. 使用QEMU启动并查看串口输出：`-serial stdio`
3. 查看审计日志：审计日志会记录异常信息
4. 使用GDB调试：`make debug`
5. 检查能力验证失败：审计日志会记录能力验证结果

## 重要注意事项

1. **不要简化代码** - 用户明确禁止简化代码，任何修改必须保持功能完整性
2. **不要创建测试脚本** - 除非用户明确要求，否则不要创建测试脚本
3. **不要使用root权限** - 构建和测试过程不应使用sudo或root权限
4. **严格遵守TD文档** - 所有修改必须遵循 `docs/TD/` 目录下的技术文档
5. **保持架构独立性** - 核心代码应尽量减少架构依赖，使用HAL抽象
6. **形式化验证优先** - 核心代码必须满足形式化验证要求
7. **审计日志很重要** - 修改代码时注意审计日志的完整性

## 故障排查

### 构建失败

- 检查交叉编译工具链是否安装
- 确认 `build_config.mk` 配置正确
- 查看详细输出：`make V=1`
- 清理后重新构建：`make clean && make`

### 引导失败

- 检查引导程序输出（串口）
- 验证HIC镜像格式正确
- 确认内存布局无冲突
- 检查页表初始化
- 查看审计日志

### 内核崩溃

- 查看异常处理日志
- 检查能力验证结果
- 验证内存访问权限
- 使用GDB调试
- 检查栈溢出

### 性能问题

- 使用性能监控工具
- 检查快速路径优化
- 验证缓存对齐
- 分析热点函数
- 优化关键路径

## 参考资料

### 核心文档

- [项目概述](docs/Wiki/01-Overview.md) - 了解HIC的设计哲学
- [架构设计](docs/Wiki/02-Architecture.md) - 深入理解三层模型
- [快速开始](docs/Wiki/03-QuickStart.md) - 开始构建和运行
- [构建系统](docs/Wiki/04-BuildSystem.md) - 详细的构建说明
- [开发环境](docs/Wiki/05-DevelopmentEnvironment.md) - 搭建开发环境
- [代码规范](docs/Wiki/06-CodingStandards.md) - 了解代码风格
- [Core-0层详解](docs/Wiki/08-Core0.md) - 内核核心实现
- [能力系统](docs/Wiki/11-CapabilitySystem.md) - 能力系统详解
- [形式化验证](docs/Wiki/15-FormalVerification.md) - 数学证明和安全保证

### 技术参考

- [三层模型](docs/TD/三层模型.md) - 核心架构参考文档
- [BIOS引导](docs/TD/bios.md) - BIOS引导实现
- [UEFI引导](docs/TD/uefi.md) - UEFI引导实现
- [引导加载程序](docs/TD/引导加载程序.md) - 引导程序详解

### 外部资源

- GitHub仓库：https://github.com/DslsDZC/HIC
- 许可证：GPL-2.0
- 联系方式：dsls.dzc@gmail.com

## 项目状态

- **当前版本**：0.1.0
- **最后更新**：2026-02-14
- **完成状态**：
  - ✅ 三层模型架构
  - ✅ 能力系统和审计日志
  - ✅ UEFI和BIOS双引导
  - ✅ 形式化验证框架
  - 🔄 性能优化和基准测试
  - 🔄 ARM64支持
  - 🔄 RISC-V支持
  - 🔄 Application-3层实现

## 开发者角色定位

根据你的角色，重点关注：

**新内核开发者**：
- 从 [快速开始](docs/Wiki/03-QuickStart.md) 开始
- 学习 [构建系统](docs/Wiki/04-BuildSystem.md)
- 理解 [Core-0层](docs/Wiki/08-Core0.md)
- 掌握 [能力系统](docs/Wiki/11-CapabilitySystem.md)

**学术研究者**：
- 研究 [三层模型](docs/TD/三层模型.md)
- 查看 [数学证明](src/Core-0/math_proofs.tex)
- 分析 [形式化验证](src/Core-0/formal_verification.c)
- 理解 [能力理论](docs/Wiki/11-CapabilitySystem.md)

**安全专家**：
- 深入 [安全机制](docs/Wiki/13-SecurityMechanisms.md)
- 分析 [能力系统](docs/Wiki/11-CapabilitySystem.md)
- 检查 [审计系统](src/Core-0/audit.c)
- 验证 [域隔离](src/Core-0/domain.c)

**系统管理员**：
- 掌握 [快速开始](docs/Wiki/03-QuickStart.md)
- 理解 [构建系统](docs/Wiki/04-BuildSystem.md)
- 学习 [故障排查](docs/Wiki/38-Troubleshooting.md)
- 配置 [引导加载](docs/TD/引导加载程序.md)

**特权服务开发者**：
- 学习 [服务指南](src/Core-0/examples/PRIVILEGED_SERVICE_GUIDE.md)
- 查看 [示例服务](src/Core-0/examples/example_service.c)
- 理解 [域切换](src/Core-0/domain_switch.c)
- 掌握 [IPC通信](src/Core-0/syscall.c)

**应用开发者**：
- 学习 [系统调用](src/Core-0/syscall.c)
- 理解 [能力系统](docs/Wiki/11-CapabilitySystem.md)
- 查看 [API参考](docs/Wiki/11-CapabilitySystem.md)
- 掌握 [开发指南](docs/Wiki/05-DevelopmentEnvironment.md)

---

**重要提醒**：
- 本项目对代码质量和安全性有极高要求
- 任何修改必须遵循形式化验证要求
- 严格遵守代码规范和Git提交规范
- 修改前务必阅读相关文档
- 测试是强制性的，不能跳过
- 如有疑问，请先查阅文档或询问项目负责人

**最后更新时间**：2026-02-16