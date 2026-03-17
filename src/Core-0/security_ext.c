/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 安全扩展机制层实现
 * 
 * 提供硬件安全特性的操作原语
 */

#include "security_ext.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"
#include "include/hal.h"

/* 架构操作回调 */
static sec_arch_ops_t g_sec_arch_ops = {0};

/* 全局安全状态 */
static sec_state_t g_sec_state = {0};

/* 内存加密能力表 */
static sec_mem_enc_caps_t g_mem_enc_caps[SEC_MEM_ENC_MAX] = {
    { SEC_MEM_ENC_NONE,        "None",      false, false, 0 },
    { SEC_MEM_ENC_AMD_SEV,     "AMD SEV",   false, false, 128 },
    { SEC_MEM_ENC_AMD_SEV_ES,  "SEV-ES",    false, false, 128 },
    { SEC_MEM_ENC_AMD_SEV_SNP, "SEV-SNP",   false, false, 128 },
    { SEC_MEM_ENC_INTEL_TDX,   "Intel TDX", false, false, 128 },
    { SEC_MEM_ENC_ARM_REALM,   "ARM Realm", false, false, 128 },
};

/* TEE 能力 */
static sec_tee_caps_t g_tee_caps = {
    .type = SEC_TEE_NONE,
    .name = "None",
    .supported = false,
};

/* CFI 能力 */
static sec_cfi_caps_t g_cfi_caps = {
    .modes_supported = 0,
    .hw_enforced = false,
    .violations_detected = 0,
};

/* RNG 能力 */
static sec_rng_caps_t g_rng_caps = {
    .supported = false,
    .entropy_bits = 0,
    .passes_fips = false,
};

/* ========== 初始化 ========== */

void sec_init(void)
{
    memzero(&g_sec_state, sizeof(sec_state_t));
    
    if (g_sec_arch_ops.detect_mem_enc) g_sec_arch_ops.detect_mem_enc();
    if (g_sec_arch_ops.detect_tee) g_sec_arch_ops.detect_tee();
    if (g_sec_arch_ops.detect_cfi) g_sec_arch_ops.detect_cfi();
    if (g_sec_arch_ops.detect_rng) g_sec_arch_ops.detect_rng();
    
    console_puts("[SEC] Security extensions initialized\n");
}

void sec_shutdown(void)
{
    sec_cfi_disable();
    sec_mem_enc_disable();
    memzero(&g_sec_state, sizeof(sec_state_t));
}

void sec_register_arch_ops(const sec_arch_ops_t *ops)
{
    if (ops) {
        memcopy(&g_sec_arch_ops, ops, sizeof(sec_arch_ops_t));
    }
}

/* ========== 状态查询 ========== */

const sec_state_t* sec_get_state(void)
{
    return &g_sec_state;
}

/* ========== 内存加密 ========== */

bool sec_mem_enc_supported(sec_mem_enc_type_t type)
{
    if (type >= SEC_MEM_ENC_MAX) return false;
    return g_mem_enc_caps[type].supported;
}

const sec_mem_enc_caps_t* sec_mem_enc_get_caps(sec_mem_enc_type_t type)
{
    if (type >= SEC_MEM_ENC_MAX) return NULL;
    return &g_mem_enc_caps[type];
}

hic_status_t sec_mem_enc_enable(sec_mem_enc_type_t type)
{
    if (!sec_mem_enc_supported(type)) return HIC_ERROR_NOT_SUPPORTED;
    
    if (g_sec_arch_ops.mem_enc_enable) {
        hic_status_t status = g_sec_arch_ops.mem_enc_enable(type);
        if (status != HIC_SUCCESS) return status;
    }
    
    g_sec_state.mem_enc_type = type;
    g_sec_state.mem_enc_enabled = true;
    g_mem_enc_caps[type].enabled = true;
    
    return HIC_SUCCESS;
}

void sec_mem_enc_disable(void)
{
    if (!g_sec_state.mem_enc_enabled) return;
    
    if (g_sec_arch_ops.mem_enc_disable) {
        g_sec_arch_ops.mem_enc_disable();
    }
    
    g_mem_enc_caps[g_sec_state.mem_enc_type].enabled = false;
    g_sec_state.mem_enc_enabled = false;
    g_sec_state.mem_enc_type = SEC_MEM_ENC_NONE;
}

u64 sec_mem_enc_create_region(phys_addr_t base, u64 size, u32 flags)
{
    if (!g_sec_state.mem_enc_enabled) return 0;
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
    if (!g_sec_state.tee_available) return 0;
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

hic_status_t sec_tee_invoke(u64 session_id, u32 command, void *params, u32 count)
{
    if (!g_sec_state.tee_available) return HIC_ERROR_NOT_SUPPORTED;
    if (g_sec_arch_ops.tee_invoke) {
        return g_sec_arch_ops.tee_invoke(session_id, command, params, count);
    }
    return HIC_ERROR_NOT_SUPPORTED;
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
    if (!sec_cfi_supported(mode)) return HIC_ERROR_NOT_SUPPORTED;
    
    if (g_sec_arch_ops.cfi_enable) {
        hic_status_t status = g_sec_arch_ops.cfi_enable(mode);
        if (status != HIC_SUCCESS) return status;
    }
    
    g_sec_state.cfi_mode = mode;
    g_sec_state.cfi_enabled = true;
    return HIC_SUCCESS;
}

void sec_cfi_disable(void)
{
    if (!g_sec_state.cfi_enabled) return;
    if (g_sec_arch_ops.cfi_disable) {
        g_sec_arch_ops.cfi_disable();
    }
    g_sec_state.cfi_enabled = false;
    g_sec_state.cfi_mode = SEC_CFI_NONE;
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
    return g_cfi_caps.violations_detected;
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
    if (!g_sec_state.rng_available || !buffer || count == 0) return false;
    
    if (g_sec_arch_ops.rng_get_bytes) {
        return g_sec_arch_ops.rng_get_bytes(buffer, count);
    }
    
    /* 软件回退 */
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
    if (sec_rng_get_bytes(&value, sizeof(value))) return value;
    return (u32)hal_get_timestamp();
}

u64 sec_rng_get_u64(void)
{
    u64 value;
    if (sec_rng_get_bytes(&value, sizeof(value))) return value;
    return hal_get_timestamp();
}

/* ========== 密钥管理 ========== */

u64 sec_key_create(sec_key_type_t type, sec_key_algo_t algo, const void *data, u32 size, u32 flags)
{
    if (g_sec_arch_ops.key_create) {
        return g_sec_arch_ops.key_create(type, algo, data, size, flags);
    }
    return 0;
}

void sec_key_destroy(u64 key_id)
{
    if (g_sec_arch_ops.key_destroy) {
        g_sec_arch_ops.key_destroy(key_id);
    }
}

bool sec_key_use(u64 key_id, void *data, u32 size, bool encrypt)
{
    if (g_sec_arch_ops.key_use) {
        return g_sec_arch_ops.key_use(key_id, data, size, encrypt);
    }
    return false;
}

/* ========== 内部接口（供架构层设置能力） ========== */

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