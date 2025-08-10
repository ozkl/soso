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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ozterm.h"

typedef struct OztermScreen
{
    OztermCell* buffer;
    int16_t cursor_row;
    int16_t cursor_column;
    OztermColor fg_color;
    OztermColor bg_color;
    uint8_t attr_inverse;
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
    OztermColor fg_color_default;
    OztermColor bg_color_default;
    uint8_t DECCKM;
    void* custom_data;
    OztermCell** scrollback;     // Array of pointers to lines
    int16_t scrollback_head;         // Next line to write
    int16_t scrollback_count;        // Total filled lines
    int16_t scroll_offset;           // Current scroll view offset
    OztermRefresh refresh_function;
    OztermSetCharacter set_character_function;
    OztermMoveCursor move_cursor_function;
    OztermWriteToMaster write_to_master_function;
} Ozterm;

#define TAB_WIDTH 8

#define SCROLLBACK_LINES 1024

// C('A') == Control-A
#define C(x) (x - '@')

//Internal API
static void ozterm_set_character(Ozterm * terminal, int16_t row, int16_t column, uint8_t character, uint8_t callback);
static void ozterm_reset_attributes(Ozterm* terminal);
static void ozterm_reset_attributes_screen(Ozterm* terminal, OztermScreen* screen);
static void ozterm_clear(Ozterm* terminal);
static void ozterm_line_insert_characters(Ozterm* terminal, uint8_t c, int16_t count);
static void ozterm_line_delete_characters(Ozterm* terminal, int16_t count);
static void ozterm_put_character(Ozterm* terminal, uint8_t c);
static void ozterm_move_cursor(Ozterm* terminal, int16_t row, int16_t column);
static void ozterm_move_cursor_diff(Ozterm* terminal, int16_t row, int16_t column);
static void ozterm_scroll_up(Ozterm* terminal, int lines);
static void ozterm_scroll_up_region(Ozterm* terminal, int lines);
static void ozterm_scroll_down_region(Ozterm* terminal, int lines);
static void ozterm_insert_lines(Ozterm* terminal, int from_row, int count);
static void ozterm_delete_lines(Ozterm* terminal, int from_row, int count);
static void ozterm_switch_to_alt_screen(Ozterm* terminal);
static void ozterm_restore_main_screen(Ozterm* terminal);

static void * malloc_impl(size_t size)
{
    return malloc(size);
}

static void free_impl(void * address)
{
    free(address);
}


Ozterm* ozterm_create(uint16_t row_count, uint16_t column_count)
{
    Ozterm* terminal = malloc_impl(sizeof(Ozterm));
    memset((uint8_t*)terminal, 0, sizeof(Ozterm));

    terminal->screen_main = malloc_impl(sizeof(OztermScreen));
    memset((uint8_t*)terminal->screen_main, 0, sizeof(OztermScreen));
    terminal->screen_main->buffer = malloc_impl(row_count * column_count * sizeof(OztermCell));
    memset((uint8_t*)terminal->screen_main->buffer, 0, row_count * column_count * sizeof(OztermCell));
    terminal->screen_main->cursor_column = 0;
    terminal->screen_main->cursor_row = 0;

    terminal->screen_alternative = malloc_impl(sizeof(OztermScreen));
    memset((uint8_t*)terminal->screen_alternative, 0, sizeof(OztermScreen));
    terminal->screen_alternative->buffer = malloc_impl(row_count * column_count * sizeof(OztermCell));
    memset((uint8_t*)terminal->screen_alternative->buffer, 0, row_count * column_count * sizeof(OztermCell));
    terminal->screen_alternative->cursor_column = 0;
    terminal->screen_alternative->cursor_row = 0;

    terminal->screen_active = terminal->screen_main;
    terminal->alternative_active = 0;
    terminal->saved_cursor_column = 0;
    terminal->saved_cursor_row = 0;
    terminal->column_count = column_count;
    terminal->row_count = row_count;
    terminal->scroll_top = 0;
    terminal->scroll_bottom = terminal->row_count - 1;
    terminal->fg_color_default.index = 7;
    terminal->bg_color_default.index = 0;

    terminal->scrollback = malloc_impl(sizeof(OztermCell*) * SCROLLBACK_LINES);
    for (int i = 0; i < SCROLLBACK_LINES; ++i)
    {
        terminal->scrollback[i] = malloc_impl(sizeof(OztermCell) * terminal->column_count);
        memset(terminal->scrollback[i], 0, sizeof(OztermCell) * terminal->column_count);
    }
    terminal->scrollback_head = 0;
    terminal->scrollback_count = 0;
    terminal->scroll_offset = 0;

    ozterm_reset_attributes_screen(terminal, terminal->screen_main);
    ozterm_reset_attributes_screen(terminal, terminal->screen_alternative);

    ozterm_clear(terminal);

    return terminal;
}

void ozterm_destroy(Ozterm* terminal)
{
    for (int i = 0; i < SCROLLBACK_LINES; ++i)
    {
        free_impl(terminal->scrollback[i]);
    }
    free_impl(terminal->scrollback);

    free_impl(terminal->screen_main->buffer);
    free_impl(terminal->screen_main);
    free_impl(terminal->screen_alternative->buffer);
    free_impl(terminal->screen_alternative);
    free_impl(terminal);
}

void ozterm_clear_full(Ozterm* terminal)
{
    ozterm_clear(terminal);

    ozterm_switch_to_alt_screen(terminal);

    ozterm_restore_main_screen(terminal);
}

void ozterm_set_write_to_master_callback(Ozterm* terminal, OztermWriteToMaster function)
{
    terminal->write_to_master_function = function;
}

