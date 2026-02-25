<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC内核三层目录结构说明

## 目录组织

HIC内核严格按照三层模型组织代码，每一层都有独立的目录：

```
kernel/
├── core0/              # Core-0层（内核核心和仲裁者）
├── privileged1/        # Privileged-1层（特权服务沙箱）
├── application3/       # Application-3层（应用层，目前为空）
├── arch/               # 架构相关代码（x86_64, arm64, riscv）
├── lib/                # 公共库（内存、字符串、控制台）
├── examples/           # 示例代码
└── include/            # 公共头文件（架构抽象）
```

## Core-0层 (core0/)

**职责**：
- 内核核心和仲裁者
- 能力系统管理
- 域管理
- 页表管理
- 物理内存管理
- 调度器
- 系统调用处理
- 中断路由
- 审计日志
- 监控服务
- 形式化验证
- 硬件探测
- 构建配置
- YAML解析
- PKCS#1验证

**文件列表**：
- audit.c/h - 审计日志系统
- boot_info.c/h - 启动信息处理
- build_config.c/h - 构建配置
- capability.c/h - 能力系统
- domain.c/h - 域管理
- domain_switch.c/h - 域切换
- exception.c/h - 异常处理
- formal_verification.c/h - 形式化验证
- formal_verification_ext.h - 扩展验证
- hardware_probe.c/h - 硬件探测
- irq.c/h - 中断处理
- kernel_start.c - 内核启动
- main.c - 主函数
- math_proofs.tex - 数学证明
- module_loader.c/h - 模块加载器
- module_signature.c - 模块签名
- monitor.c/h - 监控服务
- pagetable.c/h - 页表管理
- performance.c/h - 性能优化
- pkcs1.c/h - PKCS#1验证
- pmm.c/h - 物理内存管理
- scheduler.c - 调度器
- syscall.c/h - 系统调用
- thread.h - 线程管理
- tpm.h - TPM支持
- types.h - 类型定义
- yaml.c/h - YAML解析
- hal_cr3.h - CR3控制

**依赖规则**：
- 只能引用core0/目录下的头文件
- 可以引用lib/目录下的公共库
- 可以引用include/目录下的公共头文件
- 不能引用privileged1/或application3/的代码

## Privileged-1层 (privileged1/)

**职责**：
- 特权服务沙箱
- 服务管理
- 端点注册（API网关）
- 中断处理分发
- MMIO映射
- 服务间通信

**文件列表**：
- privileged_service.c/h - 服务管理

**依赖规则**：
- 可以引用core0/目录下的头文件（通过能力系统调用）
- 可以引用lib/目录下的公共库
- 不能引用application3/的代码
- 不能直接引用其他Privileged-1服务的代码

**调用关系**：
```
Privileged-1服务
    ↓ (通过系统调用)
Core-0能力系统
    ↓ (验证后)
调用其他Privileged-1服务或返回结果
```

## Application-3层 (application3/)

**职责**：
- 用户应用层
- 不可信用户代码

**文件列表**：
- （目前为空，待应用开发）

**依赖规则**：
- 只能通过系统调用访问Core-0
- 只能通过端点能力访问Privileged-1服务
- 不能直接访问硬件或内核内存

## 公共库 (lib/)

**职责**：
- 提供跨层使用的公共函数

**文件列表**：
- console.c/h - 控制台输出
- mem.c/h - 内存操作
- string.c/h - 字符串操作

**使用规则**：
- 所有层都可以使用
- 不包含任何层特定的逻辑

## 公共头文件 (include/)

**职责**：
- 架构抽象接口
- 硬件抽象层（HAL）

**文件列表**：
- arch.h - 架构相关定义
- hal.h - 硬件抽象层

**使用规则**：
- 所有层都可以使用
- 只包含架构无关的接口定义

## 架构相关代码 (arch/)

**职责**：
- 架构特定的实现

**目录结构**：
```
arch/
├── x86_64/        # x86_64架构
├── arm64/         # ARM64架构
└── riscv/         # RISC-V架构
```

**使用规则**：
- Core-0使用架构特定的实现
- 通过HAL接口访问

## 跨层调用规则

### 1. Application-3 → Core-0
```
应用进程
    ↓ syscall
Core-0系统调用处理
    ↓ 验证
执行操作或返回结果
```

### 2. Application-3 → Privileged-1
```
应用进程
    ↓ syscall (带端点能力)
Core-0验证端点能力
    ↓ 切换域
Privileged-1服务处理
    ↓ 返回结果
Core-0切换回应用进程
```

