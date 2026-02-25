<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 代码规范

本文档定义了HIC项目的代码规范，确保代码质量和一致性。

## C语言规范

### 文件组织

#### 头文件 (.h)

```c
/**
 * @file filename.h
 * @brief 文件简短描述
 * @details 详细描述（可选）
 * @author 作者名
 * @date 创建日期
 */

#ifndef HIC_FILENAME_H
#define HIC_FILENAME_H

#include <types.h>

/* 宏定义 */
#define MAX_VALUE 100

/* 类型定义 */
typedef struct {
    int field1;
    int field2;
} my_type_t;

/* 函数声明 */
void function_name(param_type param1, param_type param2);

#endif /* HIC_FILENAME_H */
```

#### 源文件 (.c)

```c
/**
 * @file filename.c
 * @brief 文件简短描述
 */

#include "filename.h"
#include <stdlib.h>

/* 私有宏定义 */
#define PRIVATE_MACRO 42

/* 私有类型定义 */
typedef struct {
    int private_field;
} private_type_t;

/* 私有函数声明 */
static void private_function(param_type param);

/* 公共函数实现 */
void function_name(param_type param1, param_type param2)
{
    /* 函数体 */
}

/* 私有函数实现 */
static void private_function(param_type param)
{
    /* 函数体 */
}
```

### 命名规范

#### 常量

```c
/* 全局常量：全大写，下划线分隔 */
#define MAX_SIZE 1024
#define PI 3.14159265
```

#### 变量

```c
/* 局部变量：小写，下划线分隔 */
int local_variable;
char *string_ptr;

/* 全局变量：前缀g_ */
int g_global_counter;
```

#### 函数

```c
/* 公共函数：小写，下划线分隔 */
void function_name(int param1, int param2);

/* 私有函数：static前缀 */
static void private_function(int param);

/* 回调函数：后缀_cb */
int callback_function_cb(void *context);
```

#### 类型

```c
/* 结构体：_t后缀 */
typedef struct {
    int field;
} my_type_t;

/* 枚举：_e后缀 */
typedef enum {
    VALUE_1,
    VALUE_2,
    VALUE_3
} my_enum_e;
```

#### 汇编

```c
/* 汇编函数：_asm后缀 */
void interrupt_handler_asm(void);
```

### 格式规范

#### 缩进

```c
/* 使用4空格缩进，不使用制表符 */
if (condition) {
    statement1;
    statement2;
}
```

#### 大括号

```c
/* K&R风格 */
if (condition) {
    statement;
} else {
    statement;
}

/* 函数定义左大括号换行 */
void function_name(void)
{
    statement;
}
```

#### 空格

```c
/* 运算符前后加空格 */
result = a + b;
result = a * b - c;

/* 函数调用括号前不加空格 */
function_name(param1, param2);

/* 控制语句关键字后加空格 */
if (condition) {
    statement;
}

/* 指针声明：*靠近类型 */
char *ptr;
const char *const_ptr;
```

#### 换行

```c
/* 每行不超过80字符（特殊情况除外） */
very_long_function_name(parameter1, parameter2, parameter3,
                        parameter4, parameter5);

/* 参数过多时换行 */
result = function_with_many_parameters(
    param1,
    param2,
    param3,
    param4
);
```

### 注释规范

#### 文件注释

```c
/**
 * @file filename.h
 * @brief 文件简短描述
 * @details 详细描述，可以多行
 * @author 作者名 <email@example.com>
 * @date 2026-02-14
 * @version 1.0
 */
```

#### 函数注释

```c
/**
 * @brief 函数简短描述
 * @details 详细描述，包括功能、算法、注意事项等
 *
 * @param param1 参数1描述
 * @param param2 参数2描述
 * @return 返回值描述
 * @retval HIC_SUCCESS 成功
 * @retval HIC_ERROR 失败
 *
 * @note 注意事项
 * @warning 警告信息
 * @example
 * // 使用示例
 * status = function_name(value1, value2);
 */
int function_name(int param1, int param2);
```

#### 结构体注释

```c
/**
 * @brief 结构体简短描述
 * @details 详细描述
 */
typedef struct {
    int field1;  /**< 字段1描述 */
    int field2;  /**< 字段2描述 */
} my_type_t;
```

