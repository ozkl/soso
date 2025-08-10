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

#include <stdint.h>

typedef struct Ozterm Ozterm;

typedef struct OztermColor
{
    uint8_t index;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t use_rgb;
} OztermColor;

typedef struct OztermCell
{
    uint8_t character;
    OztermColor fg_color;
    OztermColor bg_color;
} OztermCell;

typedef void (*OztermRefresh)(Ozterm* terminal);
typedef void (*OztermSetCharacter)(Ozterm* terminal, int16_t row, int16_t column, OztermCell* cell);
typedef void (*OztermMoveCursor)(Ozterm* terminal, int16_t old_row, int16_t old_column, int16_t row, int16_t column);
typedef void (*OztermWriteToMaster)(Ozterm* terminal, const uint8_t* data, int32_t size);


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
void ozterm_clear_full(Ozterm* terminal);
void ozterm_set_write_to_master_callback(Ozterm* terminal, OztermWriteToMaster function);
void ozterm_set_render_callbacks(Ozterm* terminal, OztermRefresh refresh_func, OztermSetCharacter character_func, OztermMoveCursor cursor_func);
void ozterm_set_custom_data(Ozterm* terminal, void* custom_data);
void* ozterm_get_custom_data(Ozterm* terminal);
int16_t ozterm_get_row_count(Ozterm* terminal);
int16_t ozterm_get_column_count(Ozterm* terminal);
int16_t ozterm_get_cursor_row(Ozterm* terminal);
int16_t ozterm_get_cursor_column(Ozterm* terminal);
OztermCell* ozterm_get_row_data(Ozterm* terminal, int16_t row);
void ozterm_set_default_color(Ozterm* terminal, OztermColor fg, OztermColor bg);
void ozterm_get_default_color(Ozterm* terminal, OztermColor* fg, OztermColor* bg);
void ozterm_put_text(Ozterm* terminal, const uint8_t* text, int32_t size);

//this is scroll back mechanism, not related to the scrolling inside page or region
//scroll_offset is based from last line
void ozterm_scroll(Ozterm* terminal, int16_t scroll_offset);
int16_t ozterm_get_scroll(Ozterm* terminal);
int16_t ozterm_get_scroll_count(Ozterm* terminal);

//this will cause a OztermWriteToMaster
void ozterm_send_key(Ozterm* terminal, OztermKeyModifier modifier, uint8_t character);

//give the data from master to the terminal
void ozterm_have_read_from_master(Ozterm* terminal, const uint8_t* data, int32_t size);

#endif // OZTERM_H
