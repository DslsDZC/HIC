<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC Wiki - 完整文档中心

欢迎来到HIC项目的完整文档中心！

## 🚀 快速开始

如果你是第一次接触HIC，建议按以下顺序阅读：

1. **[项目概述](./01-Overview.md)** - 了解HIC是什么，设计哲学和目标
2. **[快速开始](./03-QuickStart.md)** - 如何快速构建和运行HIC
3. **[架构设计](./02-Architecture.md)** - 深入理解三层模型架构

## 📚 文档分类

### 核心文档
- [项目概述](./01-Overview.md) - HIC是什么，设计哲学和目标
- [架构设计](./02-Architecture.md) - 三层模型架构详解
- [快速开始](./03-QuickStart.md) - 如何快速构建和运行HIC

### 开发指南
- [构建系统](./04-BuildSystem.md) - 详细的构建说明和配置
- [开发环境](./05-DevelopmentEnvironment.md) - 开发环境搭建指南
- [代码规范](./06-CodingStandards.md) - 代码风格和规范
- [测试指南](./07-Testing.md) - 如何运行测试

### 架构详解
- [Core-0层](./08-Core0.md) - 内核核心与仲裁者
- [Privileged-1层](./09-Privileged1.md) - 特权服务沙箱
- [Application-3层](./10-Application3.md) - 应用层
- [能力系统](./11-CapabilitySystem.md) - 能力系统详解
- [物理内存管理](./12-PhysicalMemory.md) - 物理内存直接映射

### 安全机制
- [安全架构](./13-SecurityArchitecture.md) - 整体安全架构
- [审计日志](./14-AuditLogging.md) - 审计日志系统
- [形式化验证](./15-FormalVerification.md) - 形式化验证和数学证明
- [安全启动](./16-SecureBoot.md) - 安全启动机制

### 性能优化
- [性能指标](./17-PerformanceMetrics.md) - 性能目标和指标
- [优化技术](./18-OptimizationTechniques.md) - 性能优化技术
- [快速路径](./19-FastPath.md) - 快速路径优化

### 模块化系统
- [模块格式](./20-ModuleFormat.md) - .hicmod模块格式
- [模块管理器](./21-ModuleManager.md) - 模块生命周期管理
- [滚动更新](./22-RollingUpdate.md) - 滚动更新机制
- [API版本管理](./23-APIVersioning.md) - API版本管理

### 多架构支持
- [x86_64架构](./24-x86_64.md) - x86-64架构支持
- [ARM64架构](./25-ARM64.md) - ARM64架构支持
- [RISC-V架构](./26-RISC-V.md) - RISC-V架构支持
- [无MMU架构](./27-NoMMU.md) - 无MMU架构简化设计

### 引导系统
- [引导加载程序](./28-Bootloader.md) - Bootloader架构和实现
- [UEFI引导](./29-UEFI.md) - UEFI引导流程
- [BIOS引导](./30-BIOS.md) - BIOS引导流程

### 专题
- [资源管理](./31-ResourceManagement.md) - 资源管理和配额
- [通信机制](./32-Communication.md) - IPC和通信机制
- [异常处理](./33-ExceptionHandling.md) - 异常处理和恢复
- [监控服务](./34-MonitorService.md) - 监控服务

### 参考资料
- [技术术语表](./35-Glossary.md) - 技术术语解释
- [常见问题](./36-FAQ.md) - 常见问题和解答
- [最佳实践](./37-BestPractices.md) - 最佳实践建议
- [故障排查](./38-Troubleshooting.md) - 故障排查指南

### 贡献指南
- [如何贡献](./39-Contributing.md) - 如何为项目做贡献
- [提交指南](./40-CommitGuidelines.md) - 提交消息规范
- [审查流程](./41-ReviewProcess.md) - 代码审查流程

## 🎯 按角色查看文档

### 用户
- [快速开始](./03-QuickStart.md) - 快速上手
- [项目概述](./01-Overview.md) - 了解项目
- [常见问题](./36-FAQ.md) - 解决问题

### 开发者
- [开发环境](./05-DevelopmentEnvironment.md) - 搭建环境
- [构建系统](./04-BuildSystem.md) - 构建项目
- [代码规范](./06-CodingStandards.md) - 遵循规范
- [测试指南](./07-Testing.md) - 运行测试