#### 行内注释

```c
/* 单行注释 */
int value = 42;  /* 行尾注释 */

/*
 * 多行注释
 * 第二行
 * 第三行
 */
```

### 代码组织

#### 头文件保护

```c
#ifndef HIC_FILENAME_H
#define HIC_FILENAME_H

/* 内容 */

#endif /* HIC_FILENAME_H */
```

#### 包含顺序

```c
/* 1. 系统头文件 */
#include <stdint.h>
#include <string.h>

/* 2. 项目公共头文件 */
#include "types.h"
#include "capability.h"

/* 3. 模块私有头文件 */
#include "private_header.h"
```

#### 函数组织

```c
/* 1. 宏定义 */
#define MACRO_NAME value

/* 2. 类型定义 */
typedef struct {
    int field;
} type_t;

/* 3. 全局变量 */
int g_global_var;

/* 4. 私有函数声明 */
static void private_func(void);

/* 5. 公共函数实现 */
void public_func(void)
{
    /* 实现 */
}

/* 6. 私有函数实现 */
static void private_func(void)
{
    /* 实现 */
}
```

## 汇编规范

### 文件格式

```assembly
/**
 * @file filename.S
 * @brief 文件简短描述
 */

#include "asm.h"

.section .text
.global function_name

/**
 * @brief 函数简短描述
 * @details 详细描述
 *
 * @param %rdi 参数1
 * @param %rsi 参数2
 * @return %rax 返回值
 */
function_name:
    /* 保存寄存器 */
    pushq %rbp
    movq %rsp, %rbp

    /* 函数体 */
    movl $42, %eax

    /* 恢复寄存器 */
    leave
    ret
```

### 命名规范

```assembly
/* 标签：小写，下划线分隔 */
function_name:
    /* 代码 */
    ret

/* 局部标签：.数字 */
function_name:
    jmp 1f
    jmp 2f
1:
    /* 代码 */
2:
    /* 代码 */
```

### 注释规范

```assembly
/* 单行注释 */
movl $42, %eax  /* 设置返回值 */

/* 多行注释 */
/*
 * 这里执行复杂的计算
 * 需要多个步骤
 */
```

## Python规范

### 文件格式

```python
"""
文件简短描述

详细描述，可以多行
"""

# 导入标准库
import sys
import os

# 导入第三方库
import yaml

# 导入项目模块
from config import settings


class ClassName:
    """类简短描述"""

    def __init__(self, param1, param2):
        """初始化方法

        Args:
            param1: 参数1描述
            param2: 参数2描述
        """
        self.param1 = param1
        self.param2 = param2

    def method_name(self, arg1, arg2):
        """方法简短描述

        Args:
            arg1: 参数1描述
            arg2: 参数2描述

        Returns:
            返回值描述
        """
        result = arg1 + arg2
        return result


def function_name(param1, param2):
    """函数简短描述

    Args:
        param1: 参数1描述
        param2: 参数2描述

    Returns:
        返回值描述
    """
    return param1 + param2
```

### 命名规范

```python
# 变量和函数：小写，下划线分隔
my_variable = 42
def my_function():
    pass

# 类：驼峰命名
class MyClass:
    pass

# 常量：全大写，下划线分隔
MAX_VALUE = 100

# 私有成员：前缀下划线
_private_variable = 42
```

## Makefile规范

### 格式规范

```makefile
# 变量定义
PROJECT = hic-kernel
VERSION = 0.1.0

# 目标
all: bootloader kernel

bootloader:
	@echo "Building bootloader..."
	$(MAKE) -C src/bootloader

kernel:
	@echo "Building kernel..."
	$(MAKE) -C build

clean:
	@echo "Cleaning..."
	rm -rf output/*

.PHONY: all bootloader kernel clean
```

### 命名规范

```makefile
# 变量：大写，下划线分隔
PROJECT_NAME = hic
BUILD_DIR = build

# 目标：小写，下划线分隔
all:
clean:
install:

# 伪目标
.PHONY: all clean install
```

## Git提交规范

### 提交消息格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type类型

- `feat`: 新功能
- `fix`: 修复bug
- `docs`: 文档更新
- `style`: 代码格式（不影响功能）
- `refactor`: 重构（不是新功能也不是修复）
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具相关