void ozterm_set_render_callbacks(Ozterm* terminal, OztermRefresh refresh_func, OztermSetCharacter character_func, OztermMoveCursor cursor_func)
{
    terminal->refresh_function = refresh_func;
    terminal->set_character_function = character_func;
    terminal->move_cursor_function = cursor_func;
}

void ozterm_set_custom_data(Ozterm* terminal, void* custom_data)
{
    terminal->custom_data = custom_data;
}

void* ozterm_get_custom_data(Ozterm* terminal)
{
    return terminal->custom_data;
}

int16_t ozterm_get_row_count(Ozterm* terminal)
{
    return terminal->row_count;
}

int16_t ozterm_get_column_count(Ozterm* terminal)
{
    return terminal->column_count;
}

int16_t ozterm_get_cursor_row(Ozterm* terminal)
{
    return terminal->screen_active->cursor_row;
}

int16_t ozterm_get_cursor_column(Ozterm* terminal)
{
    return terminal->screen_active->cursor_column;
}

OztermCell* ozterm_get_row_data(Ozterm* terminal, int16_t row)
{
    OztermCell* row_buffer = NULL;

    // scrollback mode
    if (terminal->scroll_offset > 0)
    {
        int scroll_index = terminal->scrollback_count - terminal->scroll_offset + row;

        if (scroll_index < terminal->scrollback_count)
        {
            int ring_index = (terminal->scrollback_head - terminal->scrollback_count + scroll_index + SCROLLBACK_LINES) % SCROLLBACK_LINES;
            row_buffer = terminal->scrollback[ring_index];
        }
        else
        {
            int buffer_y = row - terminal->scroll_offset;
            row_buffer = terminal->screen_active->buffer + (buffer_y * terminal->column_count);
        }
    }
    else
    {
        row_buffer = terminal->screen_active->buffer + (row * terminal->column_count);
    }
    
    return row_buffer;
}

void ozterm_set_default_color(Ozterm* terminal, OztermColor fg, OztermColor bg)
{
    terminal->fg_color_default = fg;
    terminal->bg_color_default = bg;
}

void ozterm_get_default_color(Ozterm* terminal, OztermColor* fg, OztermColor* bg)
{
    *fg = terminal->fg_color_default;
    *bg = terminal->bg_color_default;
}

static void ozterm_reset_attributes(Ozterm* terminal)
{
    ozterm_reset_attributes_screen(terminal, terminal->screen_active);
}

static void ozterm_reset_attributes_screen(Ozterm* terminal, OztermScreen* screen)
{
    screen->fg_color = terminal->fg_color_default;
    screen->bg_color = terminal->bg_color_default;
    
    screen->attr_inverse = 0;
}

void ozterm_scroll(Ozterm* terminal, int16_t scroll_offset)
{
    if (scroll_offset > terminal->scrollback_count)
    {
        scroll_offset = terminal->scrollback_count;
    }

    if (scroll_offset < 0)
    {
        scroll_offset = 0;
    }

    terminal->scroll_offset = scroll_offset;

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}

int16_t ozterm_get_scroll(Ozterm* terminal)
{
    return terminal->scroll_offset;
}

int16_t ozterm_get_scroll_count(Ozterm* terminal)
{
    return terminal->scrollback_count;
}

static void write_to_master(Ozterm* terminal, const char* data, int32_t size)
{
    if (terminal->write_to_master_function && size > 0)
    {
        terminal->write_to_master_function(terminal, (uint8_t*)data, size);
    }
}

static void ozterm_switch_to_alt_screen(Ozterm* terminal)
{
    terminal->alternative_active = 1;
    terminal->screen_active = terminal->screen_alternative;

    //clear alternate screen
    ozterm_reset_attributes(terminal);
    ozterm_clear(terminal);

    if (terminal->refresh_function)
    {
        terminal->refresh_function(terminal);
    }
}

static void ozterm_restore_main_screen(Ozterm* terminal)
{
    terminal->alternative_active = 0;
    terminal->screen_active = terminal->screen_main;

    if (terminal->refresh_function)
    {
        terminal->refresh_function(terminal);
    }
}

static uint8_t ozterm_is_cell_writable(Ozterm * terminal, OztermCell * cell)
{
    return 1;
}

static void ozterm_set_character(Ozterm * terminal, int16_t row, int16_t column, uint8_t character, uint8_t callback)
{
    if (row >= 0 && row < terminal->row_count && column >= 0 && column < terminal->column_count)
    {
        OztermCell * cell = terminal->screen_active->buffer + (row * terminal->column_count + column);

        if (ozterm_is_cell_writable(terminal, cell))
        {
            cell->character = character;

            if (terminal->screen_active->attr_inverse)
            {
                cell->fg_color = terminal->screen_active->bg_color;
                cell->bg_color = terminal->screen_active->fg_color;
            }
            else
            {
                cell->fg_color = terminal->screen_active->fg_color;
                cell->bg_color = terminal->screen_active->bg_color;
            }

            if (callback && terminal->set_character_function)
                terminal->set_character_function(terminal, row, column, cell);
        }
    }
}

static void ozterm_scroll_up(Ozterm* terminal, int lines)
{
    if (lines <= 0) lines = 1;

    for (int l = 0; l < lines; ++l)
    {
        int top = terminal->scroll_top + l;
        memcpy(
            terminal->scrollback[terminal->scrollback_head],
            &terminal->screen_active->buffer[top * terminal->column_count],
            sizeof(OztermCell) * terminal->column_count
        );
        terminal->scrollback_head = (terminal->scrollback_head + 1) % SCROLLBACK_LINES;
        if (terminal->scrollback_count < SCROLLBACK_LINES)
            terminal->scrollback_count++;
    }

    ozterm_scroll_up_region(terminal, lines);
}

