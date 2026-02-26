<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 测试指南

本文档介绍HIC项目的测试方法和测试策略。

## 测试概览

### 测试金字塔

```
        /\
       /  \      集成测试
      /____\     - 端到端测试
     /      \    - 模块间测试
    /________\   - 性能测试
   /          \  系统测试
  /            \ - 安全测试
 /______________\ - 兼容性测试
```

### 测试类型

| 测试类型 | 覆盖范围 | 执行频率 | 工具 |
|----------|----------|----------|------|
| 单元测试 | 函数/模块 | 每次提交 | 自定义测试框架 |
| 集成测试 | 模块间 | 每日构建 | 自定义测试框架 |
| 系统测试 | 完整系统 | 发布前 | QEMU |
| 性能测试 | 关键路径 | 每周 | LMbench, cyclictest |
| 安全测试 | 安全机制 | 每月 | 手动 + 自动化 |

## 单元测试

### 测试框架

HIC使用自定义的轻量级测试框架：

```c
/**
 * @file test_example.c
 * @brief 单元测试示例
 */

#include <test_framework.h>
#include "module_under_test.h"

/* 测试用例 */
TEST(test_case_name)
{
    /* 准备 */
    int input = 42;
    int expected = 84;

    /* 执行 */
    int result = multiply_by_two(input);

    /* 断言 */
    ASSERT_EQ(result, expected);
}

/* 测试套件 */
TEST_SUITE(suite_name)
{
    RUN_TEST(test_case_1);
    RUN_TEST(test_case_2);
    RUN_TEST(test_case_3);
}

/* 主函数 */
int main(void)
{
    RUN_SUITE(suite_name);
    return test_result();
}
```

### 运行单元测试

```bash
# 运行所有测试
make test

# 运行特定测试
make test TEST=capability

# 运行测试并生成覆盖率报告
make test COVERAGE=1
```

### 编写测试用例

```c
/**
 * @brief 测试能力创建
 */
TEST(capability_create)
{
    cap_id_t cap;

    /* 创建能力 */
    status = cap_create(
        HIC_DOMAIN_CORE,
        CAP_TYPE_MEMORY,
        CAP_PERM_READ | CAP_PERM_WRITE,
        0x100000,
        0x1000,
        &cap
    );

    /* 断言创建成功 */
    ASSERT_EQ(status, HIC_SUCCESS);
    ASSERT_NE(cap, HIC_INVALID_CAP_ID);

    /* 验证能力属性 */
    cap_entry_t entry;
    status = cap_get_info(cap, &entry);
    ASSERT_EQ(status, HIC_SUCCESS);
    ASSERT_EQ(entry.type, CAP_TYPE_MEMORY);
    ASSERT_EQ(entry.rights, CAP_PERM_READ | CAP_PERM_WRITE);
    ASSERT_EQ(entry.owner, HIC_DOMAIN_CORE);

    /* 清理 */
    cap_revoke(cap);
}

/**
 * @brief 测试能力传递
 */
TEST(capability_transfer)
{
    cap_id_t cap1, cap2;

    /* 创建能力 */
    cap_create(DOMAIN_A, CAP_TYPE_MEMORY, CAP_PERM_READ, 0x100000, 0x1000, &cap1);

    /* 传递能力 */
    status = cap_transfer(DOMAIN_A, DOMAIN_B, cap1, 0, &cap2);
    ASSERT_EQ(status, HIC_SUCCESS);
    ASSERT_NE(cap2, HIC_INVALID_CAP_ID);

    /* 验证传递 */
    cap_entry_t entry;
    cap_get_info(cap2, &entry);
    ASSERT_EQ(entry.owner, DOMAIN_B);

    /* 清理 */
    cap_revoke(cap1);
    cap_revoke(cap2);
}
```

## 集成测试

### 测试场景

#### 1. 引导加载程序测试

```bash
# 测试UEFI引导
make test-bootloader-uefi

# 测试BIOS引导
make test-bootloader-bios

# 测试签名验证
make test-bootloader-signature
```

#### 2. 内核初始化测试

```bash
# 测试内存初始化
make test-kernel-init-memory

# 测试能力系统初始化
make test-kernel-init-capability

# 测试调度器初始化
make test-kernel-init-scheduler
```

#### 3. 模块间通信测试

```bash
# 测试IPC通信
make test-ipc-communication

# 测试共享内存
make test-shared-memory

# 测试中断路由
make test-interrupt-routing
```

## 系统测试

### QEMU测试

#### UEFI引导测试

```bash
# 启动QEMU（UEFI模式）
qemu-system-x86_64 \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -drive format=raw,file=output/bootx64.efi \
  -drive format=raw,file=output/hic-kernel.bin \
  -m 512M \
  -serial stdio \
  -d int,cpu_reset
```

#### BIOS引导测试

```bash
# 启动QEMU（BIOS模式）
qemu-system-x86_64 \
  -drive format=raw,file=output/bios.bin \
  -drive format=raw,file=output/hic-kernel.bin \
  -m 512M \
  -serial stdio \
  -d int,cpu_reset
```

### 自动化测试脚本