### 3. Privileged-1 → Core-0
```
Privileged-1服务
    ↓ syscall
Core-0处理请求
    ↓ 验证能力
执行操作或返回结果
```

### 4. Privileged-1 → Privileged-1
```
Privileged-1服务A
    ↓ ipc_call (带端点能力)
Core-0验证端点能力
    ↓ 切换域
Privileged-1服务B处理
    ↓ 返回结果
Core-0切换回服务A
```

## 编译规则

### Core-0编译
```makefile
$(BUILD_DIR)/$(CORE0_DIR)/%.o: $(CORE0_DIR)/%.c
    $(CC) $(CFLAGS) -I$(CORE0_DIR) -I$(ARCH_DIR) -I$(LIB_DIR) -c $< -o $@
```

### Privileged-1编译
```makefile
$(BUILD_DIR)/$(PRIVILEGED1_DIR)/%.o: $(PRIVILEGED1_DIR)/%.c
    $(CC) $(CFLAGS) -I$(PRIVILEGED1_DIR) -I$(CORE0_DIR) -I$(ARCH_DIR) -I$(LIB_DIR) -c $< -o $@
```

### Application-3编译
```makefile
$(BUILD_DIR)/$(APPLICATION3_DIR)/%.o: $(APPLICATION3_DIR)/%.c
    $(CC) $(CFLAGS) -I$(APPLICATION3_DIR) -I$(ARCH_DIR) -I$(LIB_DIR) -c $< -o $@
```

## 链接顺序

链接时按照以下顺序：
1. Core-0目标文件
2. Privileged-1目标文件
3. Application-3目标文件
4. 架构目标文件
5. 库目标文件

## 安全隔离

### 内存隔离
- Core-0、Privileged-1、Application-3使用不同的页表
- 通过MMU强制隔离
- 只有Core-0可以修改页表

### 能力隔离
- 所有跨层访问必须通过能力系统验证
- 能力由Core-0管理
- 能力可以传递但不能伪造

### 控制流隔离
- 跨层调用必须通过系统调用或IPC
- Core-0验证所有调用
- 防止直接跳转

## 开发指南

### 添加Core-0功能
1. 在core0/目录添加.c和.h文件
2. 在头文件中声明接口
3. 更新Makefile（如果需要）
4. 确保不依赖其他层的代码

### 添加Privileged-1服务
1. 在privileged1/目录添加.c和.h文件
2. 只引用core0/和lib/的头文件
3. 通过能力系统与Core-0交互
4. 不要直接访问硬件或内核内存

### 添加Application-3应用
1. 在application3/目录添加.c和.h文件
2. 只通过系统调用访问系统功能
3. 使用端点能力访问Privileged-1服务
4. 不要包含任何内核头文件

## 注意事项

1. **严格分层**：不要在代码中引用错误层的头文件
2. **能力系统**：所有跨层访问必须通过能力系统
3. **内存隔离**：不要试图直接访问其他层的内存
4. **接口清晰**：每层只暴露必要的接口
5. **最小权限**：只授予必要的权限

## 违规示例

### ❌ 错误：Privileged-1直接访问Core-0内存
```c
// privileged1/service.c
#include "../core0/pmm.h"  // 错误：不应该直接包含

// 尝试直接访问Core-0数据
extern core0_data_t g_core0_data;  // 错误：不应该访问
```

### ✅ 正确：Privileged-1通过系统调用访问Core-0
```c
// privileged1/service.c
#include "../core0/types.h"  // 正确：只包含类型定义

// 通过系统调用请求内存
hic_status_t status = syscall_alloc_memory(size, &addr);
```

### ❌ 错误：Core-0包含Privileged-1头文件
```c
// core0/module_loader.c
#include "../privileged1/privileged_service.h"  // 错误：不应该包含
```

### ✅ 正确：Core-0通过接口调用Privileged-1
```c
// core0/module_loader.c
// 通过函数指针或能力系统调用
status = service_init(instance_id, service_name);
```

## 总结

HIC内核的三层目录结构确保了：
1. **清晰的职责分离**：每层有明确的职责
2. **严格的安全隔离**：通过MMU和能力系统强制隔离
3. **易于维护**：代码组织清晰，便于理解和修改
4. **可扩展性**：可以独立开发和部署每一层

遵循这些规则可以确保系统的安全性和可维护性。