static void ozterm_scroll_up_region(Ozterm* terminal, int lines)
{
    if (lines <= 0) lines = 1;

    int top = terminal->scroll_top;
    int bottom = terminal->scroll_bottom;
    int columns = terminal->column_count;
    
    OztermCell* buf = terminal->screen_active->buffer;

    if (lines > bottom - top + 1)
        lines = bottom - top + 1;

    // Scroll UP: move lines up
    for (int y = top; y <= bottom - lines; ++y)
    {
        for (int x = 0; x < columns; ++x) {
            int from = (y + lines) * columns + x;
            int to = y * columns + x;

            if (ozterm_is_cell_writable(terminal, &buf[to]))
            {
                buf[to] = buf[from];
            }
        }
    }

    // Clear newly exposed lines at the bottom
    for (int row = bottom - lines + 1; row <= bottom; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            ozterm_set_character(terminal, row, col, ' ', 0);
        }
    }

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}

static void ozterm_scroll_down_region(Ozterm* terminal, int lines)
{
    if (lines <= 0) lines = 1;

    int top = terminal->scroll_top;
    int bottom = terminal->scroll_bottom;
    int columns = terminal->column_count;
    OztermCell* buf = terminal->screen_active->buffer;

    // Clamp lines to available region
    if (lines > bottom - top + 1)
    {
        lines = bottom - top + 1;
    }

    // Scroll DOWN: move lines from bottom up to top
    for (int row = bottom; row >= top + lines; --row)
    {
        for (int col = 0; col < columns; ++col)
        {
            int to = row * columns + col;
            int from = (row - lines) * columns + col;

            if (ozterm_is_cell_writable(terminal, &buf[to]))
            {
                buf[to] = buf[from];
            }
        }
    }

    // Clear top N lines
    for (int row = top; row < top + lines; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            ozterm_set_character(terminal, row, col, ' ', 0);
        }
    }

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}

static void ozterm_insert_lines(Ozterm* terminal, int from_row, int count)
{
    if (count <= 0) return;

    int top = from_row;
    int bottom = terminal->scroll_bottom;
    int columns = terminal->column_count;

    if (top < terminal->scroll_top || top > bottom) return;

    if (count > bottom - top + 1)
        count = bottom - top + 1;

    OztermCell* buf = terminal->screen_active->buffer;

    // Shift lines down
    for (int row = bottom; row >= top + count; --row)
    {
        for (int col = 0; col < columns; ++col)
        {
            int to = row * columns + col;
            int from = (row - count) * columns + col;

            if (ozterm_is_cell_writable(terminal, &buf[to]))
                buf[to] = buf[from];
        }
    }

    // Clear inserted lines
    for (int row = top; row < top + count; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            ozterm_set_character(terminal, row, col, ' ', 0);
        }
    }

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}

static void ozterm_delete_lines(Ozterm* terminal, int from_row, int count)
{
    if (count <= 0) return;

    int top = from_row;
    int bottom = terminal->scroll_bottom;
    int columns = terminal->column_count;

    if (top < terminal->scroll_top || top > bottom) return;

    if (count > bottom - top + 1)
        count = bottom - top + 1;

    OztermCell* buf = terminal->screen_active->buffer;

    // Shift lines up
    for (int row = top; row <= bottom - count; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            int to = row * columns + col;
            int from = (row + count) * columns + col;

            if (ozterm_is_cell_writable(terminal, &buf[to]))
                buf[to] = buf[from];
        }
    }

    // Clear bottom lines
    for (int row = bottom - count + 1; row <= bottom; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            ozterm_set_character(terminal, row, col, ' ', 0);
        }
    }

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}


static void ozterm_clear(Ozterm* terminal)
{
    for (int row = 0; row < terminal->row_count; ++row)
    {
        for (int col = 0; col < terminal->column_count; ++col)
        {
            ozterm_set_character(terminal, row, col, ' ', 0);
        }
    }

    ozterm_move_cursor(terminal, 0, 0);

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}

static void ozterm_line_insert_characters(Ozterm* terminal, uint8_t c, int16_t count)
{
    OztermCell *cell = terminal->screen_active->buffer + (terminal->screen_active->cursor_row * terminal->column_count);
    int16_t x = terminal->screen_active->cursor_column;

    if (x >= terminal->column_count)
        return;

    if (x + count >= terminal->column_count)
        count = terminal->column_count - x;

    // Shift cells to the right
    for (int i = terminal->column_count - 1; i >= x + count; --i)
    {
        if (ozterm_is_cell_writable(terminal, &cell[i]))
        {
            // Find the source to copy from
            int src = i - count;

            if (src >= x)
            {
                cell[i] = cell[src];

                if (terminal->set_character_function)
                    terminal->set_character_function(terminal, terminal->screen_active->cursor_row, i, &cell[i]);
            }
            else
            {
                ozterm_set_character(terminal, terminal->screen_active->cursor_row, i, c, 1);
            }
        }
    }

    // Fill inserted area
    for (int i = 0; i < count; ++i)
    {
        if (ozterm_is_cell_writable(terminal, &cell[x + i]))
        {
            ozterm_set_character(terminal, terminal->screen_active->cursor_row, x + i, ' ', 1);
        }
    }
}

