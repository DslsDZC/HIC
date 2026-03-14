/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 配置管理服务
 * 
 * 提供系统配置的读取、修改和持久化功能
 * 支持键值对存储和配置文件管理
 */

#include "service.h"
#include <string.h>

/* ========== 常量定义 ========== */

#define MAX_CONFIG_ENTRIES  64
#define MAX_KEY_LEN         32
#define MAX_VALUE_LEN       128
#define MAX_CONFIG_FILE     4096

/* ========== 配置条目结构 ========== */

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    uint8_t type;           /* 值类型 */
    uint8_t flags;          /* 标志 */
    uint8_t reserved[2];
} config_entry_t;

/* 值类型 */
#define CONFIG_TYPE_STRING   0
#define CONFIG_TYPE_INT      1
#define CONFIG_TYPE_BOOL     2
#define CONFIG_TYPE_FLOAT    3

/* 标志 */
#define CONFIG_FLAG_READONLY  (1U << 0)
#define CONFIG_FLAG_PERSIST   (1U << 1)
#define CONFIG_FLAG_SECRET    (1U << 2)

/* ========== 全局状态 ========== */

static config_entry_t g_config[MAX_CONFIG_ENTRIES];
static uint32_t g_config_count;
static uint8_t g_config_dirty;
static char g_config_file_path[256];

/* ========== 前置声明 ========== */

static int config_set(const char *key, const char *value, int type, int flags);

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

static int str_ncmp(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static void str_cpy(char *dst, const char *src, int max_len)
{
    int i = 0;
    while (src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* 字符串转整数 */
static int str_to_int(const char *s, int *value)
{
    int result = 0;
    int sign = 1;
    
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    while (is_digit(*s)) {
        result = result * 10 + (*s - '0');
        s++;
    }
    
    *value = result * sign;
    return 0;
}

/* 整数转字符串 */
static void int_to_str(int value, char *buf, int max_len)
{
    int i = 0;
    int neg = 0;
    
    if (value < 0) {
        neg = 1;
        value = -value;
    }
    
    /* 从后向前生成 */
    char temp[16];
    int j = 0;
    
    if (value == 0) {
        temp[j++] = '0';
    } else {
        while (value > 0 && j < 15) {
            temp[j++] = '0' + (value % 10);
            value /= 10;
        }
    }
    
    if (neg && i < max_len - 1) {
        buf[i++] = '-';
    }
    
    while (j > 0 && i < max_len - 1) {
        buf[i++] = temp[--j];
    }
    buf[i] = '\0';
}

/* 字符串转布尔 */
static int str_to_bool(const char *s, int *value)
{
    if (str_cmp(s, "true") == 0 || str_cmp(s, "1") == 0 ||
        str_cmp(s, "yes") == 0 || str_cmp(s, "on") == 0) {
        *value = 1;
        return 0;
    }
    if (str_cmp(s, "false") == 0 || str_cmp(s, "0") == 0 ||
        str_cmp(s, "no") == 0 || str_cmp(s, "off") == 0) {
        *value = 0;
        return 0;
    }
    return -1;
}

/* 查找配置项 */
static config_entry_t* find_entry(const char *key)
{
    for (uint32_t i = 0; i < g_config_count; i++) {
        if (str_cmp(g_config[i].key, key) == 0) {
            return &g_config[i];
        }
    }
    return NULL;
}

/* 查找空闲槽位 */
static config_entry_t* find_free_slot(void)
{
    for (uint32_t i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_config[i].key[0] == '\0') {
            return &g_config[i];
        }
    }
    return NULL;
}

/* 解析配置行 */
static int parse_config_line(const char *line)
{
    const char *p = line;
    
    /* 跳过前导空格和注释 */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == '\n' || *p == '\0') {
        return 0;  /* 忽略 */
    }
    
    /* 解析键 */
    char key[MAX_KEY_LEN];
    int key_len = 0;
    while (*p && *p != '=' && *p != ' ' && *p != '\t' && key_len < MAX_KEY_LEN - 1) {
        key[key_len++] = *p++;
    }
    key[key_len] = '\0';
    
    /* 跳过空格和等号 */
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') {
        return -1;  /* 格式错误 */
    }
    p++;
    while (*p == ' ' || *p == '\t') p++;
    
    /* 解析值 */
    char value[MAX_VALUE_LEN];
    int value_len = 0;
    while (*p && *p != '\n' && *p != '#' && value_len < MAX_VALUE_LEN - 1) {
        if (*p != ' ' && *p != '\t') {
            value[value_len++] = *p;
        }
        p++;
    }
    value[value_len] = '\0';
    
    /* 设置配置 */
    return config_set(key, value, CONFIG_TYPE_STRING, CONFIG_FLAG_PERSIST);
}

