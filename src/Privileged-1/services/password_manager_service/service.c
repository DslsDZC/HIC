/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 密码管理服务实现
 */

#include "service.h"
#include "../include/service_api.h"
#include "../../Core-0/lib/mem.h"
#include "../../Core-0/lib/console.h"

/* 默认密码（从YAML配置读取） */
static const char DEFAULT_PASSWORD[] = "admin123";

/* 全局密码管理器 */
static password_manager_t g_password_manager;

/* SHA-384哈希函数（简化版） */
static void sha384_hash(const u8* data, u32 len, u8* hash) {
    /* TODO: 调用crypto_service的SHA-384实现 */
    /* 临时使用简单的XOR哈希 */
    memzero(hash, PASSWORD_HASH_SIZE);
    for (u32 i = 0; i < len; i++) {
        hash[i % PASSWORD_HASH_SIZE] ^= data[i];
    }
}

/* 生成随机盐 */
static void generate_salt(u8* salt, u32 size) {
    /* TODO: 使用硬件随机数生成器 */
    /* 临时使用简单模式 */
    for (u32 i = 0; i < size; i++) {
        salt[i] = (u8)(i * 7 + 13);
    }
}

/* 计算密码哈希 */
static void compute_password_hash(const char* password, u8* hash, u8* salt) {
    if (!salt) {
        salt = (u8*)password;  /* 如果没有盐，使用密码本身 */
    }
    
    sha384_hash((const u8*)password, strlen(password), hash);
    
    /* 与盐混合 */
    for (u32 i = 0; i < PASSWORD_HASH_SIZE; i++) {
        hash[i] ^= salt[i % PASSWORD_SALT_SIZE];
    }
}

/* 比较哈希 */
static bool compare_hash(const u8* hash1, const u8* hash2) {
    for (u32 i = 0; i < PASSWORD_HASH_SIZE; i++) {
        if (hash1[i] != hash2[i]) {
            return false;
        }
    }
    return true;
}

/* 检查密码策略 */
static bool check_password_policy(const char* password) {
    const password_policy_t* policy = &g_password_manager.policy;
    u32 len = strlen(password);
    
    /* 检查长度 */
    if (len < policy->min_length || len > policy->max_length) {
        return false;
    }
    
    /* 检查字符类型 */
    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;
    bool has_special = false;
    
    for (u32 i = 0; i < len; i++) {
        char c = password[i];
        if (c >= 'A' && c <= 'Z') has_upper = true;
        else if (c >= 'a' && c <= 'z') has_lower = true;
        else if (c >= '0' && c <= '9') has_digit = true;
        else has_special = true;
    }
    
    if (policy->require_uppercase && !has_upper) return false;
    if (policy->require_lowercase && !has_lower) return false;
    if (policy->require_digit && !has_digit) return false;
    if (policy->require_special && !has_special) return false;
    
    return true;
}

/* ============= 公共API实现 ============= */

/**
 * 初始化密码管理器
 */
hic_status_t password_manager_init(void) {
    memzero(&g_password_manager, sizeof(password_manager_t));
    
    /* 设置默认密码策略 */
    g_password_manager.policy.min_length = 8;
    g_password_manager.policy.max_length = 128;
    g_password_manager.policy.require_uppercase = true;
    g_password_manager.policy.require_lowercase = true;
    g_password_manager.policy.require_digit = true;
    g_password_manager.policy.require_special = false;
    g_password_manager.policy.max_age_days = 90;
    
    /* 设置默认管理员密码 */
    password_set(DEFAULT_PASSWORD, true);
    
    g_password_manager.initialized = true;
    
    console_puts("[PASSWORD] Password manager initialized\n");
    return HIC_SUCCESS;
}

/**
 * 验证密码
 */
