/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK YAML解析器
 * 遵循文档第4节：构建时硬件合成系统
 * 解析platform.yaml配置文件
 */

#ifndef HIK_KERNEL_YAML_H
#define HIK_KERNEL_YAML_H

#include "types.h"
#include "build_config.h"

/* YAML节点类型 */
typedef enum {
    YAML_TYPE_SCALAR,      /* 标量值 */
    YAML_TYPE_SEQUENCE,    /* 序列（数组） */
    YAML_TYPE_MAPPING,     /* 映射（对象） */
    YAML_TYPE_NONE,        /* 无效节点 */
} yaml_node_type_t;

/* YAML节点 */
typedef struct yaml_node {
    yaml_node_type_t type;
    char* key;             /* 键（对于映射） */
    char* value;           /* 值（对于标量） */
    
    struct yaml_node* parent;
    struct yaml_node* children;
    struct yaml_node* next;
    struct yaml_node* prev;
} yaml_node_t;

/* YAML解析器 */
typedef struct {
    const char* data;
    size_t size;
    size_t pos;
    yaml_node_t* root;
    yaml_node_t* current;
    int error;
    char error_msg[256];
} yaml_parser_t;

/* YAML解析接口 */
yaml_parser_t* yaml_parser_create(const char* data, size_t size);
void yaml_parser_destroy(yaml_parser_t* parser);

/* 解析YAML */
int yaml_parse(yaml_parser_t* parser);

/* 查询接口 */
yaml_node_t* yaml_get_root(yaml_parser_t* parser);
yaml_node_t* yaml_find_node(yaml_node_t* parent, const char* key);
yaml_node_t* yaml_get_sequence_item(yaml_node_t* sequence, u32 index);
char* yaml_get_scalar_value(yaml_node_t* node);

/* 工具函数 */
u64 yaml_get_u64(yaml_node_t* node, u64 default_val);
bool yaml_get_bool(yaml_node_t* node, bool default_val);

/* 从YAML加载构建配置 */
hik_status_t yaml_load_build_config(const char* yaml_data, size_t size, 
                                     build_config_t* config);

#endif /* HIK_KERNEL_YAML_H */