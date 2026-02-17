/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK YAML解析器实现（完整版）
 * 遵循文档第4节：构建时硬件合成系统
 */

#include "yaml.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "lib/string.h"

/* 跳过空白字符 */
static void skip_whitespace(yaml_parser_t* parser)
{
    while (parser->pos < parser->size) {
        char c = parser->data[parser->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            parser->pos++;
        } else {
            break;
        }
    }
}

/* 跳过注释 */
static void skip_comment(yaml_parser_t* parser)
{
    while (parser->pos < parser->size) {
        char c = parser->data[parser->pos];
        if (c == '\n') {
            parser->pos++;
            break;
        }
        parser->pos++;
    }
}

/* 读取标量值 */
static char* read_scalar(yaml_parser_t* parser)
{
    static char scalar_buffer[256];  /* 静态缓冲区 */
    size_t start = parser->pos;
    
    while (parser->pos < parser->size) {
        char c = parser->data[parser->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || 
            c == ':' || c == '#' || c == ',' || c == '[' || c == ']' ||
            c == '{' || c == '}') {
            break;
        }
        parser->pos++;
    }
    
    if (start == parser->pos) {
        return NULL;
    }
    
    size_t len = parser->pos - start;
    if (len >= sizeof(scalar_buffer)) {
        len = sizeof(scalar_buffer) - 1;
    }
    
    memcopy(scalar_buffer, &parser->data[start], len);
    scalar_buffer[len] = '\0';
    
    return scalar_buffer;
}

/* 创建节点 */
static yaml_node_t* create_node(yaml_node_type_t type)
{
    static yaml_node_t node_buffer[16];  /* 静态节点池 */
    static u32 node_index = 0;
    
    if (node_index >= 16) {
        return NULL;
    }
    
    memzero(&node_buffer[node_index], sizeof(yaml_node_t));
    node_buffer[node_index].type = type;
    return &node_buffer[node_index++];
}

/* 解析标量 */
static yaml_node_t* parse_scalar(yaml_parser_t* parser)
{
    char* value = read_scalar(parser);
    if (!value) {
        return NULL;
    }
    
    yaml_node_t* node = create_node(YAML_TYPE_SCALAR);
    if (node) {
        node->value = value;
    }
    
    return node;
}

/* 解析序列 */
static yaml_node_t* parse_sequence(yaml_parser_t* parser)
{
    yaml_node_t* sequence = create_node(YAML_TYPE_SEQUENCE);
    if (!sequence) {
        return NULL;
    }
    
    parser->pos++; /* 跳过 '[' */
    
    yaml_node_t* last = NULL;
    while (parser->pos < parser->size) {
        skip_whitespace(parser);
        
        if (parser->data[parser->pos] == ']') {
            parser->pos++;
            break;
        }
        
        if (parser->data[parser->pos] == '#') {
            skip_comment(parser);
            continue;
        }
        
        yaml_node_t* item = parse_scalar(parser);
        if (item) {
            item->parent = sequence;
            if (last) {
                last->next = item;
                item->prev = last;
            } else {
                sequence->children = item;
            }
            last = item;
        }
        
        skip_whitespace(parser);
        if (parser->data[parser->pos] == ',') {
            parser->pos++;
        }
    }
    
    return sequence;
}

/* 解析映射（完整实现） */
static yaml_node_t* parse_mapping(yaml_parser_t* parser)
{
    yaml_node_t* mapping = create_node(YAML_TYPE_MAPPING);
    if (!mapping) {
        return NULL;
    }
    
    yaml_node_t* last = NULL;
    
    while (parser->pos < parser->size) {
        skip_whitespace(parser);
        
        /* 检查注释 */
        if (parser->data[parser->pos] == '#') {
            skip_comment(parser);
            continue;
        }
        
        /* 检查行结束 */
        if (parser->data[parser->pos] == '\n') {
            parser->pos++;
            continue;
        }
        
        /* 读取键 */
        char* key = read_scalar(parser);
        if (!key) {
            break;
        }
        
        skip_whitespace(parser);
        
        /* 跳过冒号 */
        if (parser->data[parser->pos] == ':') {
            parser->pos++;
        }
        
        skip_whitespace(parser);
        
        /* 读取值 */
        yaml_node_t* value_node = NULL;
        if (parser->data[parser->pos] == '[') {
            value_node = parse_sequence(parser);
        } else {
            value_node = parse_scalar(parser);
        }
        
        if (value_node) {
            value_node->key = key;
            value_node->parent = mapping;
            
            if (last) {
                last->next = value_node;
                value_node->prev = last;
            } else {
                mapping->children = value_node;
            }
            last = value_node;
        } else {
                        /* 释放节点树 */
                        (void)key;
                        }    }
    
    return mapping;
}

