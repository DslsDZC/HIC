/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef IDE_DRIVER_SERVICE_H
#define IDE_DRIVER_SERVICE_H

#include <common.h>

/* IDE 驱动状态 */
typedef struct {
    int initialized;
    int drive_present;
    uint32_t total_sectors;
} ide_driver_state_t;

/* 服务接口 */
hic_status_t ide_driver_init(void);
hic_status_t ide_driver_start(void);
hic_status_t ide_driver_stop(void);
hic_status_t ide_driver_cleanup(void);

/* 磁盘操作接口 */
int ide_read_sector(uint32_t lba, void *buffer);
int ide_read_sectors(uint32_t lba, uint8_t count, void *buffer);

/* 获取驱动状态 */
ide_driver_state_t *ide_driver_get_state(void);

#endif /* IDE_DRIVER_SERVICE_H */
