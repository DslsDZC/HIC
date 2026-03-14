/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 初始服务启动器 - HIC 系统引导锚点
 * 
 * 设计理念：
 * - 极小化：只做一件事 - 启动模块管理器
 * - 静态链接到内核，解决"先有鸡还是先有蛋"问题
 * - 可动态卸载/重载（启动后可被替换）
 * - 代码量 < 300 行，可形式化验证
 * 
 * 启动流程：
 * Core-0 → init_launcher → module_manager → 其他服务
 */

#ifndef INIT_LAUNCHER_SERVICE_H
#define INIT_LAUNCHER_SERVICE_H

#include <stdint.h>
#include <stddef.h>

/* 状态码 */
#define INIT_LAUNCHER_SUCCESS      0
#define INIT_LAUNCHER_ERROR       -1
#define INIT_LAUNCHER_NOT_FOUND   -2
#define INIT_LAUNCHER_INVALID     -3
#define INIT_LAUNCHER_NO_MEMORY   -4

/* 模块管理器路径 */
#define MODULE_MANAGER_PATH       "/boot/module_manager.hicmod"

/* 初始化函数 */
int init_launcher_init(void);

/* 启动函数 - 加载并启动模块管理器 */
int init_launcher_start(void);

/* 停止函数 */
int init_launcher_stop(void);

/* 清理函数 */
int init_launcher_cleanup(void);

#endif /* INIT_LAUNCHER_SERVICE_H */