### 示例

```
feat(core): 添加能力系统实现

- 实现能力创建、传递、派生、撤销
- 添加能力验证机制
- 添加审计日志支持

Closes #123
```

```
fix(bootloader): 修复UEFI引导问题

修复了在特定UEFI固件上无法启动的问题。
原因是EFI_GUID定义不完整。

Fixes #456
```

## 代码审查检查清单

### 功能性

- [ ] 代码实现了预期功能
- [ ] 边界条件处理正确
- [ ] 错误处理完整
- [ ] 内存管理正确（无泄漏）
- [ ] 并发安全（如适用）

### 代码质量

- [ ] 命名清晰、一致
- [ ] 注释完整、准确
- [ ] 代码可读性好
- [ ] 无冗余代码
- [ ] 遵循项目规范

### 性能

- [ ] 算法复杂度合理
- [ ] 无明显性能瓶颈
- [ ] 资源使用高效

### 安全性

- [ ] 无缓冲区溢出风险
- [ ] 输入验证完整
- [ ] 权限检查正确
- [ ] 敏感数据处理安全

### 测试

- [ ] 有相应的单元测试
- [ ] 测试覆盖率足够
- [ ] 测试通过

## 最佳实践

### 1. 保持函数简短

```c
/* 好：函数单一职责 */
void print_message(const char *message)
{
    console_puts(message);
}

/* 不好：函数做太多事情 */
void do_everything(void)
{
    /* 太多代码 */
}
```

### 2. 避免全局变量

```c
/* 好：使用参数传递 */
void process_data(data_t *data)
{
    /* 处理数据 */
}

/* 不好：使用全局变量 */
data_t *g_data;
void process_data(void)
{
    /* 处理g_data */
}
```

### 3. 使用const修饰符

```c
/* 好：使用const */
void print_string(const char *str)
{
    /* str不会被修改 */
}

/* 不好：不使用const */
void print_string(char *str)
{
    /* str可能被修改 */
}
```

### 4. 错误处理

```c
/* 好：检查返回值 */
status = allocate_memory(&ptr);
if (status != HIC_SUCCESS) {
    return HIC_ERROR;
}

/* 不好：不检查返回值 */
allocate_memory(&ptr);
```

### 5. 资源清理

```c
/* 好：使用goto统一清理 */
status = resource1_init(&r1);
if (status != HIC_SUCCESS) {
    goto cleanup;
}

status = resource2_init(&r2);
if (status != HIC_SUCCESS) {
    goto cleanup1;
}

/* 正常处理 */
resource2_cleanup(&r2);
cleanup1:
    resource1_cleanup(&r1);
cleanup:
    return status;

/* 不好：分散的清理代码 */
status = resource1_init(&r1);
if (status != HIC_SUCCESS) {
    resource1_cleanup(&r1);
    return HIC_ERROR;
}

status = resource2_init(&r2);
if (status != HIC_SUCCESS) {
    resource2_cleanup(&r2);
    resource1_cleanup(&r1);
    return HIC_ERROR;
}
```

## 工具配置

### clang-format

创建 `.clang-format`：

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 80
BreakBeforeBraces: Linux
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: False
```

### .editorconfig

创建 `.editorconfig`：

```ini
root = true

[*.{c,h}]
indent_style = space
indent_size = 4
tab_width = 4
end_of_line = lf
charset = utf-8
trim_trailing_whitespace = true
insert_final_newline = true

[*.{S,asm}]
indent_style = tab
indent_size = 8
tab_width = 8
end_of_line = lf
charset = ascii
trim_trailing_whitespace = true
insert_final_newline = true

[*.py]
indent_style = space
indent_size = 4
tab_width = 4
end_of_line = lf
charset = utf-8
trim_trailing_whitespace = true
insert_final_newline = true

[Makefile]
indent_style = tab
indent_size = 4
tab_width = 4
end_of_line = lf
charset = ascii
trim_trailing_whitespace = true
```

## 参考资料

- [Linux内核编码风格](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- [Google C++风格指南](https://google.github.io/styleguide/cppguide.html)
- [MISRA C规范](https://www.misra.org.uk/Activities/MISRA-C)

---

*最后更新: 2026-02-14*