```bash
#!/bin/bash
# test_system.sh

echo "=== HIC系统测试 ==="

# 1. 构建系统
echo "[1/5] 构建系统..."
make clean
make all
if [ $? -ne 0 ]; then
    echo "构建失败"
    exit 1
fi

# 2. 运行单元测试
echo "[2/5] 运行单元测试..."
make test
if [ $? -ne 0 ]; then
    echo "单元测试失败"
    exit 1
fi

# 3. 启动系统
echo "[3/5] 启动系统..."
timeout 30 qemu-system-x86_64 \
  -drive format=raw,file=output/bootx64.efi \
  -drive format=raw,file=output/hic-kernel.bin \
  -m 512M \
  -serial stdio \
  -nographic > test.log 2>&1

# 4. 检查输出
echo "[4/5] 检查输出..."
if grep -q "Hello, HIC!" test.log; then
    echo "系统启动成功"
else
    echo "系统启动失败"
    cat test.log
    exit 1
fi

# 5. 性能测试
echo "[5/5] 性能测试..."
make test-performance

echo "=== 测试完成 ==="
```

## 性能测试

### 系统调用延迟测试

```c
/**
 * @brief 测试系统调用延迟
 */
TEST(syscall_latency)
{
    u64 start, end, total = 0;
    int iterations = 1000;

    /* 预热 */
    for (int i = 0; i < 100; i++) {
        syscall(0, 0, 0);
    }

    /* 测量 */
    for (int i = 0; i < iterations; i++) {
        start = get_timestamp();
        syscall(0, 0, 0);
        end = get_timestamp();
        total += (end - start);
    }

    u64 avg_latency = total / iterations;

    /* 断言 */
    ASSERT_LT(avg_latency, 50);  /* 目标：20-30ns */
    printf("平均系统调用延迟: %lu ns\n", avg_latency);
}
```

### 中断处理延迟测试

```c
/**
 * @brief 测试中断处理延迟
 */
TEST(interrupt_latency)
{
    u64 start, end, total = 0;
    int iterations = 100;

    /* 触发中断并测量延迟 */
    for (int i = 0; i < iterations; i++) {
        start = get_timestamp();
        trigger_interrupt(32);  /* 触发定时器中断 */
        wait_for_interrupt();
        end = get_timestamp();
        total += (end - start);
    }

    u64 avg_latency = total / iterations;

    /* 断言 */
    ASSERT_LT(avg_latency, 2000);  /* 目标：0.5-1μs */
    printf("平均中断处理延迟: %lu ns\n", avg_latency);
}
```

### 线程切换延迟测试

```c
/**
 * @brief 测试线程切换延迟
 */
TEST(thread_switch_latency)
{
    u64 start, end, total = 0;
    int iterations = 1000;

    /* 创建测试线程 */
    thread_t *thread1 = create_test_thread(1);
    thread_t *thread2 = create_test_thread(2);

    /* 测量切换延迟 */
    for (int i = 0; i < iterations; i++) {
        start = get_timestamp();
        schedule_to(thread1);
        schedule_to(thread2);
        end = get_timestamp();
        total += (end - start);
    }

    u64 avg_latency = total / iterations;

    /* 断言 */
    ASSERT_LT(avg_latency, 200);  /* 目标：120-150ns */
    printf("平均线程切换延迟: %lu ns\n", avg_latency);
}
```

## 安全测试

### 能力系统测试

```c
/**
 * @brief 测试能力验证
 */
TEST(capability_verification)
{
    cap_id_t cap;

    /* 创建能力 */
    cap_create(DOMAIN_A, CAP_TYPE_MEMORY, CAP_PERM_READ, 0x100000, 0x1000, &cap);

    /* 测试：未授权访问 */
    status = memory_write(DOMAIN_B, cap, 0x100000, 0x42);
    ASSERT_EQ(status, HIC_ERROR_PERMISSION);

    /* 测试：权限不足 */
    status = memory_write(DOMAIN_A, cap, 0x100000, 0x42);
    ASSERT_EQ(status, HIC_ERROR_PERMISSION);

    /* 清理 */
    cap_revoke(cap);
}
```

### 边界检查测试

```c
/**
 * @brief 测试缓冲区边界
 */
TEST(buffer_boundary)
{
    char buffer[1024];

    /* 测试：正常访问 */
    ASSERT_EQ(buffer_write(buffer, 0, 0, 512), HIC_SUCCESS);

    /* 测试：越界访问 */
    ASSERT_EQ(buffer_write(buffer, 0, 512, 1024), HIC_ERROR_OUT_OF_RANGE);

    /* 测试：负偏移 */
    ASSERT_EQ(buffer_write(buffer, -1, 0, 100), HIC_ERROR_INVALID_PARAM);
}
```

### 拒绝服务测试

```c
/**
 * @brief 测试资源耗尽防护
 */
TEST(resource_exhaustion)
{
    domain_id_t domain = create_test_domain();

    /* 尝试分配超过配额的内存 */
    for (int i = 0; i < 10000; i++) {
        status = allocate_memory(domain, 0x1000);
        if (status == HIC_ERROR_QUOTA_EXCEEDED) {
            /* 预期的配额限制 */
            break;
        }
    }

    /* 验证配额限制生效 */
    ASSERT_EQ(status, HIC_ERROR_QUOTA_EXCEEDED);

    /* 清理 */
    destroy_domain(domain);
}
```

