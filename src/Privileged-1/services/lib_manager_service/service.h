/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef LIB_MANAGER_SERVICE_H
#define LIB_MANAGER_SERVICE_H

#include <common.h>

/* 服务接口 */
hic_status_t service_init(void);
hic_status_t service_start(void);
hic_status_t service_stop(void);
hic_status_t service_cleanup(void);
hic_status_t service_get_info(char* buffer, u32 size);

/* 服务注册 */
void service_register_self(void);

#endif /* LIB_MANAGER_SERVICE_H */