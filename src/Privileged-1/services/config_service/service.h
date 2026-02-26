/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 配置管理服务
 * 提供YAML配置解析和管理功能
 */

#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include "../../Core-0/types.h"

/* 最大密码长度 */
#define MAX_PASSWORD_LENGTH 128

/* YAML解析器 */
typedef struct yaml_parser {
    const char* data;
    size_t size;
    size_t pos;
    void* root;
} yaml_parser_t;

/* YAML节点类型 */
typedef enum {
    YAML_TYPE_SCALAR,
    YAML_TYPE_SEQUENCE,
    YAML_TYPE_MAPPING
} yaml_node_type_t;

/* YAML节点 */
typedef struct yaml_node {
    yaml_node_type_t type;
    char* key;
    char* value;
    struct yaml_node* children;
    struct yaml_node* next;
    struct yaml_node* prev;
    struct yaml_node* parent;
} yaml_node_t;

/* ============= 配置管理API ============= */

/**
 * 初始化配置服务
 */
hic_status_t config_service_init(void);

/**
 * 解析YAML配置
 * @param yaml_data YAML数据
 * @param size 数据大小
 * @param parser 输出解析器
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t config_yaml_parse(const char* yaml_data, size_t size, yaml_parser_t* parser);

/**
 * 获取字符串值
 * @param parser YAML解析器
 * @param key 键（支持嵌套，如"security.password"）
 * @param value 输出值
 * @param max_len 最大长度
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t config_yaml_get_string(yaml_parser_t* parser, const char* key, 
                                    char* value, u32 max_len);

/**
 * 获取u64值
 * @param parser YAML解析器
 * @param key 键
 * @param default_value 默认值
 * @return 返回值
 */
u64 config_yaml_get_u64(yaml_parser_t* parser, const char* key, u64 default_value);

/**
 * 获取布尔值
 * @param parser YAML解析器
 * @param key 键
 * @param default_val 默认值
 * @return 返回值
 */
bool config_yaml_get_bool(yaml_parser_t* parser, const char* key, bool default_val);

/**
 * 获取默认密码
 * @param password 输出密码
 * @param max_len 最大长度
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t config_get_default_password(char* password, u32 max_len);

/**
 * 加载配置文件
 * @param path 文件路径
 * @param parser 输出解析器
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t config_load_file(const char* path, yaml_parser_t* parser);

/**
 * 释放解析器
 * @param parser YAML解析器
 */
void config_yaml_destroy(yaml_parser_t* parser);

/* ============= 内部函数 ============= */

/**
 * 查找节点
 * @param root 根节点
 * @param key 键
 * @return 节点指针，不存在返回NULL
 */
yaml_node_t* yaml_find_node(yaml_node_t* root, const char* key);

/**
 * 解析映射
 * @param parser YAML解析器
 * @return 节点指针
 */
yaml_node_t* yaml_parse_mapping(yaml_parser_t* parser);

/**
 * 解析标量
 * @param parser YAML解析器
 * @return 节点指针
 */
yaml_node_t* yaml_parse_scalar(yaml_parser_t* parser);

/**
 * 解析序列
 * @param parser YAML解析器
 * @return 节点指针
 */
yaml_node_t* yaml_parse_sequence(yaml_parser_t* parser);

/**
 * 跳过空白字符
 * @param parser YAML解析器
 */
void yaml_skip_whitespace(yaml_parser_t* parser);

#endif /* CONFIG_SERVICE_H */