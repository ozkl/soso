#include "keymap.h"

uint8_t g_key_map[256] =
{
  NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10
  'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20
  '\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',  // 0x30
  NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
  NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
  [0x49] = KEY_PAGEUP,
  [0x51] = KEY_PAGEDOWN,
  [0x47] = KEY_HOME,
  [0x4F] = KEY_END,
  [0x52] = KEY_INSERT,
  [0x53] = KEY_DELETE,
  [0x48] = KEY_UP,
  [0x50] = KEY_DOWN,
  [0x4B] = KEY_LEFT,
  [0x4D] = KEY_RIGHT,
  [0x9C] = '\n',      // KP_Enter
  [0xB5] = '/',       // KP_Div
  [0xC8] = KEY_UP,
  [0xD0] = KEY_DOWN,
  [0xC9] = KEY_PAGEUP,
  [0xD1] = KEY_PAGEDOWN,
  [0xCB] = KEY_LEFT,
  [0xCD] = KEY_RIGHT,
  [0x97] = KEY_HOME,
  [0xCF] = KEY_END,
  [0xD2] = KEY_INSERT,
  [0xD3] = KEY_DELETE
};

uint8_t g_key_shift_map[256] =
{
  NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
  'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
  '"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
  'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',  // 0x30
  NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
  NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
  [0x49] = KEY_PAGEUP,
  [0x51] = KEY_PAGEDOWN,
  [0x47] = KEY_HOME,
  [0x4F] = KEY_END,
  [0x52] = KEY_INSERT,
  [0x53] = KEY_DELETE,
  [0x48] = KEY_UP,
  [0x50] = KEY_DOWN,
  [0x4B] = KEY_LEFT,
  [0x4D] = KEY_RIGHT,
  [0x9C] = '\n',      // KP_Enter
  [0xB5] = '/',       // KP_Div
  [0xC8] = KEY_UP,
  [0xD0] = KEY_DOWN,
  [0xC9] = KEY_PAGEUP,
  [0xD1] = KEY_PAGEDOWN,
  [0xCB] = KEY_LEFT,
  [0xCD] = KEY_RIGHT,
  [0x97] = KEY_HOME,
  [0xCF] = KEY_END,
  [0xD2] = KEY_INSERT,
  [0xD3] = KEY_DELETE
};