/* ========== 公开接口 ========== */

/* 获取配置 */
int config_get(const char *key, char *value, uint32_t value_size)
{
    if (!key || !value || value_size == 0) {
        return -1;
    }
    
    config_entry_t *entry = find_entry(key);
    if (!entry) {
        return -2;  /* 配置项不存在 */
    }
    
    /* 复制值 */
    int i;
    for (i = 0; entry->value[i] && i < (int)(value_size - 1); i++) {
        value[i] = entry->value[i];
    }
    value[i] = '\0';
    
    return 0;
}

/* 获取整数配置 */
int config_get_int(const char *key, int *value)
{
    char buf[MAX_VALUE_LEN];
    if (config_get(key, buf, sizeof(buf)) != 0) {
        return -1;
    }
    return str_to_int(buf, value);
}

/* 获取布尔配置 */
int config_get_bool(const char *key, int *value)
{
    char buf[MAX_VALUE_LEN];
    if (config_get(key, buf, sizeof(buf)) != 0) {
        return -1;
    }
    return str_to_bool(buf, value);
}

/* 设置配置 */
int config_set(const char *key, const char *value, int type, int flags)
{
    if (!key || !value) {
        return -1;
    }
    
    int key_len = str_len(key);
    int value_len = str_len(value);
    
    if (key_len >= MAX_KEY_LEN || value_len >= MAX_VALUE_LEN) {
        return -2;  /* 键或值太长 */
    }
    
    config_entry_t *entry = find_entry(key);
    if (!entry) {
        /* 创建新条目 */
        entry = find_free_slot();
        if (!entry) {
            return -3;  /* 配置表已满 */
        }
        str_cpy(entry->key, key, MAX_KEY_LEN);
        g_config_count++;
    } else if (entry->flags & CONFIG_FLAG_READONLY) {
        return -4;  /* 只读配置 */
    }
    
    /* 设置值 */
    str_cpy(entry->value, value, MAX_VALUE_LEN);
    entry->type = (uint8_t)type;
    entry->flags = (uint8_t)flags;
    
    g_config_dirty = 1;
    
    return 0;
}

/* 保存配置到文件 */
int config_save(void)
{
    if (!g_config_dirty) {
        return 0;  /* 无需保存 */
    }
    
    /* 构建配置文件内容 */
    char buffer[MAX_CONFIG_FILE];
    int offset = 0;
    
    /* 写入文件头 */
    const char *header = "# HIC Configuration File\n# Auto-generated\n\n";
    while (*header && offset < MAX_CONFIG_FILE - 1) {
        buffer[offset++] = *header++;
    }
    
    /* 写入配置项 */
    for (uint32_t i = 0; i < g_config_count && offset < MAX_CONFIG_FILE - 128; i++) {
        config_entry_t *entry = &g_config[i];
        
        /* 只保存持久化配置 */
        if (!(entry->flags & CONFIG_FLAG_PERSIST)) {
            continue;
        }
        
        /* 写入键 */
        for (int j = 0; entry->key[j] && offset < MAX_CONFIG_FILE - 1; j++) {
            buffer[offset++] = entry->key[j];
        }
        
        /* 写入等号 */
        buffer[offset++] = '=';
        
        /* 写入值 */
        for (int j = 0; entry->value[j] && offset < MAX_CONFIG_FILE - 1; j++) {
            buffer[offset++] = entry->value[j];
        }
        
        /* 写入换行 */
        buffer[offset++] = '\n';
    }
    
    buffer[offset] = '\0';
    
    /* 写入文件 */
    extern int fat32_write_file(const char *path, const void *data, uint32_t size);
    if (fat32_write_file(g_config_file_path, buffer, offset) != 0) {
        return -5;  /* 写入失败 */
    }
    
    g_config_dirty = 0;
    return 0;
}

/* 从文件加载配置 */
int config_load(const char *path)
{
    char buffer[MAX_CONFIG_FILE];
    uint32_t bytes_read = 0;
    
    if (path) {
        str_cpy(g_config_file_path, path, sizeof(g_config_file_path));
    }
    
    /* 读取文件 */
    extern int fat32_read_file(const char *path, void *buffer, uint32_t buffer_size, uint32_t *bytes_read);
    if (fat32_read_file(g_config_file_path, buffer, sizeof(buffer) - 1, &bytes_read) != 0) {
        return -1;  /* 读取失败 */
    }
    
    buffer[bytes_read] = '\0';
    
    /* 解析每一行 */
    const char *line_start = buffer;
    const char *p = buffer;
    
    while (*p) {
        if (*p == '\n') {
            /* 处理一行 */
            char line[256];
            int line_len = p - line_start;
            if (line_len > 0 && line_len < 255) {
                for (int i = 0; i < line_len; i++) {
                    line[i] = line_start[i];
                }
                line[line_len] = '\0';
                parse_config_line(line);
            }
            line_start = p + 1;
        }
        p++;
    }
    
    /* 处理最后一行 */
    if (*line_start) {
        parse_config_line(line_start);
    }
    
    g_config_dirty = 0;
    return 0;
}

