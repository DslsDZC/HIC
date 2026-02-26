/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 配置管理服务实现
 */

#include "service.h"
#include "../include/service_api.h"
#include "../../Core-0/lib/mem.h"
#include "../../Core-0/lib/console.h"
#include "../../Core-0/lib/string.h"

/* 默认配置 */
static const char DEFAULT_PASSWORD[] = "admin123";

/* 全局配置解析器 */
static yaml_parser_t g_config_parser;
static bool g_config_loaded = false;

/* ============= YAML解析实现 ============= */

void yaml_skip_whitespace(yaml_parser_t* parser) {
    while (parser->pos < parser->size) {
        char c = parser->data[parser->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            parser->pos++;
        } else if (c == '#') {
            /* 跳过注释 */
            while (parser->pos < parser->size && parser->data[parser->pos] != '\n') {
                parser->pos++;
            }
        } else {
            break;
        }
    }
}

yaml_node_t* yaml_parse_scalar(yaml_parser_t* parser) {
    static char scalar_buffer[256];
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
    
    /* 创建节点 */
    yaml_node_t* node = (yaml_node_t*)mem_alloc(sizeof(yaml_node_t));
    if (node) {
        memzero(node, sizeof(yaml_node_t));
        node->type = YAML_TYPE_SCALAR;
        node->value = scalar_buffer;
    }
    
    return node;
}

yaml_node_t* yaml_parse_mapping(yaml_parser_t* parser) {
    yaml_node_t* mapping = (yaml_node_t*)mem_alloc(sizeof(yaml_node_t));
    if (!mapping) {
        return NULL;
    }
    
    memzero(mapping, sizeof(yaml_node_t));
    mapping->type = YAML_TYPE_MAPPING;
    
    yaml_node_t* last = NULL;
    
    while (parser->pos < parser->size) {
        yaml_skip_whitespace(parser);
        
        /* 检查注释 */
        if (parser->data[parser->pos] == '#') {
            while (parser->pos < parser->size && parser->data[parser->pos] != '\n') {
                parser->pos++;
            }
            continue;
        }
        
        /* 检查行结束 */
        if (parser->data[parser->pos] == '\n') {
            parser->pos++;
            continue;
        }
        
        /* 读取键 */
        char* key = (char*)yaml_parse_scalar(parser);
        if (!key) {
            break;
        }
        
        yaml_skip_whitespace(parser);
        
        /* 跳过冒号 */
        if (parser->data[parser->pos] == ':') {
            parser->pos++;
        }
        
        yaml_skip_whitespace(parser);
        
        /* 读取值 */
        yaml_node_t* value_node = NULL;
        if (parser->data[parser->pos] == '[') {
            parser->pos++;
            value_node = yaml_parse_sequence(parser);
        } else if (parser->data[parser->pos] == '{') {
            parser->pos++;
            value_node = yaml_parse_mapping(parser);
        } else {
            value_node = yaml_parse_scalar(parser);
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
        }
    }
    
    return mapping;
}

yaml_node_t* yaml_parse_sequence(yaml_parser_t* parser) {
    yaml_node_t* sequence = (yaml_node_t*)mem_alloc(sizeof(yaml_node_t));
    if (!sequence) {
        return NULL;
    }
    
    memzero(sequence, sizeof(yaml_node_t));
    sequence->type = YAML_TYPE_SEQUENCE;
    
    yaml_node_t* last = NULL;
    
    while (parser->pos < parser->size) {
        yaml_skip_whitespace(parser);
        
        if (parser->data[parser->pos] == ']') {
            parser->pos++;
            break;
        }
        
        yaml_node_t* item = yaml_parse_scalar(parser);
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
        
        yaml_skip_whitespace(parser);
        if (parser->data[parser->pos] == ',') {
            parser->pos++;
        }
    }
    
    return sequence;
}

yaml_node_t* yaml_find_node(yaml_node_t* root, const char* key) {
    if (!root || !key) {
        return NULL;
    }
    
    /* 检查是否是嵌套键 */
    char* dot = strchr(key, '.');
    if (dot) {
        /* 保存第一个键 */
        char first_key[128];
        u32 len = (u32)(dot - key);
        if (len >= sizeof(first_key)) {
            len = sizeof(first_key) - 1;
        }
        memcopy(first_key, key, len);
        first_key[len] = '\0';
        
        /* 查找第一个键 */
        yaml_node_t* child = root->children;
        while (child) {
            if (child->key && strcmp(child->key, first_key) == 0) {
                /* 递归查找剩余部分 */
                return yaml_find_node(child, dot + 1);
            }
            child = child->next;
        }
        
        return NULL;
    }
    
    /* 查找直接子节点 */
    yaml_node_t* child = root->children;
    while (child) {
        if (child->key && strcmp(child->key, key) == 0) {
            return child;
        }
        child = child->next;
    }
    
    return NULL;
}