static void ozterm_line_delete_characters(Ozterm* terminal, int16_t count)
{
    OztermCell *cell = terminal->screen_active->buffer + (terminal->screen_active->cursor_row * terminal->column_count);
    int16_t x = terminal->screen_active->cursor_column;

    if (x >= terminal->column_count)
        return;

    if (x + count >= terminal->column_count)
        count = terminal->column_count - x;

    // Shift cells left
    for (int i = x; i < terminal->column_count - count; ++i)
    {
        if (ozterm_is_cell_writable(terminal, &cell[i]))
        {
            int src = i + count;

            if (src < terminal->column_count)
            {
                cell[i] = cell[src];

                if (terminal->set_character_function)
                    terminal->set_character_function(terminal, terminal->screen_active->cursor_row, i, &cell[i]);
            }
            else
            {
                ozterm_set_character(terminal, terminal->screen_active->cursor_row, i, ' ', 1);
            }
        }
    }

    // Clear vacated cells at end
    for (int i = terminal->column_count - count; i < terminal->column_count; ++i)
    {
        if (ozterm_is_cell_writable(terminal, &cell[i]))
        {
            ozterm_set_character(terminal, terminal->screen_active->cursor_row, i, ' ', 1);
        }
    }
}

void ozterm_put_character_and_cursor(Ozterm* terminal, uint8_t c)
{
    if ('\n' == c)
    {
        if (terminal->screen_active->cursor_row == terminal->scroll_bottom)
        {
            // We're at bottom of scroll region — scroll up
            ozterm_scroll_up(terminal, 1);
            // Cursor stays on the same row, column unchanged
        }
        else
        {
            // Move cursor down
            ozterm_move_cursor(terminal, terminal->screen_active->cursor_row + 1, terminal->screen_active->cursor_column);
        }
    }
    else if ('\r' == c)
    {
        ozterm_move_cursor(terminal, terminal->screen_active->cursor_row, 0);
    }
    else if ('\b' == c)
    {
        if (terminal->screen_active->cursor_column > 0)
        {
            ozterm_move_cursor_diff(terminal, 0, -1);
        }
    }
    else if (c == '\t') {
        int16_t col = terminal->screen_active->cursor_column;
        int16_t spaces = TAB_WIDTH - (col % TAB_WIDTH);

        for (int16_t i = 0; i < spaces; ++i)
        {
            ozterm_put_character_and_cursor(terminal, ' ');
        }
    }
    else if (isgraph(c) || isspace(c))
    {
       // Auto-wrap logic
        if (terminal->screen_active->cursor_column >= terminal->column_count)
        {
            terminal->screen_active->cursor_column = 0;

            if (terminal->screen_active->cursor_row == terminal->scroll_bottom)
            {
                ozterm_scroll_up(terminal, 1);
            }
            else
            {
                terminal->screen_active->cursor_row++;
            }
        }

        ozterm_set_character(terminal, terminal->screen_active->cursor_row, terminal->screen_active->cursor_column, c, 1);

        ozterm_move_cursor(terminal, terminal->screen_active->cursor_row, terminal->screen_active->cursor_column + 1);
    }
}


