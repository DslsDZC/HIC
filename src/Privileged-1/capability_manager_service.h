/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 能力管理器服务 (Privileged-1) 头文件
 */

#ifndef HIC_PRIVILEGED_CAPABILITY_MANAGER_SERVICE_H
#define HIC_PRIVILEGED_CAPABILITY_MANAGER_SERVICE_H

#include "../Core-0/types.h"

/* 服务端点定义 */
#define CAP_ENDPOINT_VERIFY    0x1000
#define CAP_ENDPOINT_REVOKE    0x1001
#define CAP_ENDPOINT_DELEGATE  0x1002
#define CAP_ENDPOINT_TRANSFER  0x1003
#define CAP_ENDPOINT_DERIVE    0x1004

/* 服务生命周期函数 */
hic_status_t capability_manager_init(void);
hic_status_t capability_manager_start(void);
hic_status_t capability_manager_stop(void);
hic_status_t capability_manager_cleanup(void);

#endif /* HIC_PRIVILEGED_CAPABILITY_MANAGER_SERVICE_H */