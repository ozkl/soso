/*
 *  BSD 2-Clause License
 *
 *  Copyright (c) 2025, ozkl
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OZTERM_H
#define OZTERM_H

#include "stdint.h"

typedef struct Ozterm Ozterm;

typedef void (*OztermRefresh)(Ozterm* terminal);
typedef void (*OztermSetCharacter)(Ozterm* terminal, int16_t row, int16_t column, uint8_t character);
typedef void (*OztermMoveCursor)(Ozterm* terminal, int16_t old_row, int16_t old_column, int16_t row, int16_t column);
typedef void (*OztermWriteToMaster)(Ozterm* terminal, const uint8_t* data, int32_t size);

//Do not change/add to this
//It is also compatible to VGA text mode (must be 16 bits)
typedef struct OztermCell
{
    uint8_t character;
    uint8_t color;
} OztermCell;

typedef struct OztermScreen
{
    OztermCell * buffer;
    int16_t cursor_row;
    int16_t cursor_column;
} OztermScreen;

typedef struct Ozterm
{
    OztermScreen* screen_main;
    OztermScreen* screen_alternative;
    OztermScreen* screen_active;
    uint8_t alternative_active;
    int16_t saved_cursor_row;
    int16_t saved_cursor_column;
    int16_t row_count;
    int16_t column_count;
    int16_t scroll_top;
    int16_t scroll_bottom;
    uint8_t color;
    void* custom_data;
    OztermRefresh refresh_function;
    OztermSetCharacter set_character_function;
    OztermMoveCursor move_cursor_function;
    OztermWriteToMaster write_to_master_function;
} Ozterm;

typedef enum OztermKeyModifier
{
    OZTERM_KEYM_NONE = 0,
    OZTERM_KEYM_LEFTSHIFT = 1,
    OZTERM_KEYM_RIGHTSHIFT = 2,
    OZTERM_KEYM_CTRL = 4,
    OZTERM_KEYM_ALT = 8
} OztermKeyModifier;


#define   OZTERM_KEY_NONE       0x00
#define   OZTERM_KEY_HOME       0xE0
#define   OZTERM_KEY_END        0xE1
#define   OZTERM_KEY_UP         0xE2
#define   OZTERM_KEY_DOWN       0xE3
#define   OZTERM_KEY_LEFT       0xE4
#define   OZTERM_KEY_RIGHT      0xE5
#define   OZTERM_KEY_PAGEUP     0xE6
#define   OZTERM_KEY_PAGEDOWN   0xE7
#define   OZTERM_KEY_INSERT     0xE8
#define   OZTERM_KEY_DELETE     0xE9
#define   OZTERM_KEY_RETURN     0xEA
#define   OZTERM_KEY_BACKSPACE  0xEB
#define   OZTERM_KEY_TAB        0xEC
#define   OZTERM_KEY_ESCAPE     0xED
// Function keys
#define   OZTERM_KEY_F1         0xF1
#define   OZTERM_KEY_F2         0xF2
#define   OZTERM_KEY_F3         0xF3
#define   OZTERM_KEY_F4         0xF4
#define   OZTERM_KEY_F5         0xF5
#define   OZTERM_KEY_F6         0xF6
#define   OZTERM_KEY_F7         0xF7
#define   OZTERM_KEY_F8         0xF8
#define   OZTERM_KEY_F9         0xF9
#define   OZTERM_KEY_F10        0xFA
#define   OZTERM_KEY_F11        0xFB
#define   OZTERM_KEY_F12        0xFC


Ozterm* ozterm_create(uint16_t row_count, uint16_t column_count);
void ozterm_destroy(Ozterm* terminal);

//this will cause a OztermWriteToMaster
void ozterm_send_key(Ozterm* terminal, OztermKeyModifier modifier, uint8_t character);

//give the data from master to the terminal
void ozterm_have_read_from_master(Ozterm* terminal, const uint8_t* data, int32_t size);

#endif // OZTERM_H
