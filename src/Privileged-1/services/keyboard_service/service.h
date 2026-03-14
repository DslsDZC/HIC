/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef KEYBOARD_SERVICE_H
#define KEYBOARD_SERVICE_H

#include <common.h>

/* 键盘端口 */
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

/* 键盘缓冲区大小 */
#define KEYBOARD_BUFFER_SIZE 256

/* 特殊键码 */
#define KEY_ESCAPE      0x01
#define KEY_1           0x02
#define KEY_2           0x03
#define KEY_3           0x04
#define KEY_4           0x05
#define KEY_5           0x06
#define KEY_6           0x07
#define KEY_7           0x08
#define KEY_8           0x09
#define KEY_9           0x0A
#define KEY_0           0x0B
#define KEY_MINUS       0x0C
#define KEY_EQUAL       0x0D
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_Q           0x10
#define KEY_W           0x11
#define KEY_E           0x12
#define KEY_R           0x13
#define KEY_T           0x14
#define KEY_Y           0x15
#define KEY_U           0x16
#define KEY_I           0x17
#define KEY_O           0x18
#define KEY_P           0x19
#define KEY_LBRACKET    0x1A
#define KEY_RBRACKET    0x1B
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_A           0x1E
#define KEY_S           0x1F
#define KEY_D           0x20
#define KEY_F           0x21
#define KEY_G           0x22
#define KEY_H           0x23
#define KEY_J           0x24
#define KEY_K           0x25
#define KEY_L           0x26
#define KEY_SEMICOLON   0x27
#define KEY_APOSTROPHE  0x28
#define KEY_GRAVE       0x29
#define KEY_LSHIFT      0x2A
#define KEY_BACKSLASH   0x2B
#define KEY_Z           0x2C
#define KEY_X           0x2D
#define KEY_C           0x2E
#define KEY_V           0x2F
#define KEY_B           0x30
#define KEY_N           0x31
#define KEY_M           0x32
#define KEY_COMMA       0x33
#define KEY_DOT         0x34
#define KEY_SLASH       0x35
#define KEY_RSHIFT      0x36
#define KEY_KP_MULTIPLY 0x37
#define KEY_LALT        0x38
#define KEY_SPACE       0x39
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_NUMLOCK     0x45
#define KEY_SCROLLLOCK  0x46
#define KEY_KP_7        0x47
#define KEY_KP_8        0x48
#define KEY_KP_9        0x49
#define KEY_KP_MINUS    0x4A
#define KEY_KP_4        0x4B
#define KEY_KP_5        0x4C
#define KEY_KP_6        0x4D
#define KEY_KP_PLUS     0x4E
#define KEY_KP_1        0x4F
#define KEY_KP_2        0x50
#define KEY_KP_3        0x51
#define KEY_KP_0        0x52
#define KEY_KP_DOT      0x53
#define KEY_F11         0x57
#define KEY_F12         0x58

/* 扩展键码（E0前缀） */
#define KEY_EXT_PREFIX  0xE0
#define KEY_UP          0x48
#define KEY_DOWN        0x50
#define KEY_LEFT        0x4B
#define KEY_RIGHT       0x4D
#define KEY_HOME        0x47
#define KEY_END         0x4F
#define KEY_PAGEUP      0x49
#define KEY_PAGEDOWN    0x51
#define KEY_INSERT      0x52
#define KEY_DELETE      0x53

/* 键盘事件结构 */
typedef struct {
    u8 scancode;       /* 原始扫描码 */
    u8 key;            /* 键码 */
    char character;    /* ASCII字符（如果有） */
    u8 modifiers;      /* 修饰键状态 */
    u8 pressed;        /* 按下/释放 */
    u8 _reserved[3];   /* 填充 */
} keyboard_event_t;

/* 修饰键标志 */
#define MOD_LSHIFT    (1 << 0)
#define MOD_RSHIFT    (1 << 1)
#define MOD_LCTRL     (1 << 2)
#define MOD_RCTRL     (1 << 3)
#define MOD_LALT      (1 << 4)
#define MOD_RALT      (1 << 5)
#define MOD_CAPSLOCK  (1 << 6)
#define MOD_NUMLOCK   (1 << 7)

/* 键盘缓冲区结构 */
typedef struct {
    u32 magic;                           /* 魔数 */
    u32 write_pos;                       /* 写位置 */
    u32 read_pos;                        /* 读位置 */
    keyboard_event_t events[KEYBOARD_BUFFER_SIZE]; /* 事件缓冲区 */
    u8 modifiers;                        /* 当前修饰键状态 */
    u8 extended;                         /* 是否在扩展码序列中 */
    u8 _reserved[2];                     /* 填充 */
} keyboard_buffer_t;

/* 服务接口 */
void keyboard_service_init(void);
void keyboard_service_poll(void);
u8 keyboard_service_has_event(void);
keyboard_event_t keyboard_service_read_event(void);
char keyboard_service_read_char(void);
const char* keyboard_service_get_key_name(u8 scancode);

#endif /* KEYBOARD_SERVICE_H */