hic_status_t password_verify(const char* password) {
    if (!g_password_manager.initialized) {
        return HIC_ERROR_NOT_INITIALIZED;
    }
    
    if (!password || strlen(password) == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查管理员密码 */
    u8 computed_hash[PASSWORD_HASH_SIZE];
    compute_password_hash(password, computed_hash, g_password_manager.admin_password.salt);
    
    if (compare_hash(computed_hash, g_password_manager.admin_password.hash)) {
        /* 重置尝试次数 */
        g_password_manager.admin_password.attempts = 0;
        return HIC_SUCCESS;
    }
    
    /* 增加失败次数 */
    g_password_manager.admin_password.attempts++;
    
    if (g_password_manager.admin_password.attempts >= g_password_manager.admin_password.max_attempts) {
        console_puts("[PASSWORD] Too many failed attempts!\n");
        return HIC_ERROR_LOCKED;
    }
    
    return HIC_ERROR_AUTH_FAILED;
}

/**
 * 设置密码
 */
hic_status_t password_set(const char* password, bool is_admin) {
    if (!password || strlen(password) == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查密码策略 */
    if (!check_password_policy(password)) {
        console_puts("[PASSWORD] Password does not meet policy requirements\n");
        return HIC_ERROR_INVALID_PARAM;
    }
    
    password_hash_t* hash_store = is_admin ? &g_password_manager.admin_password 
                                          : &g_password_manager.user_password;
    
    /* 生成盐 */
    generate_salt(hash_store->salt, PASSWORD_SALT_SIZE);
    
    /* 计算哈希 */
    compute_password_hash(password, hash_store->hash, hash_store->salt);
    
    /* 设置元数据 */
    hash_store->timestamp = 0;  /* TODO: 获取当前时间戳 */
    hash_store->attempts = 0;
    hash_store->max_attempts = 5;
    
    console_puts(is_admin ? "[PASSWORD] Admin password set\n" : "[PASSWORD] User password set\n");
    return HIC_SUCCESS;
}

/**
 * 修改密码
 */
hic_status_t password_change(const char* old_password, const char* new_password) {
    if (!old_password || !new_password) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 验证旧密码 */
    if (password_verify(old_password) != HIC_SUCCESS) {
        return HIC_ERROR_AUTH_FAILED;
    }
    
    /* 设置新密码 */
    return password_set(new_password, true);
}

/**
 * 检查密码强度
 */
hic_status_t password_check_strength(const char* password, u32* strength) {
    if (!password || !strength) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u32 score = 0;
    u32 len = strlen(password);
    
    /* 长度分数 */
    if (len >= 8) score += 20;
    if (len >= 12) score += 20;
    if (len >= 16) score += 10;
    
    /* 字符类型分数 */
    bool has_upper = false, has_lower = false, has_digit = false, has_special = false;
    for (u32 i = 0; i < len; i++) {
        char c = password[i];
        if (c >= 'A' && c <= 'Z') has_upper = true;
        else if (c >= 'a' && c <= 'z') has_lower = true;
        else if (c >= '0' && c <= '9') has_digit = true;
        else has_special = true;
    }
    
    if (has_upper) score += 10;
    if (has_lower) score += 10;
    if (has_digit) score += 10;
    if (has_special) score += 20;
    
    *strength = (score > 100) ? 100 : score;
    return HIC_SUCCESS;
}

/**
 * 获取密码策略
 */
hic_status_t password_get_policy(password_policy_t* policy) {
    if (!policy) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memcopy(policy, &g_password_manager.policy, sizeof(password_policy_t));
    return HIC_SUCCESS;
}

/**
 * 设置密码策略
 */
hic_status_t password_set_policy(const password_policy_t* policy) {
    if (!policy) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memcopy(&g_password_manager.policy, policy, sizeof(password_policy_t));
    console_puts("[PASSWORD] Password policy updated\n");
    return HIC_SUCCESS;
}

/* ============= 服务API实现 ============= */

static hic_status_t service_init(void) {
    return password_manager_init();
}

static hic_status_t service_start(void) {
    console_puts("[PASSWORD] Password manager service started\n");
    return HIC_SUCCESS;
}

static hic_status_t service_stop(void) {
    console_puts("[PASSWORD] Password manager service stopped\n");
    return HIC_SUCCESS;
}

static hic_status_t service_cleanup(void) {
    memzero(&g_password_manager, sizeof(password_manager_t));
    return HIC_SUCCESS;
}

static hic_status_t service_get_info(char* buffer, u32 buffer_size) {
    if (!buffer || buffer_size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    const char* info = "Password Manager Service v1.0.0 - "
                       "Provides secure password management and verification";
    u32 len = strlen(info);
    
    if (len >= buffer_size) {
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    strcpy(buffer, info);
    return HIC_SUCCESS;
}

/* 服务API表（导出符号） */
const service_api_t g_service_api = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .get_info = service_get_info,
};

/* 服务注册函数 */
void service_register_self(void) {
    service_register("password_manager_service", &g_service_api);
}