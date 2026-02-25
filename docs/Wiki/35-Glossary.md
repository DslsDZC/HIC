<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 术语表

## 概述

本文档定义了 HIC 系统中使用的关键术语和概念。

## 核心术语

### HIC (Hierarchical Isolation Core)
分层隔离内核。一个微内核操作系统内核，使用分层架构和数学形式验证来保证安全性。

### Core-0 (核心层)
HIC 的最底层特权层，负责系统核心功能：内存管理、能力系统、调度器、域切换等。

### Privileged-1 (特权层)
HIC 的中间特权层，提供设备驱动、文件系统、网络栈等特权服务。

### Application-3 (应用层)
HIC 的最高层用户空间，运行应用程序，通过系统调用和IPC与内核交互。

## 能力系统

### Capability (能力)
表示对系统资源的访问权限的令牌，具有不可伪造、可转移、可撤销等特性。

### Capability Derivation (能力派生)
从父能力创建子能力的过程，子能力继承父能力的权限子集。

### Capability Revocation (能力撤销)
撤销能力，使其失效并释放相关资源。

### Capability Conservation (能力守恒)
系统中的能力总数守恒，能力不能凭空产生或消失。

## 内存管理

### Physical Memory (物理内存)
计算机的实际内存，通过 PMM 管理物理页帧。

### Virtual Memory (虚拟内存)
进程看到的内存空间，通过页表映射到物理内存。

### Memory Isolation (内存隔离)
不同域的内存空间完全隔离，确保安全性。

### Page Frame (页帧)
物理内存分配的基本单位，大小为 4KB。

## 安全特性

### Formal Verification (形式验证)
使用数学方法证明系统正确性的过程。

### Invariant (不变式)
系统运行过程中始终保持成立的属性。

### Capability Monotonicity (能力单调性)
能力的权限只能减少，不能增加。

### Type Safety (类型安全)
所有操作都符合类型系统的规则。

### Deadlock Freedom (无死锁)
系统不会陷入死锁状态。

### Resource Quota Conservation (资源配额守恒)
资源配额总和守恒，不会超过限制。

## 性能指标

### System Call Latency (系统调用延迟)
执行系统调用所需的时间，目标 20-30ns。

### Interrupt Latency (中断延迟)
中断发生到处理程序开始执行的时间，目标 0.5-1μs。

### Context Switch Latency (上下文切换延迟)
切换线程上下文所需的时间，目标 120-150ns。

## 架构特性

### Domain (域)
HIC 的基本执行单元，具有独立的地址空间和资源配额。

### Domain Switch (域切换)
从一个域切换到另一个域的过程。

### Fast Path (快速路径)
优化后的常见操作路径，减少延迟。

### Rolling Update (滚动更新)
在线更新系统组件而不重启系统。

### Secure Boot (安全启动)
验证启动组件签名的安全机制。

## 其他术语

### IPC (Inter-Process Communication)
进程间通信，用于域间数据交换。

### TPM (Trusted Platform Module)
可信平台模块，用于安全启动和密钥管理。

### Ed25519
一种公钥签名算法，用于模块签名。

### UEFI (Unified Extensible Firmware Interface)
统一的可扩展固件接口。

### BIOS (Basic Input/Output System)
基本输入/输出系统。

---

*最后更新: 2026-02-14*