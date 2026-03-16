/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 安全扩展抽象层实现
 * 
 * 提供跨平台的安全特性功能
 */

#include "security_ext.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"
#include "include/hal.h"

/* 架构操作回调 */
static sec_arch_ops_t g_sec_arch_ops = {0};

/* 全局安全状态 */
static sec_state_t g_sec_state = {
    .mem_enc_type = SEC_MEM_ENC_NONE,
    .mem_enc_enabled = false,
    .tee_type = SEC_TEE_NONE,
    .tee_available = false,
    .cfi_mode = SEC_CFI_NONE,
    .cfi_enabled = false,
    .rng_available = false,
};

/* 默认内存加密能力 */
static sec_mem_enc_caps_t g_mem_enc_caps[SEC_MEM_ENC_MAX] = {
    { SEC_MEM_ENC_NONE,        "None",      false, false, 0,   0,  0, false, false, false },
    { SEC_MEM_ENC_AMD_SEV,     "AMD SEV",   false, false, 128, 8,  0, true,  true,  true  },
    { SEC_MEM_ENC_AMD_SEV_ES,  "SEV-ES",   false, false, 128, 8,  0, true,  true,  true  },
    { SEC_MEM_ENC_AMD_SEV_SNP, "SEV-SNP",  false, false, 128, 16, 0, true,  true,  true  },
    { SEC_MEM_ENC_INTEL_SGX,   "Intel SGX", false, false, 128, 0,  0, true,  false, true  },
    { SEC_MEM_ENC_INTEL_TDX,   "Intel TDX", false, false, 128, 0,  0, true,  true,  true  },
    { SEC_MEM_ENC_ARM_REALM,  "ARM Realm", false, false, 128, 0,  0, true,  true,  true  },
};

/* 默认 TEE 能力 */
static sec_tee_caps_t g_tee_caps = {
    .type = SEC_TEE_NONE,
    .name = "None",
    .supported = false,
    .secure_world = false,
    .isolated_memory = false,
    .trusted_storage = false,
};

/* 默认 CFI 能力 */
static sec_cfi_caps_t g_cfi_caps = {
    .modes_supported = 0,
    .max_shadow_size = 0,
    .supports_return_tracking = false,
    .supports_indirect_branch_tracking = false,
};

/* 默认 RNG 能力 */
static sec_rng_caps_t g_rng_caps = {
    .type = SEC_RNG_NONE,
    .name = "None",
    .supported = false,
    .entropy_bits = 0,
    .max_rate = 0,
    .passes_fips = false,
};

/* ========== 初始化 ========== */

/**
 * 初始化安全扩展
 */
void sec_init(void)
{
    console_puts("[SEC] Initializing security extensions\n");
    
    /* 清零状态 */
    memzero(&g_sec_state, sizeof(sec_state_t));
    
    /* 探测硬件安全特性 */
    if (g_sec_arch_ops.detect_mem_enc) {
        g_sec_arch_ops.detect_mem_enc();
    }
    if (g_sec_arch_ops.detect_tee) {
        g_sec_arch_ops.detect_tee();
    }
    if (g_sec_arch_ops.detect_cfi) {
        g_sec_arch_ops.detect_cfi();
    }
    if (g_sec_arch_ops.detect_rng) {
        g_sec_arch_ops.detect_rng();
    }
    
    console_puts("[SEC] Security extensions initialized\n");
    console_puts("[SEC]   Memory encryption: ");
    console_puts(g_sec_state.mem_enc_enabled ? "enabled" : "disabled");
    console_puts("\n");
    console_puts("[SEC]   TEE: ");
    console_puts(g_sec_state.tee_available ? "available" : "unavailable");
    console_puts("\n");
    console_puts("[SEC]   CFI: ");
    console_puts(g_sec_state.cfi_enabled ? "enabled" : "disabled");
    console_puts("\n");
    console_puts("[SEC]   RNG: ");
    console_puts(g_sec_state.rng_available ? "available" : "unavailable");
    console_puts("\n");
}

