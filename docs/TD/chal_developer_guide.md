<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# CHAL 兼容性硬件抽象层

本文档详细说明了HIC系统的CHAL (Compatibility Hardware Abstraction Layer) 兼容性硬件抽象层。

## 版本信息

- **文档版本**: 1.0.0
- **最后更新**: 2026-02-26
- **适用内核版本**: HIC Core-0 v1.0+

---

## 一、CHAL概述

### 1.1 什么是CHAL？

CHAL (Compatibility Hardware Abstraction Layer) 是HIC系统的兼容性硬件抽象层，建立在现有HAL之上的一个更高级抽象层。

**设计目标**：
- **简单**：减少参数，简化使用流程
- **直观**：使用直观的命名和参数
- **方便**：提供便捷的封装，减少样板代码
- **兼容**：保证API/ABI的向后兼容性

### 1.2 CHAL vs HAL

| 特性 | HAL (硬件抽象层) | CHAL (兼容性抽象层) |
|------|-----------------|-------------------|
| **定位** | 底层硬件抽象 | 高级兼容性封装 |
| **接口风格** | 完整、功能丰富 | 简化、便捷使用 |
| **参数数量** | 可能较多 | 通常不超过3个 |
| **命名风格** | 技术性、详细 | 直观、简洁 |
| **兼容性保证** | 架构隔离 | API/ABI兼容 |
| **使用场景** | 内核核心代码 | 应用和服务代码 |

### 1.3 CHAL的优势

✅ **更简单**：减少参数，减少错误
✅ **更直观**：命名清晰，易于理解
✅ **更方便**：提供便捷封装，减少样板代码
✅ **更安全**：内置错误检查和验证
✅ **更兼容**：保证API/ABI的向后兼容性

---

## 二、CHAL设计原则

### 2.1 简单原则

**定义**：函数参数不超过3个（特殊情况除外）

**实施**：
- 提供默认值
- 减少可选参数
- 使用结构体封装复杂参数

**示例**：
```c
/* HAL风格（可能需要更多参数） */
hal_context_switch(prev_context, next_context, flags, options);

/* CHAL风格（简化版） */
// chal_context_switch(prev, next);  // 使用默认值
```

### 2.2 直观原则

**定义**：函数名称清晰，参数名称直观

**实施**：
- 使用动词+名词的命名方式
- 参数名称清晰表达用途
- 避免缩写和技术术语

**示例**：
```c
/* 直观的命名 */
chal_read32(addr);        // 清晰：读取32位
chal_write32(addr, val);  // 清晰：写入32位
chal_bit_set(reg, bit);   // 清晰：设置位
```

### 2.3 方便原则

**定义**：提供便捷的封装，减少样板代码

**实施**：
- 提供常用操作的快捷函数
- 使用宏简化重复代码
- 提供RAII风格的封装

**示例**：
```c
/* 便捷的中断作用域 */
CHAL_IRQ_SCOPE();
// ... 临界区代码 ...
CHAL_IRQ_SCOPE_END();

/* 便捷的页操作 */
u64 page_num = chal_page_number(addr);
u64 page_addr = chal_page_address(page_num);
```

### 2.4 兼容原则

**定义**：保证API/ABI的向后兼容性

**实施**：
- 永不删除公开接口
- 永不修改接口签名
- 只添加新功能，不修改现有功能
- 提供版本兼容性检查

**示例**：
```c
/* 版本兼容性检查 */
if (chal_is_version_compatible(1, 0)) {
    // 使用v1.0+的功能
}

/* API兼容性检查 */
if (chal_check_api_compatibility(CHAL_VERSION_1_0)) {
    // 兼容的API
}
```

---

## 三、CHAL接口分类

### 3.1 内存操作

#### 基础读写

```c
u8 chal_read8(const volatile void* addr);
void chal_write8(volatile void* addr, u8 val);

u16 chal_read16(const volatile void* addr);
void chal_write16(volatile void* addr, u16 val);

u32 chal_read32(const volatile void* addr);
void chal_write32(volatile void* addr, u32 val);

u64 chal_read64(const volatile void* addr);
void chal_write64(volatile void* addr, u64 val);
```

#### 内存拷贝操作

```c
void chal_memset(void* dst, u8 c, u64 n);
void chal_memcpy(void* dst, const void* src, u64 n);
int chal_memcmp(const void* s1, const void* s2, u64 n);
```

#### 安全操作

```c
chal_error_t chal_safe_read32(const volatile void* addr, u32* out_val);
chal_error_t chal_safe_write32(volatile void* addr, u32 val);
```