static void ozterm_put_character(Ozterm* terminal, uint8_t c)
{
    static enum ParseState { STATE_NORMAL, STATE_ESC, STATE_CSI, STATE_OSC, STATE_G0, STATE_G1, STATE_HASH } parse_state = STATE_NORMAL;
    static char param_buf[32];
    static int param_len = 0;
    static char seq_buf[64];
    static int seq_len = 0;
    static char osc_buf[64];
    static int osc_index = 0;
    static char final_byte = 0;
    static uint8_t is_private = 0;

    //print_debug_character(c);

    switch (parse_state)
    {
        case STATE_NORMAL:
            if (c == '\033')
            {
                parse_state = STATE_ESC;
            } 
            else
            {
                if ((c >= 0x20 && c <= 0x7E) || c == '\n' || c == '\r' || c == '\b' || c == '\t')
                {  
                    ozterm_put_character_and_cursor(terminal, c);
                }
            }
            break;

        case STATE_ESC:
            if (c == '[')
            {
                parse_state = STATE_CSI;
                param_len = 0;
                seq_len = 0;
                is_private = 0;
                param_buf[0] = '\0';
                seq_buf[0] = '\0';
            }
            else if (c == ']')
            {
                parse_state = STATE_OSC;
                osc_index = 0;
                osc_buf[0] = '\0';
            }
            else if (c == '(')
            {
                parse_state = STATE_G0;
            }
            else if (c == ')')
            {
                parse_state = STATE_G1;
            }
            else if (c == '#')
            {
                parse_state = STATE_HASH;
            }
            else if (c == '7')
            {
                terminal->saved_cursor_row = terminal->screen_active->cursor_row;
                terminal->saved_cursor_column = terminal->screen_active->cursor_column;
                parse_state = STATE_NORMAL;
            }
            else if (c == '8')
            {
                ozterm_move_cursor(terminal, terminal->saved_cursor_row, terminal->saved_cursor_column);
                parse_state = STATE_NORMAL;
            }
            else if (c == 'c')
            {
                // ESC c — Full reset (RIS).
                ozterm_clear(terminal);
                ozterm_move_cursor(terminal, 0, 0);
                parse_state = STATE_NORMAL;
            }
            else if (c == 'D')
            {
                // ESC D — Index: Move cursor down
                ozterm_move_cursor_diff(terminal, 1, 0);
                parse_state = STATE_NORMAL;
            }
            else if (c == 'E')
            {
                // ESC E — Next line (CR + LF)
                ozterm_move_cursor(terminal, terminal->screen_active->cursor_row + 1, 0);
                parse_state = STATE_NORMAL;
            }
            else if (c == 'M')
            {
                // ESC M — Reverse index (scroll down)
                ozterm_scroll_down_region(terminal, 1);
                parse_state = STATE_NORMAL;
            }
            else if (c == 'Z')
            {
                // ESC Z — Identify terminal (DECID), reply with ESC[?6c
                const char* reply = "\033[?6c";
                write_to_master(terminal, reply, strlen(reply));
                parse_state = STATE_NORMAL;
            } 
            else if (c == '\\')
            {
                // ESC \ — ST (used to end OSC), absorb silently
                parse_state = STATE_NORMAL;
            }
            else
            {
                parse_state = STATE_NORMAL;
            }
            break;
        case STATE_OSC:
            if (c == '\a')
            {  // BEL = end of OSC
                parse_state = STATE_NORMAL;
                osc_buf[osc_index] = '\0';
            }
            else if (c == '\033')
            {
                // ESC — maybe ST terminator?
                parse_state = STATE_ESC;  // check for ESC \ in next char
            }
            else if (osc_index < sizeof(osc_buf) - 1)
            {
                osc_buf[osc_index++] = c;
                osc_buf[osc_index] = '\0';
            }
            break;
            case STATE_G0:
            case STATE_G1:
                // Valid values: 'B' (ASCII), '0' (line drawing), etc.
                parse_state = STATE_NORMAL;
                break;
            case STATE_HASH:
                if (c == '8') {
                    // Fill entire screen with 'E'
                    for (int y = 0; y < terminal->row_count; ++y)
                    {
                        for (int x = 0; x < terminal->column_count; ++x)
                        {
                            ozterm_set_character(terminal, y, x, 'E', 0);
                        }
                    }
                    if (terminal->refresh_function)
                        terminal->refresh_function(terminal);

                    ozterm_move_cursor(terminal, 0, 0);
                }
                parse_state = STATE_NORMAL;

                break;

        case STATE_CSI:
            if (seq_len < (int)sizeof(seq_buf) - 1)
            {
                seq_buf[seq_len++] = c;
                seq_buf[seq_len] = '\0';
            }

            // Recognize private mode prefix
            if (c == '?' || c == '>')
            {
                is_private = 1;
                break;  // Do not add to param_buf
            }

            // Collect parameters
            if ((c >= '0' && c <= '9') || c == ';')
            {
                if (param_len < (int)sizeof(param_buf) - 1)
                {
                    param_buf[param_len++] = c;
                    param_buf[param_len] = '\0';
                }
                break;
            }

            // Final byte detected
            if (c < '@' || c > '~')
            {
                parse_state = STATE_NORMAL;
                param_len = 0;
                seq_len = 0;
                break;
            }

            final_byte = c;
            const char* effective_param = param_buf;

            int p1 = 1, p2 = 1;
            
            char *semi = strchr(effective_param, ';');
            if (semi) {
                *semi = '\0';
                p1 = atoi(effective_param);
                p2 = atoi(semi + 1);
                *semi = ';';  // restore separator (optional)
            } else if (*effective_param) {
                p1 = atoi(effective_param);
            }

            int handled = 1;

            switch (final_byte)
            {
                case 'A': ozterm_move_cursor_diff(terminal, -p1, 0); break;
                case 'B': ozterm_move_cursor_diff(terminal, p1, 0); break;
                case 'C': ozterm_move_cursor_diff(terminal, 0, p1); break;
                case 'D': ozterm_move_cursor_diff(terminal, 0, -p1); break;
                case 'H': case 'f':
                    ozterm_move_cursor(terminal, p1 > 0 ? p1 - 1 : 0, p2 > 0 ? p2 - 1 : 0);
                    break;
                case 'd':
                    ozterm_move_cursor(terminal, p1 > 0 ? p1 - 1 : 0, terminal->screen_active->cursor_column);
                    break;
                case 'G':
                    ozterm_move_cursor(terminal, terminal->screen_active->cursor_row, p1 > 0 ? p1 - 1 : 0);
                    break;
                case 'n':
                    if (strcmp(effective_param, "6") == 0)
                    {
                        char reply[32];
                        snprintf(reply, sizeof(reply), "\033[%d;%dR",
                            terminal->screen_active->cursor_row + 1,
                            terminal->screen_active->cursor_column + 1);
                        write_to_master(terminal, reply, strlen(reply));
                    }
                    else
                    {
                        handled = 0;
                    }
                    break;
                case 'J': {
                    int mode = 0;
                    if (effective_param[0])
                        mode = atoi(effective_param);

                    int cy = terminal->screen_active->cursor_row;
                    int cx = terminal->screen_active->cursor_column;

                    switch (mode)
                    {
                        case 0:  // From cursor to end of screen
                            for (int y = cy; y < terminal->row_count; ++y)
                            {
                                int x_start = (y == cy) ? cx : 0;
                                for (int x = x_start; x < terminal->column_count; ++x)
                                {
                                    ozterm_set_character(terminal, y, x, ' ', 1);
                                }
                            }
                            break;

                        case 1:  // From top to cursor
                            for (int y = 0; y <= cy; ++y)
                            {
                                int x_end = (y == cy) ? cx + 1 : terminal->column_count;
                                for (int x = 0; x < x_end; ++x)
                                {
                                    ozterm_set_character(terminal, y, x, ' ', 1);
                                }
                            }
                            break;

                        case 2:  // Entire screen
                        default:
                            for (int y = 0; y < terminal->row_count; ++y)
                            {
                                for (int x = 0; x < terminal->column_count; ++x)
                                {
                                    ozterm_set_character(terminal, y, x, ' ', 1);
                                }
                            }
                            break;
                    }

                    break;
                }
                case 'K': {
                    int y = terminal->screen_active->cursor_row;
                    int x_start = 0, x_end = terminal->column_count;

                    // Default to 0 (erase right)
                    int mode = 0;
                    if (effective_param[0])
                        mode = atoi(effective_param);

                    switch (mode)
                    {
                        case 0:  // Erase to right
                            x_start = terminal->screen_active->cursor_column;
                            break;
                        case 1:  // Erase to left
                            x_end = terminal->screen_active->cursor_column + 1;
                            break;
                        case 2:  // Erase entire line
                            break;
                        default:
                            break;
                    }

                    for (int x = x_start; x < x_end; ++x)
                    {
                        ozterm_set_character(terminal, y, x, ' ', 1);
                    }
                    break;
                }
                case 'm': {
                    handled = 1;  // will reset to 0 only if nothing matches
                    char* p = param_buf;
                    if (p)
                    {
                        if (*p == '\0')
                        {
                            ozterm_reset_attributes(terminal);
                        }
                        else
                        {
                            while (*p)
                            {
                                int code = strtol(p, &p, 10);

                                if (code == 0)
                                    ozterm_reset_attributes(terminal);
                                else if (code >= 30 && code <= 37)
                                {
                                    terminal->screen_active->fg_color.index = code - 30;
                                    terminal->screen_active->fg_color.use_rgb = 0;
                                }
                                else if (code >= 40 && code <= 47)
                                {
                                    terminal->screen_active->bg_color.index = code - 40;
                                    terminal->screen_active->bg_color.use_rgb = 0;
                                }
                                else if (code >= 90 && code <= 97)
                                {
                                    terminal->screen_active->fg_color.index = code - 90 + 8;
                                    terminal->screen_active->fg_color.use_rgb = 0;
                                }
                                else if (code >= 100 && code <= 107)
                                {
                                    terminal->screen_active->bg_color.index = code - 100 + 8;
                                    terminal->screen_active->bg_color.use_rgb = 0;
                                }
                                else if (code == 7)
                                    terminal->screen_active->attr_inverse = 1;
                                else if (code == 27)
                                    terminal->screen_active->attr_inverse = 0;
                                else if (code == 39)
                                    terminal->screen_active->fg_color = terminal->fg_color_default;
                                else if (code == 49)
                                    terminal->screen_active->bg_color = terminal->bg_color_default;
                                else if (code == 38 || code == 48)
                                {
                                    // Extended color: 5;<idx> = 256-color, 2;<r>;<g>;<b> = truecolor
                                    // ptr p is right after the “38” or “48”
                                    if (*p == ';') p++;               // skip the ‘;’
                                    int mode = strtol(p, &p, 10);
                                    if (mode == 5 && *p == ';')
                                    {
                                        // 256-color mode
                                        p++;
                                        int idx = strtol(p, &p, 10);
                                        if (code == 38)
                                        {
                                            terminal->screen_active->fg_color.index = (uint8_t)idx;
                                            terminal->screen_active->fg_color.use_rgb = 0;
                                        }
                                        else
                                        {
                                            terminal->screen_active->bg_color.index = (uint8_t)idx;
                                            terminal->screen_active->bg_color.use_rgb = 0;
                                        }
                                    }
                                    else if (mode == 2 && *p == ';') {
                                        // true-color mode (r;g;b)
                                        p++;
                                        int r = strtol(p, &p, 10);
                                        if (*p == ';') p++;
                                        int g = strtol(p, &p, 10);
                                        if (*p == ';') p++;
                                        int b = strtol(p, &p, 10);
                                        if (code == 38)
                                        {
                                            terminal->screen_active->fg_color.red = (uint8_t)r;
                                            terminal->screen_active->fg_color.green = (uint8_t)g;
                                            terminal->screen_active->fg_color.blue = (uint8_t)b;
                                            terminal->screen_active->fg_color.use_rgb = 1;
                                        }
                                        else
                                        {
                                            terminal->screen_active->bg_color.red = (uint8_t)r;
                                            terminal->screen_active->bg_color.green = (uint8_t)g;
                                            terminal->screen_active->bg_color.blue = (uint8_t)b;
                                            terminal->screen_active->bg_color.use_rgb = 1;
                                        }
                                    }
                                    // else: unsupported sub-mode, ignore
                                }
                                else
                                    handled = 0;

                                if (*p == ';') p++;  // skip to next param
                            }
                        }
                    }
                    break;
                }
                case 'h':
                    if (is_private && strcmp(effective_param, "1049") == 0)
                    {
                        ozterm_switch_to_alt_screen(terminal);
                    }
                    else if (is_private && strcmp(effective_param, "2004") == 0)
                    {
                        // Enable bracketed paste mode
                    }
                    else if (is_private && strcmp(effective_param, "25") == 0)
                    {
                        //terminal->cursor_visible = true;
                    }
                    else if (is_private && strcmp(effective_param, "12") == 0)
                    {
                        // enable cursor blink
                    }
                    else if (is_private && strcmp(effective_param, "7") == 0)
                    {
                        //terminal->autowrap_enabled = true;
                    }
                    else if (is_private && strcmp(effective_param, "8") == 0)
                    {
                        //set auto-repeat: no need to implement
                    }
                    else if (is_private && strcmp(effective_param, "1") == 0)
                    {
                        terminal->DECCKM = 1;
                    }
                    else if (is_private && strcmp(effective_param, "3") == 0)
                    {
                        //DECCOLM: Set number of columns to 132 : ignore
                    }
                    else
                    {
                        handled = 0;
                    }
                    break;
                case 'l':
                    if (is_private && strcmp(effective_param, "1049") == 0)
                    {
                        ozterm_restore_main_screen(terminal);
                    }
                    else if (is_private && strcmp(effective_param, "2004") == 0)
                    {
                        // Disable bracketed paste mode
                    }
                    else if (is_private && strcmp(effective_param, "25") == 0)
                    {
                        // terminal->cursor_visible = false;
                    }
                    else if (is_private && strcmp(effective_param, "12") == 0)
                    {
                        // disable cursor blink
                    }
                    else if (is_private && strcmp(effective_param, "7") == 0)
                    {
                        //terminal->autowrap_enabled = false;
                    }
                    else if (is_private && strcmp(effective_param, "8") == 0)
                    {
                        //reset auto-repeat: no need to implement
                    }
                    else if (is_private && strcmp(effective_param, "1") == 0)
                    {
                        terminal->DECCKM = 0;
                    }
                    else if (is_private && strcmp(effective_param, "3") == 0)
                    {
                        //DECCOLM: Set number of columns to 132 : ignore
                    }
                    else
                    {
                        handled = 0;
                    }
                    break;
                case 't':
                {
                    if (strcmp(effective_param, "11") == 0)
                    {
                        const char* reply = "\033[1t"; // Window is visible
                        write_to_master(terminal, reply, strlen(reply));
                    }
                    else if (strncmp(param_buf, "22;", 3) == 0)
                    {
                        // Ignore all title stack ops
                    }
                    else if (strncmp(param_buf, "23;", 3) == 0)
                    {
                        // Ignore icon name stack ops
                    }
                    else
                    {
                        handled = 0;
                    }
                    break;
                }
                case 'c':
                    if (is_private)
                    {
                        const char* reply = "\033[>0;0;0c";
                        write_to_master(terminal, reply, strlen(reply));
                    }
                    else
                    {
                        if (strcmp(param_buf, "0") == 0) //CSI [0c (DA request)
                        {
                            const char* reply = "\033[?1;0c";
                            write_to_master(terminal, reply, strlen(reply));
                        }
                        else
                        {
                            handled = 0;
                        }
                    }
                    break;
                case '@':
                    ozterm_line_insert_characters(terminal, ' ', p1 > 0 ? p1 : 1);
                    break;
                case 'P':
                    ozterm_line_delete_characters(terminal, p1 > 0 ? p1 : 1);
                    break;
                case 'r':
                    if (p1 >= 1 && p2 >= 1 && p1 <= terminal->row_count && p2 <= terminal->row_count)
                    {
                        terminal->scroll_top = p1 - 1;
                        terminal->scroll_bottom = p2 - 1;
                    }
                    else
                    {
                        terminal->scroll_top = 0;
                        terminal->scroll_bottom = terminal->row_count - 1;
                    }
                    break;
                case 'M':
                    {
                        int y = terminal->screen_active->cursor_row;
                        int count = (p1 > 0 ? p1 : 1);
                        ozterm_delete_lines(terminal, y, count);
                        break;
                    }

                case 'L':
                    {
                        int y = terminal->screen_active->cursor_row;
                        int count = (p1 > 0 ? p1 : 1);
                        ozterm_insert_lines(terminal, y, count);
                        break;
                    }
                case 'S':
                    {
                        int count = (p1 > 0) ? p1 : 1;
                        ozterm_scroll_up_region(terminal, count);
                        break;
                    }
                case 'T':
                    {
                        int count = (p1 > 0) ? p1 : 1;
                        ozterm_scroll_down_region(terminal, count);
                        break;
                    }
                default:
                    handled = 0;
                    break;
            }

            if (!handled)
            {
                printf("Unhandled CSI sequence: CSI [%s%s%c\n",
                    is_private ? "?" : "",
                    param_buf[0] ? param_buf : "",
                    final_byte);
            }

            //fprintf(stderr, "CSI parsed: [%s%c\n", param_buf, final_byte);

            parse_state = STATE_NORMAL;
            param_len = 0;
            seq_len = 0;
            
            break;
    }
}

