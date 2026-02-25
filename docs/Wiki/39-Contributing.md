<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 贡献指南

## 概述

感谢您对 HIC 项目的兴趣！本文档将指导您如何为 HIC 做出贡献。

## 开始贡献

### 1. Fork 仓库

```bash
# Fork HIC 仓库到您的 GitHub 账户
# 然后克隆您的 fork
git clone https://github.com/your-username/HIC.git
cd HIC
```

### 2. 设置开发环境

```bash
# 安装依赖
./scripts/setup.sh

# 构建项目
./build.sh
```

### 3. 创建分支

```bash
# 创建功能分支
git checkout -b feature/your-feature-name

# 或者修复分支
git checkout -b fix/issue-number
```

## 代码贡献

### 编码规范

- 遵循 [编码规范](./06-CodingStandards.md)
- 使用一致的代码风格
- 添加必要的注释
- 编写测试用例

### 提交规范

```bash
# 提交格式
git commit -m "type: subject

<body>

<footer>"
```

**Type 类型**:
- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式调整
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建/工具相关

**示例**:
```
feat: 添加 ARM64 支持

实现 ARM64 架构支持，包括：
- ARM64 系统调用
- ARM64 中断处理
- ARM64 异常处理

Closes #123
```

## 文档贡献

### Wiki 文档

- 文档使用 Markdown 格式
- 遵循现有的文档结构
- 提供清晰的示例
- 保持文档更新

### 代码注释

```c
/**
 * 函数简述
 *
 * 详细描述
 *
 * @param param1 参数1描述
 * @param param2 参数2描述
 * @return 返回值描述
 */
int function(int param1, int param2);
```

## 测试贡献

### 单元测试

```c
// 测试文件命名: test_*.c
TEST(test_function) {
    // 准备
    int input = 42;
    
    // 执行
    int result = function(input);
    
    // 断言
    ASSERT_EQUAL(result, 84);
}
```

### 集成测试

```c
// 测试组件交互
TEST(test_ipc_communication) {
    domain_id_t d1 = create_domain();
    domain_id_t d2 = create_domain();
    
    // 测试通信
    hic_status_t status = ipc_call(d1, endpoint, ...);
    
    ASSERT_EQUAL(status, HIC_SUCCESS);
}
```

## Pull Request 流程

### 1. 推送到您的 fork

```bash
git push origin feature/your-feature-name
```

### 2. 创建 Pull Request

- 在 GitHub 上创建 PR
- 填写 PR 模板
- 关联相关问题
- 添加适当的标签

### 3. 代码审查

- 等待维护者审查
- 根据反馈修改代码
- 更新 PR

### 4. 合并

- 通过所有 CI 检查
- 获得至少一个批准
- 合并到主分支

## 社区行为准则

- 尊重他人
- 建设性反馈
- 关注问题而非个人
- 接受并给予优雅
- 对社区最有利

## 获取帮助

- 查看现有文档
- 搜索相关问题
- 在 GitHub Issues 中提问
- 参与社区讨论

## 相关文档

- [提交指南](./40-CommitGuidelines.md) - 提交指南
- [代码审查流程](./41-ReviewProcess.md) - 代码审查流程
- [编码规范](./06-CodingStandards.md) - 编码规范

---

*最后更新: 2026-02-14*