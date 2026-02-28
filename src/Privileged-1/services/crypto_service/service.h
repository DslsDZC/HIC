/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef CRYPTO_SERVICE_SERVICE_H
#define CRYPTO_SERVICE_SERVICE_H

#include <common.h>

/* 服务接口 */
hic_status_t crypto_service_init(void);
hic_status_t crypto_service_start(void);
hic_status_t crypto_service_stop(void);
hic_status_t crypto_service_cleanup(void);
hic_status_t crypto_service_get_info(char* buffer, u32 size);

#endif /* CRYPTO_SERVICE_SERVICE_H */
