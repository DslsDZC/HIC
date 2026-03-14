/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 键盘服务
 * 
 * 支持 PS/2 键盘驱动
 * 默认美式键盘布局 (US QWERTY)
 */

#include "service.h"

/* 魔数 */
#define KEYBOARD_BUFFER_MAGIC 0x4B455942  /* "KEYB" */

/* 端口 I/O 函数 */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* 美式键盘布局 - 普通模式（小写） */
static const char us_layout_normal[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* 美式键盘布局 - Shift模式（大写） */
static const char us_layout_shift[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* 键名表 */
static const char* key_names[128] = {
    [KEY_ESCAPE]    = "Escape",
    [KEY_1]         = "1",
    [KEY_2]         = "2",
    [KEY_3]         = "3",
    [KEY_4]         = "4",
    [KEY_5]         = "5",
    [KEY_6]         = "6",
    [KEY_7]         = "7",
    [KEY_8]         = "8",
    [KEY_9]         = "9",
    [KEY_0]         = "0",
    [KEY_MINUS]     = "Minus",
    [KEY_EQUAL]     = "Equal",
    [KEY_BACKSPACE] = "Backspace",
    [KEY_TAB]       = "Tab",
    [KEY_Q]         = "Q",
    [KEY_W]         = "W",
    [KEY_E]         = "E",
    [KEY_R]         = "R",
    [KEY_T]         = "T",
    [KEY_Y]         = "Y",
    [KEY_U]         = "U",
    [KEY_I]         = "I",
    [KEY_O]         = "O",
    [KEY_P]         = "P",
    [KEY_LBRACKET]  = "LBracket",
    [KEY_RBRACKET]  = "RBracket",
    [KEY_ENTER]     = "Enter",
    [KEY_LCTRL]     = "LCtrl",
    [KEY_A]         = "A",
    [KEY_S]         = "S",
    [KEY_D]         = "D",
    [KEY_F]         = "F",
    [KEY_G]         = "G",
    [KEY_H]         = "H",
    [KEY_J]         = "J",
    [KEY_K]         = "K",
    [KEY_L]         = "L",
    [KEY_SEMICOLON] = "Semicolon",
    [KEY_APOSTROPHE]= "Apostrophe",
    [KEY_GRAVE]     = "Grave",
    [KEY_LSHIFT]    = "LShift",
    [KEY_BACKSLASH] = "Backslash",
    [KEY_Z]         = "Z",
    [KEY_X]         = "X",
    [KEY_C]         = "C",
    [KEY_V]         = "V",
    [KEY_B]         = "B",
    [KEY_N]         = "N",
    [KEY_M]         = "M",
    [KEY_COMMA]     = "Comma",
    [KEY_DOT]       = "Dot",
    [KEY_SLASH]     = "Slash",
    [KEY_RSHIFT]    = "RShift",
    [KEY_LALT]      = "LAlt",
    [KEY_SPACE]     = "Space",
    [KEY_CAPSLOCK]  = "CapsLock",
    [KEY_F1]        = "F1",
    [KEY_F2]        = "F2",
    [KEY_F3]        = "F3",
    [KEY_F4]        = "F4",
    [KEY_F5]        = "F5",
    [KEY_F6]        = "F6",
    [KEY_F7]        = "F7",
    [KEY_F8]        = "F8",
    [KEY_F9]        = "F9",
    [KEY_F10]       = "F10",
    [KEY_NUMLOCK]   = "NumLock",
    [KEY_SCROLLLOCK]= "ScrollLock",
    [KEY_F11]       = "F11",
    [KEY_F12]       = "F12",
};

/* 全局键盘缓冲区 */
static keyboard_buffer_t g_keyboard_buffer = {
    .magic = KEYBOARD_BUFFER_MAGIC,
    .write_pos = 0,
    .read_pos = 0,
    .modifiers = 0,
    .extended = 0
};

/* 检查键盘是否有数据 */
static u8 keyboard_has_data(void) {
    return (inb(KEYBOARD_STATUS_PORT) & 0x01) != 0;
}

/* 读取扫描码 */
static u8 keyboard_read_scancode(void) {
    return inb(KEYBOARD_DATA_PORT);
}

/* 将扫描码转换为ASCII字符 */
static char scancode_to_char(u8 scancode) {
    if (scancode >= 128) {
        return 0;  /* 释放码，无字符 */
    }
    
    u8 shift = (g_keyboard_buffer.modifiers & (MOD_LSHIFT | MOD_RSHIFT)) != 0;
    
    if (shift) {
        return us_layout_shift[scancode];
    } else {
        return us_layout_normal[scancode];
    }
}

/* 更新修饰键状态 */
static void update_modifier(u8 scancode, u8 pressed) {
    switch (scancode) {
        case KEY_LSHIFT:
            if (pressed) g_keyboard_buffer.modifiers |= MOD_LSHIFT;
            else g_keyboard_buffer.modifiers &= ~MOD_LSHIFT;
            break;
        case KEY_RSHIFT:
            if (pressed) g_keyboard_buffer.modifiers |= MOD_RSHIFT;
            else g_keyboard_buffer.modifiers &= ~MOD_RSHIFT;
            break;
        case KEY_LCTRL:
            if (pressed) g_keyboard_buffer.modifiers |= MOD_LCTRL;
            else g_keyboard_buffer.modifiers &= ~MOD_LCTRL;
            break;
        case KEY_LALT:
            if (pressed) g_keyboard_buffer.modifiers |= MOD_LALT;
            else g_keyboard_buffer.modifiers &= ~MOD_LALT;
            break;
        case KEY_CAPSLOCK:
            if (pressed) g_keyboard_buffer.modifiers ^= MOD_CAPSLOCK;  /* 切换 */
            break;
        case KEY_NUMLOCK:
            if (pressed) g_keyboard_buffer.modifiers ^= MOD_NUMLOCK;   /* 切换 */
            break;
    }
}

/* 添加事件到缓冲区 */
static void push_event(keyboard_event_t *event) {
    g_keyboard_buffer.events[g_keyboard_buffer.write_pos] = *event;
    g_keyboard_buffer.write_pos = (g_keyboard_buffer.write_pos + 1) % KEYBOARD_BUFFER_SIZE;
}

/* 初始化键盘服务 */
void keyboard_service_init(void) {
    /* 清空缓冲区 */
    for (int i = 0; i < KEYBOARD_BUFFER_SIZE; i++) {
        g_keyboard_buffer.events[i].scancode = 0;
        g_keyboard_buffer.events[i].key = 0;
        g_keyboard_buffer.events[i].character = 0;
        g_keyboard_buffer.events[i].modifiers = 0;
        g_keyboard_buffer.events[i].pressed = 0;
    }
    
    g_keyboard_buffer.magic = KEYBOARD_BUFFER_MAGIC;
    g_keyboard_buffer.write_pos = 0;
    g_keyboard_buffer.read_pos = 0;
    g_keyboard_buffer.modifiers = 0;
    g_keyboard_buffer.extended = 0;
    
    /* 清空键盘控制器缓冲区 */
    while (keyboard_has_data()) {
        keyboard_read_scancode();
    }
}

/* 轮询键盘并处理事件 */
void keyboard_service_poll(void) {
    while (keyboard_has_data()) {
        u8 scancode = keyboard_read_scancode();
        
        /* 处理扩展码序列 */
        if (g_keyboard_buffer.extended) {
            g_keyboard_buffer.extended = 0;
            
            /* 扩展码：只处理按下事件 */
            if (!(scancode & 0x80)) {
                keyboard_event_t event = {
                    .scancode = scancode,
                    .key = scancode | 0x80,  /* 标记为扩展键 */
                    .character = 0,           /* 扩展键通常无ASCII字符 */
                    .modifiers = g_keyboard_buffer.modifiers,
                    .pressed = 1
                };
                push_event(&event);
            }
            continue;
        }
        
        /* 检查扩展码前缀 */
        if (scancode == KEY_EXT_PREFIX) {
            g_keyboard_buffer.extended = 1;
            continue;
        }
        
        /* 判断按下/释放 */
        u8 pressed = !(scancode & 0x80);
        u8 key = scancode & 0x7F;
        
        /* 更新修饰键状态 */
        update_modifier(key, pressed);
        
        /* 创建事件 */
        keyboard_event_t event = {
            .scancode = scancode,
            .key = key,
            .character = pressed ? scancode_to_char(key) : 0,
            .modifiers = g_keyboard_buffer.modifiers,
            .pressed = pressed
        };
        
        push_event(&event);
    }
}

/* 检查是否有事件 */
u8 keyboard_service_has_event(void) {
    return g_keyboard_buffer.write_pos != g_keyboard_buffer.read_pos;
}

/* 读取事件 */
keyboard_event_t keyboard_service_read_event(void) {
    if (!keyboard_service_has_event()) {
        keyboard_event_t empty = {0};
        return empty;
    }
    
    keyboard_event_t event = g_keyboard_buffer.events[g_keyboard_buffer.read_pos];
    g_keyboard_buffer.read_pos = (g_keyboard_buffer.read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return event;
}

/* 读取字符（阻塞直到有字符） */
char keyboard_service_read_char(void) {
    while (1) {
        keyboard_service_poll();
        
        while (keyboard_service_has_event()) {
            keyboard_event_t event = keyboard_service_read_event();
            if (event.pressed && event.character) {
                return event.character;
            }
        }
        
        /* 短暂休眠 - 在真实系统中应该使用更高效的方式 */
        for (volatile int i = 0; i < 1000; i++);
    }
}

/* 获取键名 */
const char* keyboard_service_get_key_name(u8 scancode) {
    u8 key = scancode & 0x7F;
    if (key < 128 && key_names[key]) {
        return key_names[key];
    }
    return "Unknown";
}