**使用示例**：
```c
/* 读取设备寄存器 */
u32 status = chal_read32(&device->status_reg);

/* 写入配置 */
chal_write32(&device->config_reg, 0x12345678);

/* 安全读取 */
u32 value;
if (chal_ok(chal_safe_read32(addr, &value))) {
    // 使用value
}
```

### 3.2 IO操作

```c
u8 chal_io_read8(u16 port);
void chal_io_write8(u16 port, u8 val);

u16 chal_io_read16(u16 port);
void chal_io_write16(u16 port, u16 val);

u32 chal_io_read32(u16 port);
void chal_io_write32(u16 port, u32 val);
```

**使用示例**：
```c
/* 串口输出 */
chal_io_write8(COM1_PORT + 0, 'H');
chal_io_write8(COM1_PORT + 0, 'e');
chal_io_write8(COM1_PORT + 0, 'l');
chal_io_write8(COM1_PORT + 0, 'l');
chal_io_write8(COM1_PORT + 0, 'o');
```

### 3.3 中断控制

```c
bool chal_irq_disable(void);
void chal_irq_enable(void);
void chal_irq_restore(bool state);
```

**RAII风格封装**：
```c
#define CHAL_IRQ_SCOPE() \
    bool __irq_state = chal_irq_disable()

#define CHAL_IRQ_SCOPE_END() \
    chal_irq_restore(__irq_state)
```

**使用示例**：
```c
/* 传统方式 */
bool old_state = chal_irq_disable();
// ... 临界区代码 ...
chal_irq_restore(old_state);

/* RAII方式（推荐） */
CHAL_IRQ_SCOPE();
// ... 临界区代码 ...
CHAL_IRQ_SCOPE_END();
```

### 3.4 内存屏障

```c
void chal_barrier(void);   // 完整屏障
void chal_rbarrier(void);  // 读屏障
void chal_wbarrier(void);  // 写屏障
```

**使用示例**：
```c
/* 写入后确保完成 */
chal_write32(&device->cmd, CMD_START);
chal_wbarrier();  // 确保写入完成

/* 读取前确保一致性 */
chal_rbarrier();
u32 status = chal_read32(&device->status);
```

### 3.5 时间操作

```c
u64 chal_time_now(void);              // 获取时间戳（纳秒）
void chal_time_delay_us(u32 us);      // 延迟微秒
void chal_time_delay_ms(u32 ms);      // 延迟毫秒
void chal_time_delay_s(u32 s);        // 延迟秒
```

**使用示例**：
```c
/* 测量时间 */
u64 start = chal_time_now();
// ... 执行操作 ...
u64 elapsed = chal_time_now() - start;

/* 延迟 */
chal_time_delay_ms(100);  // 延迟100毫秒
```

### 3.6 页操作

```c
#define CHAL_PAGE_SIZE    4096
#define CHAL_PAGE_SHIFT   12
#define CHAL_PAGE_MASK    (CHAL_PAGE_SIZE - 1)

u64 chal_page_align(u64 addr);      // 页对齐
u64 chal_page_floor(u64 addr);      // 页下对齐
bool chal_page_is_aligned(u64 addr); // 检查对齐
u64 chal_page_number(u64 addr);     // 计算页号
u64 chal_page_address(u64 page_num); // 页号转地址
u64 chal_page_count(u64 size);      // 计算页数
```

**使用示例**：
```c
/* 对齐地址 */
u64 aligned_addr = chal_page_align(addr);

/* 计算页数 */
u64 num_pages = chal_page_count(size);

/* 计算页号 */
u64 page_num = chal_page_number(addr);
```

### 3.7 位操作

```c
void chal_bit_set(volatile u32* reg, u32 bit);
void chal_bit_clear(volatile u32* reg, u32 bit);
void chal_bit_toggle(volatile u32* reg, u32 bit);
bool chal_bit_test(const volatile u32* reg, u32 bit);

void chal_bits_set(volatile u32* reg, u32 mask);
void chal_bits_clear(volatile u32* reg, u32 mask);
bool chal_bits_test_all(const volatile u32* reg, u32 mask);
bool chal_bits_test_any(const volatile u32* reg, u32 mask);
```

**使用示例**：
```c
/* 设置位 */
chal_bit_set(&device->ctrl, 3);

/* 清除位 */
chal_bit_clear(&device->ctrl, 5);

/* 测试位 */
if (chal_bit_test(&device->status, 0)) {
    // 位0已设置
}

/* 测试多个位 */
if (chal_bits_test_all(&device->status, 0x03)) {
    // 位0和位1都已设置
}
```

