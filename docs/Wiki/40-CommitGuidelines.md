<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 提交指南

## 概述

本文档提供了 HIC 项目的 Git 提交规范和最佳实践。

## 提交消息格式

### 结构

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type 类型

- **feat**: 新功能
- **fix**: Bug 修复
- **docs**: 文档更新
- **style**: 代码格式调整（不影响代码逻辑）
- **refactor**: 重构（既不是新功能也不是 Bug 修复）
- **perf**: 性能优化
- **test**: 添加或修改测试
- **chore**: 构建/工具相关变更
- **revert**: 回滚之前的提交

### Subject

- 使用祈使句："add" 而不是 "added"
- 首字母小写
- 不以句号结尾
- 限制在 50 个字符以内

### Body

- 解释"为什么"和"是什么"，而不是"怎么做"
- 每行限制在 72 个字符以内
- 使用空行分隔段落

### Footer

- 引用相关问题：`Closes #123`
- 破坏性变更：`BREAKING CHANGE:`

## 示例

### 新功能

```
feat(core): 添加快速路径优化

实现了系统调用的快速路径，通过缓存常用操作减少延迟。

性能提升：
- 系统调用延迟从 30ns 降低到 20ns
- 上下文切换延迟从 150ns 降低到 120ns

Closes #456
```

### Bug 修复

```
fix(capability): 修复能力派生时的权限泄漏

在能力派生时，子能力可能继承父能力的所有权限，
导致权限泄漏。修复了权限继承逻辑，确保子能力
只能继承明确的权限。

Fixes #789
```

### 文档更新

```
docs(wiki): 添加 ARM64 架构文档

添加了 ARM64 架构的详细文档，包括：
- 寄存器上下文
- 系统调用实现
- 中断处理
- 性能优化

Closes #321
```

### 重构

```
refactor(scheduler): 优化调度器数据结构

使用更高效的数据结构实现调度器，减少查找开销。

改进：
- 使用优先级队列替换链表
- 优化线程查找算法
- 减少 O(n) 操作
```

### 性能优化

```
perf(memory): 优化内存分配器

使用 slab 分配器优化小对象分配，减少碎片。

性能提升：
- 小对象分配速度提升 40%
- 内存碎片减少 25%
```

## 提交最佳实践

### 原子提交

```bash
# 每个提交只做一件事
git add file1.c
git commit -m "feat: 添加功能 A"

git add file2.c
git commit -m "fix: 修复问题 B"
```

### 避免合并提交

```bash
# 使用 rebase 而不是 merge
git pull --rebase

# 或者
git fetch origin
git rebase origin/main
```

### 清理历史

```bash
# 交互式 rebase
git rebase -i HEAD~5

# 压缩提交
squash

# 修改提交
edit
```

### 提交前检查

```bash
# 检查代码风格
./scripts/check_style.sh

# 运行测试
./scripts/test.sh

# 构建项目
./build.sh
```

## 常见错误

### 提交消息太长

```
# 错误
feat(core): 添加快速路径优化，通过缓存常用操作减少延迟，提升系统性能

# 正确
feat(core): 添加快速路径优化

通过缓存常用操作减少延迟，提升系统性能。
```

### 混合变更

```
# 错误
feat: 添加功能 A 并修复问题 B

# 正确
feat(core): 添加功能 A

fix(capability): 修复问题 B
```

### 缺少上下文

```
# 错误
fix: 修复 Bug

# 正确
fix(capability): 修复能力派生时的权限泄漏

子能力可能继承父能力的所有权限，导致权限泄漏。
```

## Git 配置

### 提交模板

```bash
# 创建提交模板
cat > ~/.gitmessage << 'EOF'
<type>(<scope>): <subject>

<body>

<footer>
EOF

# 配置 Git 使用模板
git config --global commit.template ~/.gitmessage
```

### 提交钩子

```bash
# 安装提交钩子
./scripts/install_hooks.sh

# 提交前自动检查
# - 代码风格
# - 测试通过
# - 构建成功
```

## 相关文档

- [贡献指南](./39-Contributing.md) - 贡献指南
- [代码审查流程](./41-ReviewProcess.md) - 代码审查流程
- [编码规范](./06-CodingStandards.md) - 编码规范

---

*最后更新: 2026-02-14*