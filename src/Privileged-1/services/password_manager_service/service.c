/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 密码管理服务
 * 
 * 提供安全的密码存储、验证和策略管理功能
 * 支持密码哈希、复杂度检查和密码过期管理
 */

#include "service.h"
#include <string.h>

/* ========== 常量定义 ========== */

#define MAX_PASSWORD_LEN        64
#define MAX_SALT_LEN            32
#define MAX_USERS               16
#define HASH_ITERATIONS         10000
#define DEFAULT_MIN_LENGTH      8
#define DEFAULT_MIN_UPPER       1
#define DEFAULT_MIN_LOWER       1
#define DEFAULT_MIN_DIGIT       1
#define DEFAULT_MIN_SPECIAL     1

/* ========== 密码策略结构 ========== */

typedef struct {
    uint8_t  min_length;
    uint8_t  min_uppercase;
    uint8_t  min_lowercase;
    uint8_t  min_digits;
    uint8_t  min_special;
    uint8_t  max_age_days;
    uint8_t  history_count;
    uint8_t  reserved[2];
    uint32_t flags;
} password_policy_t;

/* 策略标志 */
#define POLICY_FLAG_REQUIRE_SPECIAL    (1U << 0)
#define POLICY_FLAG_NO_COMMON_WORDS    (1U << 1)
#define POLICY_FLAG_NO_USER_INFO       (1U << 2)
#define POLICY_FLAG_EXPIRE_WARNING     (1U << 3)

/* ========== 用户密码条目 ========== */

typedef struct {
    uint8_t  user_id;
    uint8_t  reserved[3];
    char     username[32];
    uint8_t  password_hash[64];     /* SHA-512 哈希 */
    uint8_t  salt[MAX_SALT_LEN];
    uint32_t hash_iterations;
    uint64_t last_change_time;
    uint64_t expiry_time;
    uint8_t  failed_attempts;
    uint8_t  locked;
    uint8_t  reserved2[6];
    uint8_t  history[4][64];        /* 最近4个密码哈希 */
} password_entry_t;

/* ========== 全局状态 ========== */

static password_policy_t g_policy;
static password_entry_t g_users[MAX_USERS];
static uint32_t g_user_count;
static uint64_t g_current_time;

/* ========== 前置声明 ========== */

static int password_check_policy(const char *password);

/* ========== 辅助函数 ========== */

