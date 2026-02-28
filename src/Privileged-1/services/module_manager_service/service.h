/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef MODULE_MANAGER_SERVICE_SERVICE_H
#define MODULE_MANAGER_SERVICE_SERVICE_H

#include <common.h>
#include <module_types.h>

/* 最大模块路径长度 */
#define MAX_MODULE_PATH 256

/* 服务接口 */
hic_status_t module_manager_service_init(void);
hic_status_t module_manager_service_start(void);
hic_status_t module_manager_service_stop(void);
hic_status_t module_manager_service_cleanup(void);
hic_status_t module_manager_service_get_info(char* buffer, u32 size);

/* 模块管理接口 */
hic_status_t module_load(const char *path, int verify_signature);
hic_status_t module_unload(const char *name);
hic_status_t module_list(module_info_t *modules, int *count);
hic_status_t module_info(const char *name, module_info_t *info);
hic_status_t module_verify(const char *path);

/* 模块重启接口 */
hic_status_t module_restart(const char *name);
hic_status_t module_set_auto_restart(const char *name, u8 enable);

/* 滚动更新接口 */
hic_status_t module_rolling_update(const char *name, const char *new_path, int verify);
hic_status_t module_backup_state(const char *name);
hic_status_t module_restore_state(const char *name);

#endif /* MODULE_MANAGER_SERVICE_SERVICE_H */