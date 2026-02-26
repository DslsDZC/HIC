/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC服务API标准接口
 * 所有Privileged-1服务必须实现此接口
 */

#ifndef HIC_SERVICE_API_H
#define HIC_SERVICE_API_H

#include "../Core-0/types.h"

/* 服务状态 */
typedef enum {
    SERVICE_STATE_STOPPED = 0,
    SERVICE_STATE_INITIALIZING,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_PAUSED,
    SERVICE_STATE_STOPPING,
    SERVICE_STATE_ERROR
} service_state_t;

/* 服务API函数指针 */
typedef hic_status_t (*service_init_t)(void);
typedef hic_status_t (*service_start_t)(void);
typedef hic_status_t (*service_stop_t)(void);
typedef hic_status_t (*service_cleanup_t)(void);
typedef hic_status_t (*service_get_info_t)(char* buffer, u32 buffer_size);

/* 服务API结构 */
typedef struct service_api {
    service_init_t init;
    service_start_t start;
    service_stop_t stop;
    service_cleanup_t cleanup;
    service_get_info_t get_info;
} service_api_t;

/* 服务注册信息 */
typedef struct service_registration {
    char name[64];
    char version[16];
    u8 uuid[16];
    domain_id_t domain_id;
    service_state_t state;
    const service_api_t* api;
} service_registration_t;

/* 服务初始化宏 */
#define SERVICE_INIT() \
    __attribute__((constructor)) \
    static void service_constructor(void) { \
        service_register_self(); \
    }

/* 服务注册函数（由服务实现） */
extern void service_register_self(void);

/* 服务管理API（由Core-0提供） */
hic_status_t service_register(const char* name, const service_api_t* api);
hic_status_t service_unregister(const char* name);
hic_status_t service_get(const char* name, service_registration_t** out);

#endif /* HIC_SERVICE_API_H */