/**
 * 关闭安全扩展
 */
void sec_shutdown(void)
{
    console_puts("[SEC] Shutting down security extensions\n");
    
    /* 禁用所有安全特性 */
    sec_cfi_disable();
    sec_mem_enc_disable();
    
    memzero(&g_sec_state, sizeof(sec_state_t));
}

/**
 * 注册架构操作回调
 */
void sec_register_arch_ops(const sec_arch_ops_t *ops)
{
    if (ops) {
        memcopy(&g_sec_arch_ops, ops, sizeof(sec_arch_ops_t));
        console_puts("[SEC] Architecture ops registered\n");
    }
}

/* ========== 状态查询 ========== */

/**
 * 获取安全状态
 */
const sec_state_t* sec_get_state(void)
{
    return &g_sec_state;
}

/**
 * 打印安全信息
 */
void sec_print_info(void)
{
    console_puts("\n[SEC] ===== Security Extensions Status =====\n");
    
    console_puts("[SEC] Memory Encryption: ");
    if (g_sec_state.mem_enc_enabled) {
        console_puts(g_mem_enc_caps[g_sec_state.mem_enc_type].name);
        console_puts(" (enabled)\n");
    } else {
        console_puts("disabled\n");
    }
    
    console_puts("[SEC] TEE: ");
    if (g_sec_state.tee_available) {
        console_puts(g_tee_caps.name);
        console_puts("\n");
    } else {
        console_puts("unavailable\n");
    }
    
    console_puts("[SEC] CFI: ");
    if (g_sec_state.cfi_enabled) {
        console_puts("mode ");
        console_putu32(g_sec_state.cfi_mode);
        console_puts("\n");
    } else {
        console_puts("disabled\n");
    }
    
    console_puts("[SEC] RNG: ");
    if (g_sec_state.rng_available) {
        console_puts(g_rng_caps.name);
        console_puts(" (");
        console_putu32(g_rng_caps.entropy_bits);
        console_puts(" bits)\n");
    } else {
        console_puts("unavailable\n");
    }
    
    console_puts("[SEC] =======================================\n\n");
}

/* ========== 内存加密 ========== */

/**
 * 检查内存加密是否支持
 */
bool sec_mem_enc_supported(sec_mem_enc_type_t type)
{
    if (type >= SEC_MEM_ENC_MAX) {
        return false;
    }
    return g_mem_enc_caps[type].supported;
}

/**
 * 获取内存加密能力
 */
const sec_mem_enc_caps_t* sec_mem_enc_get_caps(sec_mem_enc_type_t type)
{
    if (type >= SEC_MEM_ENC_MAX) {
        return NULL;
    }
    return &g_mem_enc_caps[type];
}

/**
 * 启用内存加密
 */