/* 删除配置 */
int config_delete(const char *key)
{
    if (!key) {
        return -1;
    }
    
    config_entry_t *entry = find_entry(key);
    if (!entry) {
        return -2;  /* 配置项不存在 */
    }
    
    if (entry->flags & CONFIG_FLAG_READONLY) {
        return -3;  /* 只读配置 */
    }
    
    /* 清空条目 */
    entry->key[0] = '\0';
    entry->value[0] = '\0';
    entry->type = 0;
    entry->flags = 0;
    
    if (g_config_count > 0) {
        g_config_count--;
    }
    
    g_config_dirty = 1;
    return 0;
}

/* 列出所有配置 */
int config_list(char *buffer, uint32_t buffer_size, uint32_t *count)
{
    if (!buffer || !count) {
        return -1;
    }
    
    uint32_t offset = 0;
    *count = g_config_count;
    
    for (uint32_t i = 0; i < g_config_count && offset < buffer_size - 64; i++) {
        config_entry_t *entry = &g_config[i];
        
        /* 写入键 */
        for (int j = 0; entry->key[j] && offset < buffer_size - 2; j++) {
            buffer[offset++] = entry->key[j];
        }
        
        buffer[offset++] = '=';
        
        /* 写入值（如果是敏感信息则隐藏） */
        if (entry->flags & CONFIG_FLAG_SECRET) {
            buffer[offset++] = '*';
            buffer[offset++] = '*';
            buffer[offset++] = '*';
        } else {
            for (int j = 0; entry->value[j] && offset < buffer_size - 2; j++) {
                buffer[offset++] = entry->value[j];
            }
        }
        
        buffer[offset++] = '\n';
    }
    
    buffer[offset] = '\0';
    return 0;
}

/* ========== 默认配置 ========== */

static void load_default_config(void)
{
    /* 系统默认配置 */
    config_set("system.name", "HIC System", CONFIG_TYPE_STRING, CONFIG_FLAG_PERSIST);
    config_set("system.version", "0.1.0", CONFIG_TYPE_STRING, CONFIG_FLAG_READONLY | CONFIG_FLAG_PERSIST);
    config_set("system.debug", "false", CONFIG_TYPE_BOOL, CONFIG_FLAG_PERSIST);
    config_set("system.log_level", "1", CONFIG_TYPE_INT, CONFIG_FLAG_PERSIST);
    
    /* 安全配置 */
    config_set("security.password_min_length", "8", CONFIG_TYPE_INT, CONFIG_FLAG_PERSIST);
    config_set("security.login_attempts", "5", CONFIG_TYPE_INT, CONFIG_FLAG_PERSIST);
    config_set("security.session_timeout", "3600", CONFIG_TYPE_INT, CONFIG_FLAG_PERSIST);
    
    /* 网络配置 */
    config_set("network.enabled", "false", CONFIG_TYPE_BOOL, CONFIG_FLAG_PERSIST);
    config_set("network.dhcp", "true", CONFIG_TYPE_BOOL, CONFIG_FLAG_PERSIST);
    
    g_config_dirty = 0;
}

/* ========== 服务接口实现 ========== */

hic_status_t service_init(void)
{
    memset(g_config, 0, sizeof(g_config));
    g_config_count = 0;
    g_config_dirty = 0;
    
    str_cpy(g_config_file_path, "/config/system.cfg", sizeof(g_config_file_path));
    
    /* 加载默认配置 */
    load_default_config();
    
    return HIC_SUCCESS;
}

hic_status_t service_start(void)
{
    /* 尝试加载配置文件 */
    config_load(g_config_file_path);
    return HIC_SUCCESS;
}

hic_status_t service_stop(void)
{
    /* 保存配置 */
    config_save();
    return HIC_SUCCESS;
}

hic_status_t service_cleanup(void)
{
    config_save();
    memset(g_config, 0, sizeof(g_config));
    g_config_count = 0;
    return HIC_SUCCESS;
}

hic_status_t service_get_info(char* buffer, u32 size)
{
    if (buffer && size > 0) {
        const char *info = "Config Service v1.0.0\n"
                          "Provides system configuration management\n"
                          "Endpoints: get, set, save, load, delete, list";
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
    service_register("config_service", &g_service_api);
}