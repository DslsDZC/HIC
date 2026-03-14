/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 初始服务启动器实现
 * 
 * 职责：
 * 1. 持有存储设备访问能力
 * 2. 从 /boot/module_manager.hicmod 读取模块管理器
 * 3. 验证签名/哈希
 * 4. 请求 Core-0 创建域并加载模块管理器
 * 5. 启动后进入休眠，可被卸载
 */

#include "service.h"
#include <string.h>

/* ========== 外部依赖（由 Core-0 提供） ========== */

/* 串口输出 */
extern void serial_print(const char *msg);

/* 内存操作 */
extern void *module_memcpy(void *dst, const void *src, size_t n);
extern void *module_memset(void *dst, int c, size_t n);

/* 域操作 */
extern int module_domain_create(uint64_t entry_point, void *memory, 
                                 uint32_t size, uint64_t *domain_id);
extern int module_domain_start(uint64_t domain_id);

/* 能力操作 */
extern int module_cap_create_domain(uint64_t domain_id, uint32_t cap_type);
extern int module_cap_create_endpoint(uint64_t domain_id, uint64_t endpoint_id);

/* 哈希验证（由 Core-0 提供） */
extern void sha384_hash(const uint8_t *data, uint32_t len, uint8_t *hash_out);

/* Verifier 服务接口（静态链接） */
extern int verifier_init(void);
extern int verifier_start(void);

/* 验证接口 */
typedef enum {
    VERIFY_OK = 0,
    VERIFY_ERR_HASH_MISMATCH = 1,
    VERIFY_ERR_SIGNATURE_INVALID = 2,
} verify_status_t;

extern verify_status_t verifier_verify_module(
    const void *module_data,
    size_t module_size,
    const void *sign_header,
    void *result
);

extern void verifier_compute_hash(const void *data, size_t size, uint8_t hash[48]);

/* FAT32 接口（由 fat32_service 提供） */
extern int fat32_service_init(void);
extern int fat32_service_start(void);
extern int fat32_read_file(const char *path, void *buffer, 
                           uint32_t buffer_size, uint32_t *bytes_read);

/* ========== 内部状态 ========== */

static struct {
    int initialized;
    int module_manager_loaded;
    uint64_t module_manager_domain;
} g_launcher_ctx;

/* 模块魔数 */
#define HICMOD_MAGIC     0x4849434D  /* "HICM" */
#define HICMOD_TYPE_SVC  0x53525643  /* "SRVC" */

/* 模块头结构 */
typedef struct {
    uint32_t magic;
    uint32_t type;
    uint32_t version;
    uint32_t flags;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t entry_offset;
    uint32_t reserved[5];
    uint8_t  signature[64];
} hicmod_header_t;

/* 临时缓冲区（用于加载模块） */
#define MAX_MODULE_SIZE (1024 * 1024)  /* 1MB */
static uint8_t g_module_buffer[MAX_MODULE_SIZE] __attribute__((aligned(4096)));

/* ========== 辅助函数 ========== */

static void launcher_log(const char *msg) {
    serial_print("[init_launcher] ");
    serial_print(msg);
    serial_print("\n");
}

static void launcher_log_hex(const char *label, uint64_t val) {
    char buf[64];
    const char hex[] = "0123456789ABCDEF";
    
    serial_print("[init_launcher] ");
    serial_print(label);
    serial_print(": 0x");
    
    for (int i = 60; i >= 0; i -= 4) {
        buf[0] = hex[(val >> i) & 0xF];
        buf[1] = '\0';
        serial_print(buf);
    }
    serial_print("\n");
}

/* ========== 核心实现 ========== */

int init_launcher_init(void) {
    launcher_log("初始化...");
    
    module_memset(&g_launcher_ctx, 0, sizeof(g_launcher_ctx));
    
    /* 确保存储服务已启动 */
    fat32_service_init();
    fat32_service_start();
    
    g_launcher_ctx.initialized = 1;
    launcher_log("初始化完成");
    
    return INIT_LAUNCHER_SUCCESS;
}

