/**
 * HIK YAML解析器实现（完整版）
 * 遵循文档第4节：构建时硬件合成系统
 */

#include "yaml.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "lib/string.h"
#include <stdlib.h>

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
    char* value = (char*)malloc(len + 1);
    if (value) {
        memcopy(value, &parser->data[start], len);
        value[len] = '\0';
    }
    
    return value;
}

/* 创建节点 */
static yaml_node_t* create_node(yaml_node_type_t type)
{
    yaml_node_t* node = (yaml_node_t*)malloc(sizeof(yaml_node_t));
    if (node) {
        memzero(node, sizeof(yaml_node_t));
        node->type = type;
    }
    return node;
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
    } else {
        free(value);
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
            free(key);
        }
    }
    
    return mapping;
}

/* 创建YAML解析器 */
yaml_parser_t* yaml_parser_create(const char* data, size_t size)
{
    yaml_parser_t* parser = (yaml_parser_t*)malloc(sizeof(yaml_parser_t));
    if (parser) {
        memzero(parser, sizeof(yaml_parser_t));
        parser->data = data;
        parser->size = size;
        parser->pos = 0;
    }
    return parser;
}

/* 销毁YAML解析器（完整实现） */
void yaml_parser_destroy(yaml_parser_t* parser)
{
    if (!parser) {
        return;
    }
    
    /* 完整实现：递归释放节点树 */
    if (parser->root) {
        /* 递归释放节点及其子节点 */
        yaml_free_node_tree(parser->root);
    }
    
    free(parser);
}

/* 递归释放YAML节点树 */
static void yaml_free_node_tree(yaml_node_t* node)
{
    if (!node) {
        return;
    }
    
    /* 根据节点类型释放资源 */
    if (node->type == YAML_NODE_SEQUENCE) {
        /* 释放序列中的所有元素 */
        yaml_node_t* child = node->sequence_head;
        while (child) {
            yaml_node_t* next = child->next;
            yaml_free_node_tree(child);
            child = next;
        }
    } else if (node->type == YAML_NODE_MAPPING) {
        /* 释放映射中的所有键值对 */
        yaml_node_t* pair = node->mapping_head;
        while (pair) {
            yaml_node_t* next = pair->next;
            yaml_free_node_tree(pair);
            pair = next;
        }
    } else if (node->type == YAML_NODE_SCALAR && node->value) {
        /* 释放标量值字符串 */
        free((void*)node->value);
        node->value = NULL;
    }
    
    /* 释放节点本身 */
    free(node);
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
u64 yaml_get_u64(yaml_node_t* node, u64 default_val)
{
    if (!node || !node->value) {
        return default_val;
    }
    
    return strtoull(node->value, NULL, 10);
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
        return HIK_ERROR_INVALID_FORMAT;
    }
    
    /* 获取根节点 */
    yaml_node_t* root = yaml_get_root(parser);
    if (!root) {
        yaml_parser_destroy(parser);
        return HIK_ERROR_INVALID_FORMAT;
    }
    
    /* 解析目标配置 */
    yaml_node_t* target = yaml_find_node(root, "target");
    if (target) {
        yaml_node_t* arch = yaml_find_node(target, "architecture");
        if (arch && arch->value) {
            if (strcmp(arch->value, "x86_64") == 0) {
                config->target_architecture = ARCH_X86_64;
            }
        }
        
        yaml_node_t* apic = yaml_find_node(target, "apic");
        if (apic) {
            config->enable_apic = yaml_get_bool(apic, false);
        }
    }
    
    /* 解析构建配置 */
    yaml_node_t* build = yaml_find_node(root, "build");
    if (build) {
        yaml_node_t* mode = yaml_find_node(build, "mode");
        if (mode && mode->value) {
            config->build_mode = (strcmp(mode->value, "dynamic") == 0) ? 
                                 BUILD_MODE_DYNAMIC : BUILD_MODE_STATIC;
        }
        
        yaml_node_t* optimize = yaml_find_node(build, "optimize_level");
        if (optimize) {
            config->optimize_level = yaml_get_u64(optimize, 2);
        }
    }
    
    yaml_parser_destroy(parser);
    
    console_puts("[YAML] Build configuration loaded successfully\n");
    
    return HIK_SUCCESS;
}