/* ============= 公共API ============= */

hic_status_t config_service_init(void) {
    memzero(&g_config_parser, sizeof(yaml_parser_t));
    g_config_loaded = false;
    
    console_puts("[CONFIG] Config service initialized\n");
    return HIC_SUCCESS;
}

hic_status_t config_yaml_parse(const char* yaml_data, size_t size, yaml_parser_t* parser) {
    if (!yaml_data || !parser || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memzero(parser, sizeof(yaml_parser_t));
    parser->data = yaml_data;
    parser->size = size;
    parser->pos = 0;
    
    yaml_skip_whitespace(parser);
    parser->root = yaml_parse_mapping(parser);
    
    if (!parser->root) {
        return HIC_ERROR_PARSE_FAILED;
    }
    
    return HIC_SUCCESS;
}

hic_status_t config_yaml_get_string(yaml_parser_t* parser, const char* key, 
                                    char* value, u32 max_len) {
    if (!parser || !key || !value || max_len == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    yaml_node_t* node = yaml_find_node((yaml_node_t*)parser->root, key);
    if (!node || !node->value) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    u32 len = strlen(node->value);
    if (len >= max_len) {
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    strcpy(value, node->value);
    return HIC_SUCCESS;
}

u64 config_yaml_get_u64(yaml_parser_t* parser, const char* key, u64 default_value) {
    if (!parser || !key) {
        return default_value;
    }
    
    yaml_node_t* node = yaml_find_node((yaml_node_t*)parser->root, key);
    if (!node || !node->value) {
        return default_value;
    }
    
    /* 解析数字字符串 */
    u64 result = 0;
    const char* p = node->value;
    
    while (*p >= '0' && *p <= '9') {
        result = result * 10 + (u64)(*p - '0');
        p++;
    }
    
    return result;
}

bool config_yaml_get_bool(yaml_parser_t* parser, const char* key, bool default_val) {
    if (!parser || !key) {
        return default_val;
    }
    
    yaml_node_t* node = yaml_find_node((yaml_node_t*)parser->root, key);
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

hic_status_t config_get_default_password(char* password, u32 max_len) {
    if (!password || max_len == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* TODO: 从YAML配置读取密码 */
    /* 临时使用硬编码的默认密码 */
    u32 len = strlen(DEFAULT_PASSWORD);
    if (len >= max_len) {
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    strcpy(password, DEFAULT_PASSWORD);
    return HIC_SUCCESS;
}

hic_status_t config_load_file(const char* path, yaml_parser_t* parser) {
    (void)path;
    (void)parser;
    /* TODO: 实现文件读取 */
    return HIC_ERROR_NOT_IMPLEMENTED;
}

void config_yaml_destroy(yaml_parser_t* parser) {
    if (parser && parser->root) {
        /* TODO: 递归释放节点树 */
        parser->root = NULL;
    }
}

/* ============= 服务API ============= */

static hic_status_t service_init(void) {
    return config_service_init();
}

static hic_status_t service_start(void) {
    console_puts("[CONFIG] Config service started\n");
    return HIC_SUCCESS;
}

static hic_status_t service_stop(void) {
    console_puts("[CONFIG] Config service stopped\n");
    return HIC_SUCCESS;
}

static hic_status_t service_cleanup(void) {
    config_yaml_destroy(&g_config_parser);
    g_config_loaded = false;
    return HIC_SUCCESS;
}

static hic_status_t service_get_info(char* buffer, u32 buffer_size) {
    if (!buffer || buffer_size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    const char* info = "Config Service v1.0.0 - "
                       "Provides YAML configuration parsing and management";
    u32 len = strlen(info);
    
    if (len >= buffer_size) {
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    strcpy(buffer, info);
    return HIC_SUCCESS;
}

/* 服务API表 */
const service_api_t g_service_api = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .get_info = service_get_info,
};

/* 服务注册函数 */
void service_register_self(void) {
    service_register("config_service", &g_service_api);
}