## 测试覆盖率

### 生成覆盖率报告

```bash
# 编译时启用覆盖率
make clean
make CFLAGS="--coverage" LDFLAGS="--coverage"

# 运行测试
make test

# 生成报告
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# 查看报告
firefox coverage_report/index.html
```

### 覆盖率目标

| 组件 | 语句覆盖率 | 分支覆盖率 | 函数覆盖率 |
|------|-----------|-----------|-----------|
| Core-0 | >90% | >85% | >95% |
| Privileged-1 | >85% | >80% | >90% |
| Bootloader | >80% | >75% | >85% |

## 持续集成

### GitHub Actions配置

```yaml
name: Test HIC

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y gcc-mingw-w64-x86-64 gnu-efi qemu-system-x86

      - name: Build
        run: |
          make clean
          make all

      - name: Unit tests
        run: make test

      - name: System tests
        run: ./scripts/test_system.sh

      - name: Upload coverage
        uses: codecov/codecov-action@v2
        with:
          files: ./coverage.info
```

## 故障注入测试

### 内存故障注入

```c
/**
 * @brief 测试内存分配失败处理
 */
TEST(memory_allocation_failure)
{
    /* 模拟内存耗尽 */
    inject_memory_failure();

    /* 尝试分配内存 */
    status = allocate_memory(DOMAIN_A, 0x1000);

    /* 验证错误处理 */
    ASSERT_EQ(status, HIC_ERROR_OUT_OF_MEMORY);

    /* 恢复正常 */
    restore_memory();
}
```

### 定时器故障注入

```c
/**
 * @brief 测试定时器故障处理
 */
TEST(timer_failure)
{
    /* 模拟定时器故障 */
    inject_timer_failure();

    /* 触发定时器中断 */
    trigger_interrupt(32);

    /* 验证故障处理 */
    ASSERT_EQ(get_error_count(), 1);

    /* 恢复正常 */
    restore_timer();
}
```

## 性能基准测试

### 使用LMbench

```bash
# 安装LMbench
git clone https://github.com/lemire/lmbench.git
cd lmbench
make
make results

# 运行基准测试
cd src
./lat_syscall -P 1
./lat_ctx -P 1
./bw_pipe -P 1
```

### 使用cyclictest

```bash
# 安装cyclictest
sudo apt install rt-tests

# 运行实时性能测试
sudo cyclictest -p 80 -n -i 10000 -l 10000
```

## 测试最佳实践

### 1. 测试独立性

```c
/* 好：每个测试独立 */
TEST(test_case_1)
{
    setup();
    /* 测试 */
    cleanup();
}

TEST(test_case_2)
{
    setup();
    /* 测试 */
    cleanup();
}

/* 不好：测试相互依赖 */
TEST(test_case_1)
{
    global_state = 1;
}

TEST(test_case_2)
{
    ASSERT_EQ(global_state, 1);  /* 依赖test_case_1 */
}
```

### 2. 清晰的断言消息

```c
/* 好：清晰的断言 */
ASSERT_EQ(result, expected, "Expected %d, got %d", expected, result);

/* 不好：不清晰的断言 */
ASSERT_EQ(result, expected);
```

### 3. 测试边界条件

```c
TEST(buffer_size)
{
    /* 测试最小值 */
    ASSERT_EQ(process_buffer(0), HIC_SUCCESS);

    /* 测试最大值 */
    ASSERT_EQ(process_buffer(MAX_SIZE), HIC_SUCCESS);

    /* 测试越界 */
    ASSERT_EQ(process_buffer(MAX_SIZE + 1), HIC_ERROR_INVALID_PARAM);
}
```

### 4. 使用测试固件（fixtures）

```c
/* 测试固件 */
static capability_t *setup_capability(void)
{
    capability_t *cap = malloc(sizeof(capability_t));
    cap->type = CAP_TYPE_MEMORY;
    cap->rights = CAP_PERM_READ;
    return cap;
}

static void teardown_capability(capability_t *cap)
{
    free(cap);
}

/* 使用测试固件 */
TEST(capability_test)
{
    capability_t *cap = setup_capability();
    /* 测试 */
    teardown_capability(cap);
}
```

## 测试报告

### 生成测试报告

```bash
# 生成HTML报告
make test-report

# 生成JSON报告
make test-report FORMAT=json

# 生成覆盖率报告
make test-coverage
```

### 报告内容

- 测试总数
- 通过/失败数量
- 失败详情
- 执行时间
- 覆盖率统计

## 参考资料

- [Linux内核测试指南](https://www.kernel.org/doc/html/latest/dev-tools/testing-overview.html)
- [Google测试博客](https://testing.googleblog.com/)
- [单元测试最佳实践](https://github.com/google/styleguide/blob/gh-pages/googletest.md)

---

*最后更新: 2026-02-14*