### 3.8 原子操作

```c
u32 chal_atomic_read32(volatile u32* addr);
void chal_atomic_write32(volatile u32* addr, u32 val);
u32 chal_atomic_add32(volatile u32* addr, u32 val);
u32 chal_atomic_sub32(volatile u32* addr, u32 val);
u32 chal_atomic_set_bit32(volatile u32* addr, u32 bit);
u32 chal_atomic_clear_bit32(volatile u32* addr, u32 bit);
bool chal_atomic_cas32(volatile u32* addr, u32 expected, u32 desired);
```

**使用示例**：
```c
/* 原子计数器 */
u32 chal_atomic_add32(&counter, 1);

/* 原子位操作 */
chal_atomic_set_bit32(&flags, 3);

/* 原子比较交换 */
if (chal_atomic_cas32(&lock, 0, 1)) {
    // 成功获取锁
}
```

### 3.9 CPU操作

```c
void chal_cpu_halt(void);      // 停止CPU
void chal_cpu_idle(void);      // 空转等待
void chal_cpu_breakpoint(void); // 断点
void chal_cpu_fence(void);     // 指令屏障
```

**使用示例**：
```c
/* 空转等待 */
while (!condition) {
    chal_cpu_idle();
}

/* 停止CPU（严重错误） */
chal_cpu_halt();
```

### 3.10 等待操作

```c
chal_error_t chal_wait_until(bool (*condition)(void), u32 timeout_us);
chal_error_t chal_wait_for_bit_set(volatile u32* reg, u32 bit, u32 timeout_us);
chal_error_t chal_wait_for_bit_clear(volatile u32* reg, u32 bit, u32 timeout_us);
```

**使用示例**：
```c
/* 等待位设置 */
chal_error_t err = chal_wait_for_bit_set(&device->status, 0, 1000000);
if (chal_ok(err)) {
    // 成功
} else {
    // 超时
}
```

---

## 四、错误处理

### 4.1 错误代码

```c
typedef enum {
    CHAL_OK = 0,                     /* 成功 */
    CHAL_ERR_INVALID_PARAM = 1,      /* 无效参数 */
    CHAL_ERR_NOT_SUPPORTED = 2,      /* 不支持的操作 */
    CHAL_ERR_TIMEOUT = 3,            /* 超时 */
    CHAL_ERR_BUSY = 4,               /* 资源忙 */
    CHAL_ERR_NO_MEMORY = 5,          /* 内存不足 */
} chal_error_t;
```

### 4.2 错误检查工具

```c
const char* chal_strerror(chal_error_t err);  // 错误码转字符串

bool chal_ok(chal_error_t err);    // 检查成功
bool chal_fail(chal_error_t err);  // 检查失败
```

### 4.3 错误处理示例

```c
/* 基本错误处理 */
chal_error_t err = chal_safe_read32(addr, &value);
if (chal_fail(err)) {
    console_puts("Error: ");
    console_puts(chal_strerror(err));
    console_puts("\n");
    return;
}

/* 简化错误检查 */
CHAL_MUST_SUCCESS(chal_safe_write32(addr, value));

/* 使用断言 */
CHAL_ASSERT(condition);
```

---

## 五、版本兼容性

### 5.1 版本信息

```c
#define CHAL_VERSION_MAJOR  1
#define CHAL_VERSION_MINOR  0
#define CHAL_VERSION_PATCH  0
#define CHAL_VERSION_STRING "1.0.0"
```

### 5.2 版本检查

```c
u32 chal_get_version(void);
bool chal_is_version_compatible(u32 required_major, u32 required_minor);
u32 chal_get_api_version(void);
u32 chal_get_abi_version(void);
bool chal_check_api_compatibility(u32 required_version);
bool chal_check_abi_compatibility(u32 required_version);
```

### 5.3 兼容性规则

**API兼容性**：
- 主版本更高：可能兼容（向后兼容）
- 主版本相同，次版本更高：兼容
- 主版本更低：不兼容

**ABI兼容性**：
- 主版本必须完全相同
- 次版本和补丁版本可以不同

### 5.4 使用示例

```c
/* 检查版本兼容性 */
if (chal_is_version_compatible(1, 0)) {
    // 使用v1.0+的功能
}

/* 检查API兼容性 */
if (chal_check_api_compatibility(CHAL_VERSION_1_0)) {
    // API兼容
}

/* 检查ABI兼容性 */
if (chal_check_abi_compatibility(CHAL_VERSION_1_0)) {
    // ABI兼容
}
```

---

## 六、C++支持

