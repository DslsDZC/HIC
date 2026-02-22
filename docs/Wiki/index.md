<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC 项目 Wiki

欢迎使用 HIC (Hierarchical Isolation Core) 项目文档中心。

## 快速导航

### 📚 核心文档
- [项目概述](./01-Overview.md) - HIC是什么，设计哲学和目标
- [架构设计](./02-Architecture.md) - 三层模型架构详解
- [快速开始](./03-QuickStart.md) - 如何快速构建和运行HIC

### 🔧 开发指南
- [构建系统](./04-BuildSystem.md) - 详细的构建说明和配置
- [开发环境](./05-DevelopmentEnvironment.md) - 开发环境搭建指南
- [代码规范](./06-CodingStandards.md) - 代码风格和规范
- [测试指南](./07-Testing.md) - 如何运行测试

### 🏗️ 架构详解
- [Core-0层](./08-Core0.md) - 内核核心与仲裁者
- [Privileged-1层](./09-Privileged1.md) - 特权服务沙箱
- [Application-3层](./10-Application3.md) - 应用层
- [能力系统](./11-CapabilitySystem.md) - 能力系统详解
- [物理内存管理](./12-PhysicalMemory.md) - 物理内存直接映射

### 🔐 安全机制
- [安全架构](./13-SecurityArchitecture.md) - 整体安全架构
- [审计日志](./14-AuditLogging.md) - 审计日志系统
- [形式化验证](./15-FormalVerification.md) - 形式化验证和数学证明
- [安全启动](./16-SecureBoot.md) - 安全启动机制

### 🚀 性能优化
- [性能指标](./17-PerformanceMetrics.md) - 性能目标和指标
- [优化技术](./18-OptimizationTechniques.md) - 性能优化技术
- [快速路径](./19-FastPath.md) - 快速路径优化

### 🔄 模块化系统
- [模块格式](./20-ModuleFormat.md) - .hicmod模块格式
- [模块管理器](./21-ModuleManager.md) - 模块生命周期管理
- [滚动更新](./22-RollingUpdate.md) - 滚动更新机制
- [API版本管理](./23-APIVersioning.md) - API版本管理

### 🌐 多架构支持
- [x86_64架构](./24-x86_64.md) - x86-64架构支持
- [ARM64架构](./25-ARM64.md) - ARM64架构支持
- [RISC-V架构](./26-RISC-V.md) - RISC-V架构支持
- [无MMU架构](./27-NoMMU.md) - 无MMU架构简化设计

### 📦 引导系统
- [引导加载程序](./28-Bootloader.md) - Bootloader架构和实现
- [UEFI引导](./29-UEFI.md) - UEFI引导流程
- [BIOS引导](./30-BIOS.md) - BIOS引导流程

### 🎯 专题
- [资源管理](./31-ResourceManagement.md) - 资源管理和配额
- [通信机制](./32-Communication.md) - IPC和通信机制
- [异常处理](./33-ExceptionHandling.md) - 异常处理和恢复
- [监控服务](./34-MonitorService.md) - 监控服务

### 📖 参考资料
- [技术术语表](./35-Glossary.md) - 技术术语解释
- [常见问题](./36-FAQ.md) - 常见问题和解答
- [最佳实践](./37-BestPractices.md) - 最佳实践建议
- [故障排查](./38-Troubleshooting.md) - 故障排查指南

### 🤝 贡献指南
- [如何贡献](./39-Contributing.md) - 如何为项目做贡献
- [提交指南](./40-CommitGuidelines.md) - 提交消息规范
- [审查流程](./41-ReviewProcess.md) - 代码审查流程

## 项目状态

| 组件 | 状态 | 说明 |
|------|------|------|
| Core-0 | ✅ 完成 | 核心功能已实现 |
| Privileged-1 | ✅ 完成 | 服务框架已实现 |
| Application-3 | ⏳ 待开发 | 应用层待开发 |
| Bootloader | ✅ 完成 | UEFI和BIOS双引导 |
| 文档 | ✅ 完成 | 完整的技术文档 |
| 测试 | ⏳ 进行中 | 单元测试和集成测试 |

## 版本历史

- **v0.1.0** (2026-02-14) - 初始版本
  - 完成三层模型架构
  - 实现能力系统和审计日志
  - 完成UEFI和BIOS双引导
  - 完整的形式化验证

## 相关链接

- [GitHub仓库](https://github.com/*/HIC)
- [Issue追踪](https://github.com/*/HIC/issues)
- [讨论区](https://github.com/*/HIC/discussions)

## 许可证

[GPL-2.0 License](../../LICENSE)

---

*最后更新: 2026-02-14*