/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 密码管理服务
 * 提供安全的密码存储、验证和管理功能
 */

#ifndef PASSWORD_MANAGER_SERVICE_H
#define PASSWORD_MANAGER_SERVICE_H

#include "../../Core-0/types.h"
#include "../../Core-0/lib/string.h"

/* 密码哈希长度（SHA-384） */
#define PASSWORD_HASH_SIZE 48
#define PASSWORD_SALT_SIZE 16
#define PASSWORD_MAX_LENGTH 128

/* 密码策略配置 */
typedef struct password_policy {
    u32 min_length;           /* 最小长度 */
    u32 max_length;           /* 最大长度 */
    bool require_uppercase;   /* 要求大写字母 */
    bool require_lowercase;   /* 要求小写字母 */
    bool require_digit;       /* 要求数字 */
    bool require_special;     /* 要求特殊字符 */
    u32 max_age_days;         /* 密码最大使用天数 */
} password_policy_t;

/* 密码哈希存储 */
typedef struct password_hash {
    u8 hash[PASSWORD_HASH_SIZE];
    u8 salt[PASSWORD_SALT_SIZE];
    u64 timestamp;            /* 创建时间戳 */
    u32 attempts;             /* 尝试次数 */
    u32 max_attempts;         /* 最大尝试次数 */
} password_hash_t;

/* 密码管理器状态 */
typedef struct password_manager {
    password_hash_t admin_password;
    password_hash_t user_password;
    password_policy_t policy;
    bool initialized;
} password_manager_t;

/* 服务API */
/**
 * 初始化密码管理器
 */
hic_status_t password_manager_init(void);

/**
 * 验证密码
 * @param password 输入密码
 * @return 验证成功返回HIC_SUCCESS
 */
hic_status_t password_verify(const char* password);

/**
 * 设置密码
 * @param password 新密码
 * @param is_admin 是否为管理员密码
 * @return 设置成功返回HIC_SUCCESS
 */
hic_status_t password_set(const char* password, bool is_admin);

/**
 * 修改密码
 * @param old_password 旧密码
 * @param new_password 新密码
 * @return 修改成功返回HIC_SUCCESS
 */
hic_status_t password_change(const char* old_password, const char* new_password);

/**
 * 检查密码强度
 * @param password 要检查的密码
 * @param strength 输出强度（0-100）
 * @return 检查成功返回HIC_SUCCESS
 */
hic_status_t password_check_strength(const char* password, u32* strength);

/**
 * 获取密码策略
 * @param policy 输出策略
 * @return 获取成功返回HIC_SUCCESS
 */
hic_status_t password_get_policy(password_policy_t* policy);

/**
 * 设置密码策略
 * @param policy 新策略
 * @return 设置成功返回HIC_SUCCESS
 */
hic_status_t password_set_policy(const password_policy_t* policy);

#endif /* PASSWORD_MANAGER_SERVICE_H */