void ozterm_put_text(Ozterm* terminal, const uint8_t* text, int32_t size)
{
    const uint8_t* c = text;
    int32_t i = 0;
    while (*c && i < size)
    {
        ozterm_put_character(terminal, *c);
        ++c;
        ++i;
    }
}

static void ozterm_move_cursor(Ozterm* terminal, int16_t row, int16_t column)
{
    if (row >= terminal->row_count)
    {
        row = terminal->row_count - 1;
    }

    if (row < 0)
    {
        row = 0;
    }

    if (column >= terminal->column_count)
    {
        column = terminal->column_count - 1;
    }

    if (column < 0)
    {
        column = 0;
    }

    if (terminal->move_cursor_function)
    {
        terminal->move_cursor_function(terminal, terminal->screen_active->cursor_row, terminal->screen_active->cursor_column, row, column);
    }

    terminal->screen_active->cursor_row = row;
    terminal->screen_active->cursor_column = column;
}

static void ozterm_move_cursor_diff(Ozterm* terminal, int16_t row, int16_t column)
{
    ozterm_move_cursor(terminal, terminal->screen_active->cursor_row + row, terminal->screen_active->cursor_column + column);
}

static int write_csi_sequence(uint8_t *out, size_t max, int code, char final, int mod_value)
{
    if (mod_value <= 1)
    {
        if (code == 1)
            return snprintf((char*)out, max, "\033[%c", final); //emit like \033[H
        else
            return snprintf((char*)out, max, "\033[%d%c", code, final); //emit like \033[5~
    }
    else
    {
        return snprintf((char*)out, max, "\033[%d;%d%c", code, mod_value, final); //emit like \033[1;2H
    }
}

