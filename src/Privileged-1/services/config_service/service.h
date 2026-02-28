/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <common.h>

/* 服务接口 */
hic_status_t config_service_init(void);
hic_status_t config_service_start(void);
hic_status_t config_service_stop(void);
hic_status_t config_service_cleanup(void);
hic_status_t config_service_get_info(char* buffer, u32 size);

#endif /* CONFIG_SERVICE_SERVICE_H */