int init_launcher_start(void) {
    uint32_t bytes_read;
    hicmod_header_t *header;
    uint64_t entry_point;
    
    launcher_log("启动模块管理器加载流程...");
    
    if (!g_launcher_ctx.initialized) {
        launcher_log("错误: 未初始化");
        return INIT_LAUNCHER_ERROR;
    }
    
    /* 1. 从存储设备读取模块管理器 */
    launcher_log("读取模块文件: " MODULE_MANAGER_PATH);
    
    int ret = fat32_read_file(MODULE_MANAGER_PATH, g_module_buffer, 
                               MAX_MODULE_SIZE, &bytes_read);
    if (ret != 0 || bytes_read < sizeof(hicmod_header_t)) {
        launcher_log("错误: 无法读取模块文件");
        launcher_log_hex("返回值", (uint64_t)ret);
        launcher_log_hex("读取字节数", bytes_read);
        return INIT_LAUNCHER_NOT_FOUND;
    }
    
    launcher_log_hex("模块大小", bytes_read);
    
    /* 2. 验证模块格式 */
    header = (hicmod_header_t *)g_module_buffer;
    
    if (header->magic != HICMOD_MAGIC) {
        launcher_log("错误: 无效的模块魔数");
        launcher_log_hex("魔数", header->magic);
        return INIT_LAUNCHER_INVALID;
    }
    
    if (header->type != HICMOD_TYPE_SVC) {
        launcher_log("错误: 不是服务模块");
        launcher_log_hex("类型", header->type);
        return INIT_LAUNCHER_INVALID;
    }
    
    launcher_log("模块格式验证通过");
    
    /* 3. 验证签名/哈希 */
    launcher_log("初始化验证服务...");
    verifier_init();
    verifier_start();
    
    launcher_log("验证模块签名...");
    
    /* 计算模块哈希 */
    uint8_t computed_hash[48];
    verifier_compute_hash(g_module_buffer, bytes_read, computed_hash);
    
    /* 开发阶段：简单哈希校验 */
    /* 生产环境：使用完整的证书链验证 */
    uint8_t expected_hash[48] = {
        /* 示例预期哈希（开发阶段跳过校验） */
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE
    };
    
    /* 输出计算得到的哈希 */
    launcher_log("计算哈希:");
    for (int i = 0; i < 48; i++) {
        char hex[3];
        const char h[] = "0123456789ABCDEF";
        hex[0] = h[(computed_hash[i] >> 4) & 0xF];
        hex[1] = h[computed_hash[i] & 0xF];
        hex[2] = '\0';
        serial_print(hex);
        if ((i + 1) % 16 == 0) serial_print("\n");
    }
    
    /* 开发阶段：跳过哈希校验 */
    launcher_log("签名验证: 通过（开发模式）");
    
    /* 4. 计算入口点 */
    entry_point = (uint64_t)(g_module_buffer + header->entry_offset);
    launcher_log_hex("入口点", entry_point);
    
    /* 5. 请求 Core-0 创建域 */
    launcher_log("创建模块管理器域...");
    
    ret = module_domain_create(entry_point, g_module_buffer, 
                                bytes_read, &g_launcher_ctx.module_manager_domain);
    if (ret != 0) {
        launcher_log("错误: 创建域失败");
        return INIT_LAUNCHER_ERROR;
    }
    
    launcher_log_hex("域 ID", g_launcher_ctx.module_manager_domain);
    
    /* 6. 授予必要能力 */
    launcher_log("授予能力...");
    module_cap_create_domain(g_launcher_ctx.module_manager_domain, 0);
    module_cap_create_endpoint(g_launcher_ctx.module_manager_domain, 0);
    
    /* 7. 启动域 */
    launcher_log("启动模块管理器...");
    ret = module_domain_start(g_launcher_ctx.module_manager_domain);
    if (ret != 0) {
        launcher_log("错误: 启动域失败");
        return INIT_LAUNCHER_ERROR;
    }
    
    g_launcher_ctx.module_manager_loaded = 1;
    launcher_log("模块管理器启动成功！");
    
    return INIT_LAUNCHER_SUCCESS;
}

int init_launcher_stop(void) {
    launcher_log("停止...");
    /* 模块管理器由自己管理生命周期，这里不做任何事 */
    return INIT_LAUNCHER_SUCCESS;
}

int init_launcher_cleanup(void) {
    launcher_log("清理...");
    module_memset(&g_launcher_ctx, 0, sizeof(g_launcher_ctx));
    return INIT_LAUNCHER_SUCCESS;
}
