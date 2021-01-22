#ifndef KEYMAP_H
#define KEYMAP_H

#include "common.h"

typedef enum KeyModifier
{
    KM_LeftShift = 1,
    KM_RightShift = 2,
    KM_Ctrl = 4,
    KM_Alt = 8
} KeyModifier;

enum
{
    KEY_LEFTSHIFT = 0x2A,
    KEY_RIGHTSHIFT = 0x36,
    KEY_CTRL = 0x1D,
    KEY_ALT = 0x38,
    KEY_CAPSLOCK = 0x3A,
    KEY_F1 = 0x3B,
    KEY_F2 = 0x3C,
    KEY_F3 = 0x3D
};

// PC keyboard interface constants

#define KBSTATP         0x64    // kbd controller status port(I)
#define KBS_DIB         0x01    // kbd data in buffer
#define KBDATAP         0x60    // kbd data port(I)

#define NO              0

#define SHIFT           (1<<0)
#define CTL             (1<<1)
#define ALT             (1<<2)

#define CAPSLOCK        (1<<3)
#define NUMLOCK         (1<<4)
#define SCROLLLOCK      (1<<5)

#define E0ESC           (1<<6)

// Special keycodes
#define KEY_HOME        0xE0
#define KEY_END         0xE1
#define KEY_UP          0xE2
#define KEY_DOWN        0xE3
#define KEY_LEFT        0xE4
#define KEY_RIGHT       0xE5
#define KEY_PAGEUP      0xE6
#define KEY_PAGEDOWN    0xE7
#define KEY_INSERT      0xE8
#define KEY_DELETE      0xE9

// C('A') == Control-A
#define C(x) (x - '@')

extern uint8_t g_key_map[256];
extern uint8_t g_key_shift_map[256];

#endif //KEYMAP_H