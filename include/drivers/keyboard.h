#ifndef _DRIVERS_KEYBOARD_H
#define _DRIVERS_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define KEY_SPECIAL_FLAG 0x0100

enum special_keys {
    KEY_UP       = KEY_SPECIAL_FLAG | 0x01,
    KEY_DOWN     = KEY_SPECIAL_FLAG | 0x02,
    KEY_LEFT     = KEY_SPECIAL_FLAG | 0x03,
    KEY_RIGHT    = KEY_SPECIAL_FLAG | 0x04,
    KEY_HOME     = KEY_SPECIAL_FLAG | 0x05,
    KEY_END      = KEY_SPECIAL_FLAG | 0x06,
    KEY_PAGE_UP  = KEY_SPECIAL_FLAG | 0x07,
    KEY_PAGE_DOWN= KEY_SPECIAL_FLAG | 0x08,
    KEY_INSERT   = KEY_SPECIAL_FLAG | 0x09,
    KEY_DELETE   = KEY_SPECIAL_FLAG | 0x0A,
    KEY_F1       = KEY_SPECIAL_FLAG | 0x11,
    KEY_F2       = KEY_SPECIAL_FLAG | 0x12,
    KEY_F3       = KEY_SPECIAL_FLAG | 0x13,
    KEY_F4       = KEY_SPECIAL_FLAG | 0x14,
    KEY_F5       = KEY_SPECIAL_FLAG | 0x15,
    KEY_F6       = KEY_SPECIAL_FLAG | 0x16,
    KEY_F7       = KEY_SPECIAL_FLAG | 0x17,
    KEY_F8       = KEY_SPECIAL_FLAG | 0x18,
    KEY_F9       = KEY_SPECIAL_FLAG | 0x19,
    KEY_F10      = KEY_SPECIAL_FLAG | 0x1A,
    KEY_F11      = KEY_SPECIAL_FLAG | 0x1B,
    KEY_F12      = KEY_SPECIAL_FLAG | 0x1C,
};

void keyboard_init(void);
uint16_t keyboard_get_key(void);
bool keyboard_has_key(void);
char keyboard_get_char(void);

#endif // _DRIVERS_KEYBOARD_H