hic_status_t sec_mem_enc_enable(sec_mem_enc_type_t type)
{
    if (!sec_mem_enc_supported(type)) {
        return HIC_ERROR_NOT_SUPPORTED;
    }
    
    if (g_sec_arch_ops.mem_enc_enable) {
        hic_status_t status = g_sec_arch_ops.mem_enc_enable(type);
        if (status != HIC_SUCCESS) {
            return status;
        }
    }
    
    g_sec_state.mem_enc_type = type;
    g_sec_state.mem_enc_enabled = true;
    g_mem_enc_caps[type].enabled = true;
    
    console_puts("[SEC] Memory encryption enabled: ");
    console_puts(g_mem_enc_caps[type].name);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

void sec_mem_enc_disable(void)
{
    if (!g_sec_state.mem_enc_enabled) {
        return;
    }
    
    if (g_sec_arch_ops.mem_enc_disable) {
        g_sec_arch_ops.mem_enc_disable();
    }
    
    g_mem_enc_caps[g_sec_state.mem_enc_type].enabled = false;
    g_sec_state.mem_enc_enabled = false;
    g_sec_state.mem_enc_type = SEC_MEM_ENC_NONE;
    
    console_puts("[SEC] Memory encryption disabled\n");
}

u64 sec_mem_enc_create_region(phys_addr_t base, u64 size, u32 flags)
{
    if (!g_sec_state.mem_enc_enabled) {
        return 0;
    }
    
    if (g_sec_arch_ops.mem_enc_create_region) {
        return g_sec_arch_ops.mem_enc_create_region(base, size, flags);
    }
    
    return 0;
}

void sec_mem_enc_destroy_region(u64 region_id)
{
    if (g_sec_arch_ops.mem_enc_destroy_region) {
        g_sec_arch_ops.mem_enc_destroy_region(region_id);
    }
}

bool sec_mem_enc_is_encrypted(phys_addr_t addr)
{
    if (!g_sec_state.mem_enc_enabled) {
        return false;
    }
    
    if (g_sec_arch_ops.mem_enc_is_encrypted) {
        return g_sec_arch_ops.mem_enc_is_encrypted(addr);
    }
    
    return false;
}

/* ========== TEE ========== */

bool sec_tee_supported(void)
{
    return g_sec_state.tee_available;
}

const sec_tee_caps_t* sec_tee_get_caps(void)
{
    return &g_tee_caps;
}

u64 sec_tee_open_session(u64 app_id)
{
    if (!g_sec_state.tee_available) {
        return 0;
    }
    
    if (g_sec_arch_ops.tee_open_session) {
        return g_sec_arch_ops.tee_open_session(app_id);
    }
    
    return 0;
}

void sec_tee_close_session(u64 session_id)
{
    if (g_sec_arch_ops.tee_close_session) {
        g_sec_arch_ops.tee_close_session(session_id);
    }
}

hic_status_t sec_tee_invoke(u64 session_id, u32 command,
                            void *params, u32 param_count)
{
    if (!g_sec_state.tee_available) {
        return HIC_ERROR_NOT_SUPPORTED;
    }
    
    if (g_sec_arch_ops.tee_invoke) {
        return g_sec_arch_ops.tee_invoke(session_id, command, params, param_count);
    }
    
    return HIC_ERROR_NOT_IMPLEMENTED;
}

/* ========== CFI ========== */

bool sec_cfi_supported(sec_cfi_mode_t mode)
{
    return (g_cfi_caps.modes_supported & (1 << mode)) != 0;
}

const sec_cfi_caps_t* sec_cfi_get_caps(void)
{
    return &g_cfi_caps;
}

hic_status_t sec_cfi_enable(sec_cfi_mode_t mode)
{
    if (!sec_cfi_supported(mode)) {
        return HIC_ERROR_NOT_SUPPORTED;
    }
    
    if (g_sec_arch_ops.cfi_enable) {
        hic_status_t status = g_sec_arch_ops.cfi_enable(mode);
        if (status != HIC_SUCCESS) {
            return status;
        }
    }
    
    g_sec_state.cfi_mode = mode;
    g_sec_state.cfi_enabled = true;
    
    console_puts("[SEC] CFI enabled, mode ");
    console_putu32(mode);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

void sec_cfi_disable(void)
{
    if (!g_sec_state.cfi_enabled) {
        return;
    }
    
    if (g_sec_arch_ops.cfi_disable) {
        g_sec_arch_ops.cfi_disable();
    }
    
    g_sec_state.cfi_enabled = false;
    g_sec_state.cfi_mode = SEC_CFI_NONE;
    
    console_puts("[SEC] CFI disabled\n");
}

sec_cfi_mode_t sec_cfi_get_mode(void)
{
    return g_sec_state.cfi_mode;
}

u64 sec_cfi_get_violations(void)
{
    if (g_sec_arch_ops.cfi_get_violations) {
        return g_sec_arch_ops.cfi_get_violations();
    }
    return 0;
}

/* ========== RNG ========== */

bool sec_rng_supported(void)
{
    return g_sec_state.rng_available;
}

const sec_rng_caps_t* sec_rng_get_caps(void)
{
    return &g_rng_caps;
}

bool sec_rng_get_bytes(void *buffer, u32 count)
{
    if (!g_sec_state.rng_available || !buffer || count == 0) {
        return false;
    }
    
    if (g_sec_arch_ops.rng_get_bytes) {
        return g_sec_arch_ops.rng_get_bytes(buffer, count);
    }
    
    /* 软件回退：使用时间戳作为伪随机源 */
    u8 *buf = (u8 *)buffer;
    u64 seed = hal_get_timestamp();
    for (u32 i = 0; i < count; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = (u8)(seed >> 32);
    }
    
    return true;
}

u32 sec_rng_get_u32(void)
{
    u32 value;
    if (sec_rng_get_bytes(&value, sizeof(value))) {
        return value;
    }
    return (u32)hal_get_timestamp();
}

u64 sec_rng_get_u64(void)
{
    u64 value;
    if (sec_rng_get_bytes(&value, sizeof(value))) {
        return value;
    }
    return hal_get_timestamp();
}

bool sec_rng_fill_buffer(void *buffer, u32 size)
{
    return sec_rng_get_bytes(buffer, size);
}

/* ========== 密钥管理 ========== */

u64 sec_key_create(sec_key_type_t type, sec_key_algo_t algo,
                   const void *key_data, u32 key_size, u32 flags)
{
    if (g_sec_arch_ops.key_create) {
        return g_sec_arch_ops.key_create(type, algo, key_data, key_size, flags);
    }
    return 0;
}

void sec_key_destroy(u64 key_id)
{
    if (g_sec_arch_ops.key_destroy) {
        g_sec_arch_ops.key_destroy(key_id);
    }
}

const sec_key_handle_t* sec_key_get_handle(u64 key_id)
{
    (void)key_id;
    return NULL;
}

bool sec_key_use(u64 key_id, void *data, u32 size, bool encrypt)
{
    if (g_sec_arch_ops.key_use) {
        return g_sec_arch_ops.key_use(key_id, data, size, encrypt);
    }
    return false;
}

u64 sec_key_derive(u64 parent_key_id, const void *context, u32 context_size,
                   sec_key_algo_t target_algo)
{
    if (g_sec_arch_ops.key_derive) {
        return g_sec_arch_ops.key_derive(parent_key_id, context, context_size, target_algo);
    }
    return 0;
}

/* ========== 安全认证 ========== */

hic_status_t sec_attestation_get_report(void *report, u32 *size,
                                         const void *nonce, u32 nonce_size)
{
    if (g_sec_arch_ops.attestation_get_report) {
        return g_sec_arch_ops.attestation_get_report(report, size, nonce, nonce_size);
    }
    return HIC_ERROR_NOT_IMPLEMENTED;
}

bool sec_attestation_verify_report(const void *report, u32 size)
{
    if (g_sec_arch_ops.attestation_verify_report) {
        return g_sec_arch_ops.attestation_verify_report(report, size);
    }
    return false;
}

/* ========== 内部接口（供架构层使用） ========== */

void sec_set_mem_enc_supported(sec_mem_enc_type_t type, bool supported)
{
    if (type < SEC_MEM_ENC_MAX) {
        g_mem_enc_caps[type].supported = supported;
    }
}

void sec_set_tee_caps(const sec_tee_caps_t *caps)
{
    if (caps) {
        memcopy(&g_tee_caps, caps, sizeof(sec_tee_caps_t));
        g_sec_state.tee_available = caps->supported;
        g_sec_state.tee_type = caps->type;
    }
}

void sec_set_cfi_caps(const sec_cfi_caps_t *caps)
{
    if (caps) {
        memcopy(&g_cfi_caps, caps, sizeof(sec_cfi_caps_t));
    }
}

void sec_set_rng_caps(const sec_rng_caps_t *caps)
{
    if (caps) {
        memcopy(&g_rng_caps, caps, sizeof(sec_rng_caps_t));
        g_sec_state.rng_available = caps->supported;
    }
}