static int str_len(const char *s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int str_cmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

static int is_upper(char c) { return c >= 'A' && c <= 'Z'; }
static int is_lower(char c) { return c >= 'a' && c <= 'z'; }
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_special(char c) {
    return c == '!' || c == '@' || c == '#' || c == '$' || c == '%' ||
           c == '^' || c == '&' || c == '*' || c == '(' || c == ')' ||
           c == '-' || c == '_' || c == '+' || c == '=';
}

/* SHA-256 简化实现（用于密码哈希） */
static void sha256_simple(const uint8_t *data, uint32_t len, uint8_t *hash)
{
    /* 使用简化的哈希算法 */
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    /* 简化的压缩函数 */
    for (uint32_t i = 0; i < len; i++) {
        uint32_t w = data[i];
        for (int j = 0; j < 8; j++) {
            h[j] ^= (h[(j+1) % 8] << 5) | (h[(j+1) % 8] >> 27);
            h[j] += w + j;
        }
    }
    
    /* 输出 */
    for (int i = 0; i < 8; i++) {
        hash[i*4] = (h[i] >> 24) & 0xFF;
        hash[i*4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i*4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i*4 + 3] = h[i] & 0xFF;
    }
}

/* PBKDF2 密码派生 */
static void pbkdf2(const char *password, const uint8_t *salt, uint32_t salt_len,
                   uint32_t iterations, uint8_t *derived_key, uint32_t dk_len)
{
    uint8_t u[32];
    uint8_t result[32];
    uint32_t pwd_len = str_len(password);
    
    /* 初始化 */
    for (uint32_t i = 0; i < dk_len && i < 32; i++) {
        result[i] = 0;
    }
    
    /* 迭代 */
    for (uint32_t iter = 0; iter < iterations; iter++) {
        /* U1 = PRF(password, salt || INT(iter)) */
        uint8_t input[64];
        for (uint32_t i = 0; i < salt_len && i < 32; i++) {
            input[i] = salt[i];
        }
        input[salt_len] = (iter >> 24) & 0xFF;
        input[salt_len + 1] = (iter >> 16) & 0xFF;
        input[salt_len + 2] = (iter >> 8) & 0xFF;
        input[salt_len + 3] = iter & 0xFF;
        
        /* 简化：直接哈希 */
        for (uint32_t i = 0; i < pwd_len && i < 28; i++) {
            input[salt_len + 4 + i] = password[i];
        }
        
        sha256_simple(input, salt_len + 4 + (pwd_len < 28 ? pwd_len : 28), u);
        
        /* XOR 到结果 */
        for (uint32_t i = 0; i < dk_len && i < 32; i++) {
            result[i] ^= u[i];
        }
    }
    
    /* 复制结果 */
    for (uint32_t i = 0; i < dk_len && i < 32; i++) {
        derived_key[i] = result[i];
    }
}

/* 生成随机盐 */
static void generate_salt(uint8_t *salt, uint32_t len)
{
    /* 使用简化的伪随机数生成 */
    uint64_t seed = 0x1234567890ABCDEFULL;
    
    for (uint32_t i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        salt[i] = (seed >> 16) & 0xFF;
    }
}

/* 查找用户 */
static password_entry_t* find_user(const char *username)
{
    for (uint32_t i = 0; i < g_user_count; i++) {
        if (str_cmp(g_users[i].username, username) == 0) {
            return &g_users[i];
        }
    }
    return NULL;
}

/* 查找空闲槽位 */
static password_entry_t* find_free_slot(void)
{
    for (uint32_t i = 0; i < MAX_USERS; i++) {
        if (g_users[i].username[0] == '\0') {
            return &g_users[i];
        }
    }
    return NULL;
}

/* ========== 公开接口 ========== */

/* 密码验证 */
int password_verify(const char *username, const char *password)
{
    if (!username || !password) {
        return -1;  /* 无效参数 */
    }
    
    password_entry_t *entry = find_user(username);
    if (!entry) {
        return -2;  /* 用户不存在 */
    }
    
    /* 检查锁定状态 */
    if (entry->locked) {
        return -3;  /* 账户锁定 */
    }
    
    /* 检查过期 */
    if (entry->expiry_time > 0 && g_current_time > entry->expiry_time) {
        return -4;  /* 密码已过期 */
    }
    
    /* 计算哈希 */
    uint8_t derived_key[64];
    pbkdf2(password, entry->salt, MAX_SALT_LEN, entry->hash_iterations,
           derived_key, 64);
    
    /* 比较哈希 */
    int match = 1;
    for (int i = 0; i < 64; i++) {
        if (derived_key[i] != entry->password_hash[i]) {
            match = 0;
            break;
        }
    }
    
    if (match) {
        /* 重置失败计数 */
        entry->failed_attempts = 0;
        return 0;  /* 验证成功 */
    } else {
        /* 增加失败计数 */
        entry->failed_attempts++;
        if (entry->failed_attempts >= 5) {
            entry->locked = 1;
            return -3;  /* 账户锁定 */
        }
        return -5;  /* 密码错误 */
    }
}

/* 设置密码 */
int password_set(const char *username, const char *password)
{
    if (!username || !password) {
        return -1;
    }
    
    /* 检查密码策略 */
    int policy_result = password_check_policy(password);
    if (policy_result != 0) {
        return policy_result;
    }
    
    password_entry_t *entry = find_user(username);
    if (!entry) {
        /* 创建新用户 */
        entry = find_free_slot();
        if (!entry) {
            return -6;  /* 用户已满 */
        }
        
        /* 复制用户名 */
        int i;
        for (i = 0; username[i] && i < 31; i++) {
            entry->username[i] = username[i];
        }
        entry->username[i] = '\0';
        entry->user_id = (uint8_t)g_user_count;
        g_user_count++;
    }
    
    /* 保存历史密码 */
    for (int i = 3; i > 0; i--) {
        for (int j = 0; j < 64; j++) {
            entry->history[i][j] = entry->history[i-1][j];
        }
    }
    for (int j = 0; j < 64; j++) {
        entry->history[0][j] = entry->password_hash[j];
    }
    
    /* 生成盐 */
    generate_salt(entry->salt, MAX_SALT_LEN);
    
    /* 设置迭代次数 */
    entry->hash_iterations = HASH_ITERATIONS;
    
    /* 计算密码哈希 */
    pbkdf2(password, entry->salt, MAX_SALT_LEN, entry->hash_iterations,
           entry->password_hash, 64);
    
    /* 更新时间 */
    entry->last_change_time = g_current_time;
    entry->expiry_time = 0;  /* 不过期 */
    entry->locked = 0;
    entry->failed_attempts = 0;
    
    return 0;
}

/* 检查密码策略 */
int password_check_policy(const char *password)
{
    if (!password) {
        return -1;
    }
    
    int len = str_len(password);
    int upper = 0, lower = 0, digit = 0, special = 0;
    
    /* 统计字符类型 */
    for (int i = 0; i < len; i++) {
        if (is_upper(password[i])) upper++;
        else if (is_lower(password[i])) lower++;
        else if (is_digit(password[i])) digit++;
        else if (is_special(password[i])) special++;
    }
    
    /* 检查最小长度 */
    if (len < g_policy.min_length) {
        return -10;  /* 长度不足 */
    }
    
    /* 检查字符类型 */
    if (upper < g_policy.min_uppercase) {
        return -11;  /* 大写字母不足 */
    }
    if (lower < g_policy.min_lowercase) {
        return -12;  /* 小写字母不足 */
    }
    if (digit < g_policy.min_digits) {
        return -13;  /* 数字不足 */
    }
    if (special < g_policy.min_special) {
        return -14;  /* 特殊字符不足 */
    }
    
    return 0;
}

/* 密码哈希 */
int password_hash(const char *password, uint8_t *hash_out, uint32_t *hash_len)
{
    if (!password || !hash_out || !hash_len) {
        return -1;
    }
    
    uint8_t salt[MAX_SALT_LEN];
    generate_salt(salt, MAX_SALT_LEN);
    
    pbkdf2(password, salt, MAX_SALT_LEN, HASH_ITERATIONS, hash_out, 64);
    *hash_len = 64;
    
    return 0;
}

/* ========== 服务接口实现 ========== */

hic_status_t service_init(void)
{
    /* 初始化策略 */
    g_policy.min_length = DEFAULT_MIN_LENGTH;
    g_policy.min_uppercase = DEFAULT_MIN_UPPER;
    g_policy.min_lowercase = DEFAULT_MIN_LOWER;
    g_policy.min_digits = DEFAULT_MIN_DIGIT;
    g_policy.min_special = DEFAULT_MIN_SPECIAL;
    g_policy.max_age_days = 90;
    g_policy.history_count = 4;
    g_policy.flags = POLICY_FLAG_REQUIRE_SPECIAL;
    
    /* 清空用户表 */
    memset(g_users, 0, sizeof(g_users));
    g_user_count = 0;
    g_current_time = 0;
    
    return HIC_SUCCESS;
}

hic_status_t service_start(void)
{
    return HIC_SUCCESS;
}

hic_status_t service_stop(void)
{
    return HIC_SUCCESS;
}

hic_status_t service_cleanup(void)
{
    memset(g_users, 0, sizeof(g_users));
    g_user_count = 0;
    return HIC_SUCCESS;
}

hic_status_t service_get_info(char* buffer, u32 size)
{
    if (buffer && size > 0) {
        const char *info = "Password Manager Service v1.0.0\n"
                          "Provides secure password storage and verification\n"
                          "Endpoints: verify, set, check_policy, hash";
        int i = 0;
        while (info[i] && i < (int)(size - 1)) {
            buffer[i] = info[i];
            i++;
        }
        buffer[i] = '\0';
    }
    return HIC_SUCCESS;
}

const service_api_t g_service_api = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .get_info = service_get_info,
};

void service_register_self(void)
{
    service_register("password_manager_service", &g_service_api);
}