CHAL提供了C++命名空间包装，使C++代码使用更加方便。

### 6.1 命名空间

```cpp
namespace chal {
    // 内存操作
    inline u8 read8(const volatile void* addr);
    inline void write8(volatile void* addr, u8 val);
    // ...
    
    // IO操作
    inline u8 io_read8(u16 port);
    inline void io_write8(u16 port, u8 val);
    // ...
    
    // 错误检查
    inline bool ok(chal_error_t err);
    inline bool fail(chal_error_t err);
}
```

### 6.2 使用示例

```cpp
#include "chal.h"

// 使用命名空间
using namespace chal;

// 读取寄存器
u32 status = read32(&device->status);

// 写入寄存器
write32(&device->config, 0x12345678);

// 错误检查
if (ok(err)) {
    // 成功
}
```

---

## 七、调试支持

### 7.1 调试宏

```c
#ifdef CHAL_DEBUG
#define CHAL_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            chal_cpu_breakpoint(); \
        } \
    } while(0)
#else
#define CHAL_ASSERT(condition) ((void)0)
#endif

#define CHAL_UNREACHABLE() \
    do { \
        CHAL_ASSERT(0); \
        chal_cpu_halt(); \
    } while(0)

#define CHAL_MUST_SUCCESS(expr) \
    do { \
        chal_error_t _err = (expr); \
        if (chal_fail(_err)) { \
            CHAL_ASSERT(0); \
            chal_cpu_halt(); \
        } \
    } while(0)
```

### 7.2 调试函数

```c
void chal_debug_print_reg(const char* name, volatile u32* reg);
void chal_debug_print_memory(void* base, u64 size);
```

**使用示例**：
```c
/* 打印寄存器 */
chal_debug_print_reg("STATUS", &device->status);

/* 打印内存 */
chal_debug_print_memory(buffer, 1024);

/* 断言 */
CHAL_ASSERT(condition);

/* 不可达代码 */
CHAL_UNREACHABLE();
```

---

## 八、最佳实践

### 8.1 使用建议

✅ **优先使用CHAL**：应用和服务代码优先使用CHAL，而非直接使用HAL

✅ **使用RAII风格**：使用`CHAL_IRQ_SCOPE()`等宏自动管理资源

✅ **检查错误**：使用`chal_ok()`和`chal_fail()`检查错误

✅ **版本检查**：使用版本兼容性检查确保API兼容

✅ **使用便捷函数**：使用`chal_wait_for_bit_set()`等便捷函数

### 8.2 避免的做法

❌ **不要混合使用**：不要同时使用CHAL和HAL

❌ **不要忽略错误**：不要忽略错误检查

❌ **不要硬编码值**：使用`CHAL_PAGE_SIZE`等常量

❌ **不要假设兼容性**：使用版本检查确保兼容

### 8.3 性能考虑

- CHAL是内联函数，性能与HAL相当
- 内存屏障和原子操作有性能开销，谨慎使用
- 等待操作可以使用轮询或中断，根据场景选择

---

## 九、迁移指南

### 9.1 从HAL迁移到CHAL

**旧代码（HAL）**：
```c
hal_inb(port);
hal_outb(port, value);
hal_memory_barrier();
bool state = hal_disable_interrupts();
// ... 临界区 ...
hal_restore_interrupts(state);
```

**新代码（CHAL）**：
```c
chal_io_read8(port);
chal_io_write8(port, value);
chal_barrier();
CHAL_IRQ_SCOPE();
// ... 临界区 ...
CHAL_IRQ_SCOPE_END();
```

### 9.2 迁移检查清单

- [ ] 替换`hal_in*`/`hal_out*`为`chal_io_read*`/`chal_io_write*`
- [ ] 替换内存屏障为`chal_barrier*`
- [ ] 使用`CHAL_IRQ_SCOPE()`替换手动中断管理
- [ ] 使用`chal_ok()`/`chal_fail()`检查错误
- [ ] 使用`CHAL_PAGE_SIZE`等常量替代硬编码值
- [ ] 添加版本兼容性检查

---

## 十、参考文档

- [HAL硬件抽象层](../Wiki/10-HardwareAbstractionLayer.md)
- [API接口列表](./api_interface_list.md)
- [接口分类与维护原则](./interface_classification.md)
- [API开发者指南](./api_developer_guide.md)

---

## 更新日志

### v1.0.0 (2026-02-26)
- 初始版本
- 完整的兼容性硬件抽象接口
- 简单、直观、方便的设计原则
- API/ABI兼容性保证
- C++命名空间支持
- 调试和性能统计支持