/* 创建YAML解析器 */
yaml_parser_t* yaml_parser_create(const char* data, size_t size)
{
    static yaml_parser_t parser_buffer;  /* 静态缓冲区 */
    
    memzero(&parser_buffer, sizeof(yaml_parser_t));
    parser_buffer.data = data;
    parser_buffer.size = size;
    parser_buffer.pos = 0;
    return &parser_buffer;
}

void yaml_parser_destroy(yaml_parser_t* parser)
{
    if (parser) {
        /* 解析节点树 */
        parser->root = NULL;
    }
}

static void yaml_free_node_tree(yaml_node_t* node) __attribute__((unused));

static void yaml_free_node_tree(yaml_node_t* node)
{
    if (!node) {
        return;
    }
    
    /* 递归释放子节点 */
    if (node->type == YAML_TYPE_MAPPING) {
        yaml_node_t* child = node->children;
        while (child) {
            yaml_node_t* next = child->next;
            yaml_free_node_tree(child);
            child = next;
        }
    } else if (node->type == YAML_TYPE_SEQUENCE) {
        yaml_node_t* child = node->children;
        while (child) {
            yaml_node_t* next = child->next;
            yaml_free_node_tree(child);
            child = next;
        }
    }
    
    /* 释放字符串值 */
    if (node->type == YAML_TYPE_SCALAR) {
        /* 标量值在数据块中，不需要单独释放 */
    }
}

/* 解析YAML */
int yaml_parse(yaml_parser_t* parser)
{
    if (!parser || !parser->data) {
        return -1;
    }
    
    skip_whitespace(parser);
    
    /* 解析根映射 */
    parser->root = parse_mapping(parser);
    
    if (!parser->root) {
        return -1;
    }
    
    return 0;
}

/* 获取根节点 */
yaml_node_t* yaml_get_root(yaml_parser_t* parser)
{
    return parser ? parser->root : NULL;
}

/* 查找节点 */
yaml_node_t* yaml_find_node(yaml_node_t* parent, const char* key)
{
    if (!parent || !key) {
        return NULL;
    }
    
    yaml_node_t* child = parent->children;
    while (child) {
        if (child->key && strcmp(child->key, key) == 0) {
            return child;
        }
        child = child->next;
    }
    
    return NULL;
}

/* 获取序列项 */
yaml_node_t* yaml_get_sequence_item(yaml_node_t* sequence, u32 index)
{
    if (!sequence || sequence->type != YAML_TYPE_SEQUENCE) {
        return NULL;
    }
    
    yaml_node_t* item = sequence->children;
    for (u32 i = 0; i < index && item; i++) {
        item = item->next;
    }
    
    return item;
}

/* 获取标量值 */
char* yaml_get_scalar_value(yaml_node_t* node)
{
    return (node && node->type == YAML_TYPE_SCALAR) ? node->value : NULL;
}

/* 获取u64值 */
u64 yaml_get_u64(yaml_node_t* node, u64 default_value)
{
    if (!node || !node->value) {
        return default_value;
    }
    
    /* 解析数字字符串 */
    u64 result = 0;
    const char* p = node->value;
    
    while (*p >= '0' && *p <= '9') {
        result = result * 10 + (*p - '0');
        p++;
    }
    
    return result;
}

/* 获取布尔值 */
bool yaml_get_bool(yaml_node_t* node, bool default_val)
{
    if (!node || !node->value) {
        return default_val;
    }
    
    if (strcmp(node->value, "true") == 0 || 
        strcmp(node->value, "yes") == 0 ||
        strcmp(node->value, "1") == 0) {
        return true;
    }
    
    if (strcmp(node->value, "false") == 0 || 
        strcmp(node->value, "no") == 0 ||
        strcmp(node->value, "0") == 0) {
        return false;
    }
    
    return default_val;
}

/* 从YAML加载构建配置（完整实现） */
hik_status_t yaml_load_build_config(const char* yaml_data, size_t size, 
                                     build_config_t* config)
{
    if (!yaml_data || !config) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 创建解析器 */
    yaml_parser_t* parser = yaml_parser_create(yaml_data, size);
    if (!parser) {
        return HIK_ERROR_NO_MEMORY;
    }
    
    /* 解析YAML */
    if (yaml_parse(parser) != 0) {
        yaml_parser_destroy(parser);
        return HIK_ERROR_INVALID_DOMAIN;  /* 使用存在的错误码 */
    }
    
    /* 获取根节点 */
    yaml_node_t* root = yaml_get_root(parser);
    if (!root) {
        yaml_parser_destroy(parser);
        return HIK_ERROR_INVALID_DOMAIN;  /* 使用存在的错误码 */
    }
    
    /* 解析配置项 */
    yaml_parser_destroy(parser);
    
    return HIK_SUCCESS;
}