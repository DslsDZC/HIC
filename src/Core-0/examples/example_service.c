/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK示例Privileged-1服务
 * 演示如何创建和实现一个Privileged-1服务
 *
 * 此示例实现一个简单的回显服务，接收消息并返回
 */

#include "privileged_service.h"
#include "types.h"
#include "console.h"
#include "string.h"
#include "mem.h"

/* 服务端点定义 */
#define SERVICE_ENDPOINT_ECHO  0x1000

/* 消息结构 */
typedef struct service_message {
    u32    type;        /* 消息类型 */
    u32    length;      /* 数据长度 */
    u8     data[256];   /* 消息数据 */
} service_message_t;

/* ============================================================
 * 服务端点处理函数
 * ============================================================ */

/**
 * 回显端点处理函数
 * 
 * 功能：接收消息，简单处理后返回
 * 
 * 参数：
 *   msg - 接收到的消息
 *   response - 响应消息（由调用者分配）
 * 
 * 返回：
 *   处理结果
 */
static hik_status_t echo_handler(service_message_t *msg, 
                                  service_message_t *response)
{
    if (!msg || !response) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    console_puts("[ECHO-SVC] Received message type: ");
    console_putu(msg->type);
    console_puts("\n");
    
    /* 处理消息 */
    switch (msg->type) {
        case 0x01:  /* ECHO请求 */
            response->type = 0x02;  /* ECHO响应 */
            response->length = msg->length;
            
            /* 复制数据 */
            if (msg->length <= 256) {
                memcopy(response->data, msg->data, msg->length);
            }
            break;
            
        case 0x03:  /* 获取服务信息 */
            response->type = 0x04;  /* 服务信息响应 */
            strcopy((char*)response->data, "Example Echo Service v1.0", 256);
            response->length = strlength((char*)response->data);
            break;
            
        default:
            response->type = 0xFF;  /* 错误响应 */
            response->length = 0;
            return HIK_ERROR_INVALID_PARAM;
    }
    
    return HIK_SUCCESS;
}

/**
 * 性能测试端点处理函数
 * 
 * 功能：执行简单的计算任务，测试性能
 */
static hik_status_t perf_handler(service_message_t *msg,
                                  service_message_t *response)
{
    u64 start = hal_get_timestamp();
    
    /* 执行一些计算 */
    volatile u64 sum = 0;
    for (u64 i = 0; i < 1000000; i++) {
        sum += i;
    }
    
    u64 end = hal_get_timestamp();
    u64 elapsed = end - start;
    
    /* 返回结果 */
    response->type = 0x06;  /* 性能测试响应 */
    response->length = sizeof(u64);
    memcopy(response->data, &elapsed, sizeof(u64));
    
    console_puts("[ECHO-SVC] Performance test: ");
    console_putu(elapsed);
    console_puts(" ns\n");
    
    return HIK_SUCCESS;
}

/* ============================================================
 * 服务初始化和清理
 * ============================================================ */

/**
 * 服务初始化函数
 * 
 * 当服务被加载时，Core-0会调用此函数
 * 
 * 返回：
 *   初始化状态
 */
hik_status_t service_init(void)
{
    console_puts("[ECHO-SVC] Initializing...\n");
    
    /* 注册端点 */
    cap_id_t echo_cap;
    hik_status_t status = privileged_service_register_endpoint(
        0,  /* domain_id由Core-0设置 */
        "echo_endpoint",
        (virt_addr_t)echo_handler,
        SERVICE_ENDPOINT_ECHO,
        &echo_cap
    );
    
    if (status != HIK_SUCCESS) {
        console_puts("[ECHO-SVC] Failed to register echo endpoint\n");
        return status;
    }
    
    /* 注册性能测试端点 */
    cap_id_t perf_cap;
    status = privileged_service_register_endpoint(
        0,
        "perf_endpoint",
        (virt_addr_t)perf_handler,
        0x1001,
        &perf_cap
    );
    
    if (status != HIK_SUCCESS) {
        console_puts("[ECHO-SVC] Failed to register perf endpoint\n");
        return status;
    }
    
    console_puts("[ECHO-SVC] Service initialized successfully\n");
    return HIK_SUCCESS;
}

/**
 * 服务启动函数
 * 
 * 当服务被启动时，Core-0会调用此函数
 */
hik_status_t service_start(void)
{
    console_puts("[ECHO-SVC] Service started\n");
    return HIK_SUCCESS;
}

/**
 * 服务停止函数
 * 
 * 当服务被停止时，Core-0会调用此函数
 */
hik_status_t service_stop(void)
{
    console_puts("[ECHO-SVC] Service stopped\n");
    return HIK_SUCCESS;
}

/**
 * 服务清理函数
 * 
 * 当服务被卸载时，Core-0会调用此函数
 */
hik_status_t service_cleanup(void)
{
    console_puts("[ECHO-SVC] Service cleanup\n");
    return HIK_SUCCESS;
}

/* ============================================================
 * 中断处理（如果服务需要处理中断）
 * ============================================================ */

/**
 * 中断处理函数
 * 
 * 如果服务注册了中断处理函数，Core-0会在中断发生时调用此函数
 * 
 * 参数：
 *   irq_vector - 中断向量号
 */
void irq_handler(u32 irq_vector)
{
    console_puts("[ECHO-SVC] IRQ handler: ");
    console_putu(irq_vector);
    console_puts("\n");
    
    /* 处理中断 */
    /* ... */
}

/* ============================================================
 * 模块元数据（用于模块加载器）
 * ============================================================ */

/* 模块头部（由构建系统生成） */
typedef struct hikmod_header {
    u32    magic;           /* 魔数：0x48494B4D ('HIKM') */
    u32    version;         /* 版本 */
    u64    code_size;       /* 代码段大小 */
    u64    data_size;       /* 数据段大小 */
    u64    sig_offset;      /* 签名偏移 */
    u64    sig_size;        /* 签名大小 */
    char   service_name[64];/* 服务名称 */
    char   service_uuid[37];/* 服务UUID */
    u32    service_type;    /* 服务类型 */
} hikmod_header_t;

/* 模块头部示例 */
static const hikmod_header_t g_module_header = {
    .magic = 0x48494B4D,
    .version = 0x00010000,  /* v1.0.0 */
    .code_size = 0x4000,    /* 16KB */
    .data_size = 0x2000,    /* 8KB */
    .sig_offset = 0x6000,
    .sig_size = 512,
    .service_name = "example_echo_service",
    .service_uuid = "12345678-1234-1234-1234-123456789abc",
    .service_type = SERVICE_TYPE_CUSTOM,
};

/* 服务函数表（由Core-0调用） */
typedef struct service_functions {
    hik_status_t (*init)(void);
    hik_status_t (*start)(void);
    hik_status_t (*stop)(void);
    hik_status_t (*cleanup)(void);
    void (*irq_handler)(u32);
} service_functions_t;

/* 服务函数表 */
static const service_functions_t g_service_funcs = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .irq_handler = irq_handler,
};

/* 导出符号（用于模块加载器） */
const hikmod_header_t* g_module_header_ptr = &g_module_header;
const service_functions_t* g_service_funcs_ptr = &g_service_funcs;

/* ============================================================
 * 辅助函数
 * ============================================================ */

static u32 strlength(const char *str)
{
    u32 len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}