### 架构师
- [架构设计](./02-Architecture.md) - 理解架构
- [Core-0层](./08-Core0.md) - 核心层设计
- [Privileged-1层](./09-Privileged1.md) - 服务层设计
- [能力系统](./11-CapabilitySystem.md) - 能力系统设计

### 安全专家
- [安全架构](./13-SecurityArchitecture.md) - 安全架构
- [审计日志](./14-AuditLogging.md) - 审计机制
- [形式化验证](./15-FormalVerification.md) - 形式化验证
- [安全启动](./16-SecureBoot.md) - 安全启动

### 性能工程师
- [性能指标](./17-PerformanceMetrics.md) - 性能目标
- [优化技术](./18-OptimizationTechniques.md) - 优化方法
- [快速路径](./19-FastPath.md) - 快速路径

### 贡献者
- [如何贡献](./39-Contributing.md) - 贡献指南
- [提交指南](./40-CommitGuidelines.md) - 提交规范
- [审查流程](./41-ReviewProcess.md) - 审查流程

## 🔍 搜索文档

### 按关键词搜索

| 关键词 | 相关文档 |
|--------|----------|
| 构建 | [快速开始](./03-QuickStart.md), [构建系统](./04-BuildSystem.md) |
| 架构 | [架构设计](./02-Architecture.md), [Core-0层](./08-Core0.md) |
| 安全 | [安全架构](./13-SecurityArchitecture.md), [审计日志](./14-AuditLogging.md) |
| 性能 | [性能指标](./17-PerformanceMetrics.md), [优化技术](./18-OptimizationTechniques.md) |
| 模块 | [模块格式](./20-ModuleFormat.md), [模块管理器](./21-ModuleManager.md) |
| 引导 | [引导加载程序](./28-Bootloader.md), [UEFI引导](./29-UEFI.md) |

### 按问题搜索

**我想...**
- ...了解HIC：[项目概述](./01-Overview.md)
- ...快速开始：[快速开始](./03-QuickStart.md)
- ...构建项目：[构建系统](./04-BuildSystem.md)
- ...开发功能：[开发环境](./05-DevelopmentEnvironment.md)
- ...理解架构：[架构设计](./02-Architecture.md)
- ...学习安全：[安全架构](./13-SecurityArchitecture.md)
- ...优化性能：[性能指标](./17-PerformanceMetrics.md)
- ...贡献代码：[如何贡献](./39-Contributing.md)
- ...解决问题：[常见问题](./36-FAQ.md), [故障排查](./38-Troubleshooting.md)

## 📊 文档状态

| 文档类别 | 完成度 | 说明 |
|----------|--------|------|
| 核心文档 | ✅ 100% | 所有核心文档已完成 |
| 开发指南 | ✅ 100% | 所有开发指南已完成 |
| 架构详解 | ✅ 100% | 所有架构文档已完成 |
| 安全机制 | ✅ 100% | 所有安全文档已完成 |
| 性能优化 | ✅ 100% | 所有性能文档已完成 |
| 模块化系统 | ✅ 100% | 所有模块文档已完成 |
| 多架构支持 | 🔄 80% | x86_64完成，ARM64/RISC-V进行中 |
| 引导系统 | ✅ 100% | 所有引导文档已完成 |
| 专题 | ✅ 100% | 所有专题文档已完成 |
| 参考资料 | ✅ 100% | 所有参考文档已完成 |
| 贡献指南 | ✅ 100% | 所有贡献文档已完成 |

## 🔗 相关链接

- **项目主页**: [GitHub仓库](https://github.com/*/HIC)
- **问题追踪**: [Issues](https://github.com/*/HIC/issues)
- **讨论区**: [Discussions](https://github.com/*/HIC/discussions)
- **技术文档**: [TD目录](../TD/)
- **项目文档**: [README](../README.md)

## 📝 文档贡献

欢迎改进文档！如果你发现错误或有改进建议，请：

1. 提交Issue描述问题或建议
2. Fork项目并创建改进分支
3. 提交Pull Request
4. 等待审查和合并

详见 [如何贡献](./39-Contributing.md)。

## 📧 联系方式

- **Email**: *@gmail.com
- **GitHub**: https://github.com/*/HIC
- **Discussions**: https://github.com/*/HIC/discussions

## 📄 许可证

HIC文档遵循GPL-2.0许可证。详见 [LICENSE](../../LICENSE)。

---

*最后更新: 2026-02-14*