void ozterm_send_key(Ozterm* terminal, OztermKeyModifier modifier, uint8_t key)
{
    uint8_t seq[16];
    memset(seq, 0, 16);
    uint32_t size = 0;

    int mod_value = 1;

    if (modifier & (OZTERM_KEYM_LEFTSHIFT | OZTERM_KEYM_RIGHTSHIFT)) mod_value += 1;
    if (modifier & OZTERM_KEYM_ALT) mod_value += 2;
    if (modifier & OZTERM_KEYM_CTRL) mod_value += 4;

    switch (key)
    {
        // --- F1–F4 have alternate encoding (SS3 style) ---
        case OZTERM_KEY_F1:
        case OZTERM_KEY_F2:
        case OZTERM_KEY_F3:
        case OZTERM_KEY_F4: {
            const char base = 'P' + (key - OZTERM_KEY_F1); // 'P', 'Q', 'R', 'S'
            if (mod_value == 1)
            {
                // No modifier — use SS3
                seq[0] = '\033';
                seq[1] = 'O';
                seq[2] = base;
                size = 3;
            }
            else
            {
                // Modifier present — use CSI style
                size = write_csi_sequence(seq, sizeof(seq), 1, base, mod_value);
            }
            break;
        }

        // --- F5–F12 (standard CSI [NN~) ---
        case OZTERM_KEY_F5:  size = write_csi_sequence(seq, sizeof(seq), 15, '~', mod_value); break;
        case OZTERM_KEY_F6:  size = write_csi_sequence(seq, sizeof(seq), 17, '~', mod_value); break;
        case OZTERM_KEY_F7:  size = write_csi_sequence(seq, sizeof(seq), 18, '~', mod_value); break;
        case OZTERM_KEY_F8:  size = write_csi_sequence(seq, sizeof(seq), 19, '~', mod_value); break;
        case OZTERM_KEY_F9:  size = write_csi_sequence(seq, sizeof(seq), 20, '~', mod_value); break;
        case OZTERM_KEY_F10: size = write_csi_sequence(seq, sizeof(seq), 21, '~', mod_value); break;
        case OZTERM_KEY_F11: size = write_csi_sequence(seq, sizeof(seq), 23, '~', mod_value); break;
        case OZTERM_KEY_F12: size = write_csi_sequence(seq, sizeof(seq), 24, '~', mod_value); break;

        // --- Navigation + Editing keys ---
        case OZTERM_KEY_HOME:     size = write_csi_sequence(seq, sizeof(seq), 1, 'H', mod_value); break;
        case OZTERM_KEY_END:      size = write_csi_sequence(seq, sizeof(seq), 1, 'F', mod_value); break;
        case OZTERM_KEY_UP:
            if (terminal->DECCKM)
                size = snprintf((char*)seq, sizeof(seq), "\033OA");
            else
                size = write_csi_sequence(seq, sizeof(seq), 1, 'A', mod_value);
            break;
        case OZTERM_KEY_DOWN:
            if (terminal->DECCKM)
                size = snprintf((char*)seq, sizeof(seq), "\033OB");
            else
                size = write_csi_sequence(seq, sizeof(seq), 1, 'B', mod_value);
            break;
        case OZTERM_KEY_LEFT:
            if (terminal->DECCKM)
                size = snprintf((char*)seq, sizeof(seq), "\033OD");
            else
                size = write_csi_sequence(seq, sizeof(seq), 1, 'D', mod_value);
            break;
        case OZTERM_KEY_RIGHT:
            if (terminal->DECCKM)
                size = snprintf((char*)seq, sizeof(seq), "\033OC");
            else
                size = write_csi_sequence(seq, sizeof(seq), 1, 'C', mod_value);
            break;
        case OZTERM_KEY_PAGEUP:   size = write_csi_sequence(seq, sizeof(seq), 5, '~', mod_value); break;
        case OZTERM_KEY_PAGEDOWN: size = write_csi_sequence(seq, sizeof(seq), 6, '~', mod_value); break;
        case OZTERM_KEY_INSERT:   size = write_csi_sequence(seq, sizeof(seq), 2, '~', mod_value); break;
        case OZTERM_KEY_DELETE:   size = write_csi_sequence(seq, sizeof(seq), 3, '~', mod_value); break;


        // --- Control character keys ---
        case OZTERM_KEY_RETURN:    seq[0] = '\r';   size = 1; break;
        case OZTERM_KEY_BACKSPACE: seq[0] = 127;    size = 1; break;
        case OZTERM_KEY_ESCAPE:    seq[0] = '\033'; size = 1; break;
        case OZTERM_KEY_TAB:       seq[0] = '\t';   size = 1; break;
    default:
        // Handle Ctrl+key combos (Ctrl+A to Ctrl+Z)
        if (modifier == OZTERM_KEYM_CTRL)
        {
            if (isgraph(key))
            {
                uint8_t upper = toupper(key);
                key = C(upper);
            }
        }
        
        seq[0] = key;
        size = 1;
        
        break;
    }

    if (terminal->write_to_master_function && size > 0)
    {
        terminal->write_to_master_function(terminal, seq, size);
    }
}

void ozterm_have_read_from_master(Ozterm* terminal, const uint8_t* data, int32_t size)
{
    ozterm_put_text(terminal, data, size);
}