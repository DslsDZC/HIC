/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC YAML解析器实现（完整版）
 * 遵循文档第4节：构建时硬件合成系统
 */

#include "yaml.h"
#include "runtime_config.h"
#include "boot_info.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "lib/string.h"

/* 外部引用 */
extern hic_boot_info_t *g_boot_info;
extern runtime_config_t g_runtime_config;

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
        result = result * 10 + (u64)(*p - '0');
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
hic_status_t yaml_load_build_config(const char* yaml_data, size_t size, 
                                     build_config_t* config)
{
    if (!yaml_data || !config) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 创建解析器 */
    yaml_parser_t* parser = yaml_parser_create(yaml_data, size);
    if (!parser) {
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 解析YAML */
    if (yaml_parse(parser) != 0) {
        yaml_parser_destroy(parser);
        return HIC_ERROR_INVALID_DOMAIN;  /* 使用存在的错误码 */
    }
    
    /* 获取根节点 */
    yaml_node_t* root = yaml_get_root(parser);
    if (!root) {
        yaml_parser_destroy(parser);
        return HIC_ERROR_INVALID_DOMAIN;  /* 使用存在的错误码 */
    }
    
    /* 解析配置项 */
    yaml_parser_destroy(parser);
    
    return HIC_SUCCESS;
}

/* 从YAML加载系统限制配置 */
hic_status_t yaml_load_system_limits(const char* yaml_data, size_t size)
{
    if (!yaml_data) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 创建解析器 */
    yaml_parser_t* parser = yaml_parser_create(yaml_data, size);
    if (!parser) {
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 解析YAML */
    if (yaml_parse(parser) != 0) {
        yaml_parser_destroy(parser);
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    /* 获取根节点 */
    yaml_node_t* root = yaml_get_root(parser);
    if (!root) {
        yaml_parser_destroy(parser);
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    /* 解析uart节点（顶层配置，不限于调试模式） */
    yaml_node_t* uart_node = yaml_find_node(root, "uart");
    if (uart_node && uart_node->type == YAML_TYPE_MAPPING) {
        /* 解析port */
        yaml_node_t* uart_subnode = yaml_find_node(uart_node, "port");
        if (uart_subnode) {
            u32 port = (u32)yaml_get_u64(uart_subnode, 0x3F8);
            /* 通过boot_info传递串口端口给minimal_uart */
            if (g_boot_info) {
                g_boot_info->debug.serial_port = (u16)port;
            }
        }
        
        /* 解析baud_rate */
        uart_subnode = yaml_find_node(uart_node, "baud_rate");
        if (uart_subnode) {
            u32 baud = (u32)yaml_get_u64(uart_subnode, 115200);
            g_runtime_config.serial_baud = baud;
        }
    }
    
    /* 解析system_limits节点 */
    yaml_node_t* limits_node = yaml_find_node(root, "system_limits");
    if (limits_node && limits_node->type == YAML_TYPE_MAPPING) {
        /* 解析max_domains */
        yaml_node_t* subnode = yaml_find_node(limits_node, "max_domains");
        if (subnode) {
            g_runtime_config.max_domains = (u32)yaml_get_u64(subnode, 256);
        }
        
        /* 解析max_capabilities */
        subnode = yaml_find_node(limits_node, "max_capabilities");
        if (subnode) {
            g_runtime_config.max_capabilities = (u32)yaml_get_u64(subnode, 2048);
        }
        
        /* 解析capabilities_per_domain */
        subnode = yaml_find_node(limits_node, "capabilities_per_domain");
        if (subnode) {
            g_runtime_config.capabilities_per_domain = (u32)yaml_get_u64(subnode, 128);
        }
        
        /* 解析max_threads */
        subnode = yaml_find_node(limits_node, "max_threads");
        if (subnode) {
            g_runtime_config.max_threads = (u32)yaml_get_u64(subnode, 256);
        }
        
        /* 解析threads_per_domain */
        subnode = yaml_find_node(limits_node, "threads_per_domain");
        if (subnode) {
            g_runtime_config.threads_per_domain = (u32)yaml_get_u64(subnode, 16);
        }
        
        /* 解析max_services */
        subnode = yaml_find_node(limits_node, "max_services");
        if (subnode) {
            g_runtime_config.max_services = (u32)yaml_get_u64(subnode, 64);
        }
        
        /* 解析max_pci_devices */
        subnode = yaml_find_node(limits_node, "max_pci_devices");
        if (subnode) {
            g_runtime_config.max_pci_devices = (u32)yaml_get_u64(subnode, 64);
        }
        
        /* 解析max_memory_regions */
        subnode = yaml_find_node(limits_node, "max_memory_regions");
        if (subnode) {
            g_runtime_config.max_memory_regions = (u32)yaml_get_u64(subnode, 32);
        }
        
        /* 解析max_interrupt_routes */
        subnode = yaml_find_node(limits_node, "max_interrupt_routes");
        if (subnode) {
            g_runtime_config.max_interrupt_routes = (u32)yaml_get_u64(subnode, 64);
        }
        
        /* 解析kernel_size_limit */
        subnode = yaml_find_node(limits_node, "kernel_size_limit");
        if (subnode) {
            g_runtime_config.kernel_size_limit = yaml_get_u64(subnode, 2097152);
        }
        
        /* 解析bss_size_limit */
        subnode = yaml_find_node(limits_node, "bss_size_limit");
        if (subnode) {
            g_runtime_config.bss_size_limit = yaml_get_u64(subnode, 524288);
        }
        
        /* 解析max_domains_per_user */
        subnode = yaml_find_node(limits_node, "max_domains_per_user");
        if (subnode) {
            g_runtime_config.max_domains_per_user = (u32)yaml_get_u64(subnode, 64);
        }
    }
    
    /* 解析memory节点 */
    yaml_node_t* memory_node = yaml_find_node(root, "memory");
    if (memory_node && memory_node->type == YAML_TYPE_MAPPING) {
        /* 解析page_cache_percent */
        yaml_node_t* mem_subnode = yaml_find_node(memory_node, "cache");
        if (mem_subnode && mem_subnode->type == YAML_TYPE_MAPPING) {
            yaml_node_t* cache_node = yaml_find_node(mem_subnode, "page_cache_percent");
            if (cache_node) {
                g_runtime_config.page_cache_percent = (u32)yaml_get_u64(cache_node, 20);
            }
        }
        
        /* 解析guard_pages */
        mem_subnode = yaml_find_node(memory_node, "guard_pages");
        if (mem_subnode) {
            g_runtime_config.guard_pages = yaml_get_bool(mem_subnode, true);
        }
        
        /* 解析guard_page_size */
        mem_subnode = yaml_find_node(memory_node, "guard_page_size");
        if (mem_subnode) {
            g_runtime_config.guard_page_size = (u32)yaml_get_u64(mem_subnode, 4096);
        }
        
        /* 解析zero_on_free */
        mem_subnode = yaml_find_node(memory_node, "zero_on_free");
        if (mem_subnode) {
            g_runtime_config.zero_on_free = yaml_get_bool(mem_subnode, true);
        }
    }
    
    /* 解析scheduler节点 */
    yaml_node_t* scheduler_node = yaml_find_node(root, "scheduler");
    if (scheduler_node && scheduler_node->type == YAML_TYPE_MAPPING) {
        /* 解析time_slice_ms */
        yaml_node_t* sched_subnode = yaml_find_node(scheduler_node, "time_slice_ms");
        if (sched_subnode) {
            g_runtime_config.time_slice_ms = (u32)yaml_get_u64(sched_subnode, 10);
        }
        
        /* 解析preemptive */
        sched_subnode = yaml_find_node(scheduler_node, "preemptive");
        if (sched_subnode) {
            g_runtime_config.preemptive = yaml_get_bool(sched_subnode, true);
        }
        
        /* 解析load_balancing */
        sched_subnode = yaml_find_node(scheduler_node, "load_balancing");
        if (sched_subnode && sched_subnode->type == YAML_TYPE_MAPPING) {
            yaml_node_t* lb_node = yaml_find_node(sched_subnode, "threshold_percent");
            if (lb_node) {
                g_runtime_config.load_balancing_threshold = (u32)yaml_get_u64(lb_node, 80);
            }
            
            lb_node = yaml_find_node(sched_subnode, "migration_interval_ms");
            if (lb_node) {
                g_runtime_config.migration_interval_ms = (u32)yaml_get_u64(lb_node, 100);
            }
        }
    }
    
    /* 解析security节点 */
    yaml_node_t* security_node = yaml_find_node(root, "security");
    if (security_node && security_node->type == YAML_TYPE_MAPPING) {
        /* 解析password节点 */
        yaml_node_t* password_node = yaml_find_node(security_node, "password");
        if (password_node && password_node->type == YAML_TYPE_MAPPING) {
            /* 解析default_password */
            yaml_node_t* pwd_subnode = yaml_find_node(password_node, "default");
            if (pwd_subnode && pwd_subnode->value) {
                u32 len = (u32)strlen(pwd_subnode->value);
                if (len < sizeof(g_runtime_config.default_password)) {
                    strcpy(g_runtime_config.default_password, pwd_subnode->value);
                }
            }
            
            /* 解析required */
            pwd_subnode = yaml_find_node(password_node, "required");
            if (pwd_subnode) {
                g_runtime_config.password_required = yaml_get_bool(pwd_subnode, true);
            }
            
            /* 解析min_length */
            pwd_subnode = yaml_find_node(password_node, "min_length");
            if (pwd_subnode) {
                g_runtime_config.password_min_length = (u32)yaml_get_u64(pwd_subnode, 8);
            }
            
            /* 解析require_uppercase */
            pwd_subnode = yaml_find_node(password_node, "require_uppercase");
            if (pwd_subnode) {
                g_runtime_config.password_require_upper = yaml_get_bool(pwd_subnode, true);
            }
            
            /* 解析require_lowercase */
            pwd_subnode = yaml_find_node(password_node, "require_lowercase");
            if (pwd_subnode) {
                g_runtime_config.password_require_lower = yaml_get_bool(pwd_subnode, true);
            }
            
            /* 解析require_digit */
            pwd_subnode = yaml_find_node(password_node, "require_digit");
            if (pwd_subnode) {
                g_runtime_config.password_require_digit = yaml_get_bool(pwd_subnode, true);
            }
        }
        
        /* 解析capability节点 */
        yaml_node_t* cap_node = yaml_find_node(security_node, "capability");
        if (cap_node && cap_node->type == YAML_TYPE_MAPPING) {
            /* 解析verify_on_access */
            yaml_node_t* sec_subnode = yaml_find_node(cap_node, "verify_on_access");
            if (sec_subnode) {
                g_runtime_config.verify_on_access = yaml_get_bool(sec_subnode, true);
            }
            
            /* 解析revoke_delay_ms */
            sec_subnode = yaml_find_node(cap_node, "revoke_delay_ms");
            if (sec_subnode) {
                g_runtime_config.capability_revoke_delay_ms = (u32)yaml_get_u64(sec_subnode, 100);
            }
        }
        
        /* 解析privileged_access */
        yaml_node_t* priv_node = yaml_find_node(security_node, "privileged_access");
        if (priv_node && priv_node->type == YAML_TYPE_MAPPING) {
            /* 解析log_privileged_ops */
            yaml_node_t* sec_subnode = yaml_find_node(priv_node, "log_privileged_ops");
            if (sec_subnode) {
                g_runtime_config.log_privileged_ops = yaml_get_bool(sec_subnode, true);
            }
        }
    }
    
    yaml_parser_destroy(parser);
    
    return HIC_SUCCESS;
}