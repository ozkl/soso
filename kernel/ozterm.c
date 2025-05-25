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

#include "common.h"
#include "alloc.h"
#include "ozterm.h"

#define TAB_WIDTH 8

// C('A') == Control-A
#define C(x) (x - '@')

//Internal API
uint8_t ozterm_get_character(Ozterm * terminal, int16_t row, int16_t column);
void ozterm_clear(Ozterm* terminal);
void ozterm_clear_line_right(Ozterm* terminal);
void ozterm_line_insert_characters(Ozterm* terminal, uint8_t c, int16_t count);
void ozterm_line_delete_characters(Ozterm* terminal, int16_t count);
void ozterm_put_character(Ozterm* terminal, uint8_t c);
void ozterm_move_cursor(Ozterm* terminal, int16_t row, int16_t column);
void ozterm_move_cursor_diff(Ozterm* terminal, int16_t row, int16_t column);
void ozterm_scroll_up_region(Ozterm* terminal, int lines);
void ozterm_scroll_down_region(Ozterm* terminal, int lines);


Ozterm* ozterm_create(uint16_t row_count, uint16_t column_count)
{
    Ozterm* terminal = kmalloc(sizeof(Ozterm));
    memset((uint8_t*)terminal, 0, sizeof(Ozterm));

    terminal->screen_main = kmalloc(sizeof(OztermScreen));
    terminal->screen_main->buffer = kmalloc(row_count * column_count * sizeof(OztermCell));
    terminal->screen_main->cursor_column = 0;
    terminal->screen_main->cursor_row = 0;

    terminal->screen_alternative = kmalloc(sizeof(OztermScreen));
    terminal->screen_alternative->buffer = kmalloc(row_count * column_count * sizeof(OztermCell));
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
    terminal->color = 0x0A;

    ozterm_clear(terminal);

    return terminal;
}

void ozterm_destroy(Ozterm* terminal)
{
    kfree(terminal->screen_main->buffer);
    kfree(terminal->screen_main);
    kfree(terminal->screen_alternative->buffer);
    kfree(terminal->screen_alternative);
    kfree(terminal);
}

static void write_to_master(Ozterm* terminal, const char* data, int32_t size)
{
    if (terminal->write_to_master_function && size > 0)
    {
        terminal->write_to_master_function(terminal, (uint8_t*)data, size);
    }
}

void ozterm_switch_to_alt_screen(Ozterm* terminal)
{
    terminal->alternative_active = 1;
    terminal->screen_active = terminal->screen_alternative;
    
    ozterm_clear(terminal);

    if (terminal->refresh_function)
    {
        terminal->refresh_function(terminal);
    }
}

void ozterm_restore_main_screen(Ozterm* terminal)
{
    terminal->alternative_active = 0;
    terminal->screen_active = terminal->screen_main;

    if (terminal->refresh_function)
    {
        terminal->refresh_function(terminal);
    }
}

uint8_t ozterm_get_character(Ozterm * terminal, int16_t row, int16_t column)
{
    OztermCell * video = terminal->screen_active->buffer + (row * terminal->column_count + column);
    return video->character;
}

void ozterm_scroll_up_region(Ozterm* terminal, int lines)
{
    if (lines <= 0) lines = 1;

    int top = terminal->scroll_top;
    int bottom = terminal->scroll_bottom;
    int columns = terminal->column_count;
    OztermCell* buf = terminal->screen_active->buffer;

    if (lines > bottom - top + 1)
        lines = bottom - top + 1;

    // Scroll UP: move lines from top+lines → top
    for (int row = top; row <= bottom - lines; ++row)
    {
        memcpy(buf + row * columns,
               buf + (row + lines) * columns,
               sizeof(OztermCell) * columns);
    }

    // Clear bottom lines
    for (int row = bottom - lines + 1; row <= bottom; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            int idx = row * columns + col;
            buf[idx].character = ' ';
            buf[idx].color = terminal->color;
        }
    }

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}


void ozterm_scroll_down_region(Ozterm* terminal, int lines)
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
        memcpy(buf + row * columns, buf + (row - lines) * columns, sizeof(OztermCell) * columns);
    }

    // Clear top N lines
    for (int row = top; row < top + lines; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            OztermCell* cell = &buf[row * columns + col];
            cell->character = ' ';
            cell->color = terminal->color;
        }
    }

    if (terminal->refresh_function)
        terminal->refresh_function(terminal);
}


void ozterm_clear(Ozterm* terminal)
{
    OztermCell * video = terminal->screen_active->buffer;
    int i = 0;

    for (i = 0; i < terminal->row_count * terminal->column_count; ++i)
    {
        video[i].character = ' ';
        video[i].color = terminal->color;
    }

    ozterm_move_cursor(terminal, 0, 0);
}

void ozterm_clear_line_right(Ozterm* terminal)
{
    for (int x = terminal->screen_active->cursor_column; x < terminal->column_count; ++x)
    {
        OztermCell * video = terminal->screen_active->buffer + (terminal->screen_active->cursor_row * terminal->column_count + x);
        video->character = ' ';
        video->color = terminal->color;
    }
}

void ozterm_line_insert_characters(Ozterm* terminal, uint8_t c, int16_t count)
{
    OztermCell * video = terminal->screen_active->buffer + (terminal->screen_active->cursor_row * terminal->column_count);
    int16_t x = terminal->screen_active->cursor_column;

    if (x < terminal->column_count)
    {
        if (x + count >= terminal->column_count)
            count = terminal->column_count - x;
        
            // Shift everything to the right
        for (int i = terminal->column_count - 1; i >= x + count; --i)
        {
            video[i] = video[i - count];
        }

        // Fill inserted space
        for (int i = 0; i < count; ++i)
        {
            video[x + i].character = ' ';
            video[x + i].color = terminal->color;
        }
    }
}

void ozterm_line_delete_characters(Ozterm* terminal, int16_t count)
{
    OztermCell * video = terminal->screen_active->buffer + (terminal->screen_active->cursor_row * terminal->column_count);
    int16_t x = terminal->screen_active->cursor_column;

    // Cap delete range to what's left in line
    if (x < terminal->column_count)
    {
        if (x + count >= terminal->column_count)
            count = terminal->column_count - x;

        // Shift everything to the left
        for (int16_t i = x; i < terminal->column_count - count; ++i)
        {
            video[i] = video[i + count];
        }

        // Fill space
        for (int16_t i = terminal->column_count - count; i < terminal->column_count; ++i)
        {
            video[x + i].character = ' ';
            video[x + i].color = terminal->color;
        }
    }
}

void ozterm_put_character_and_cursor(Ozterm* terminal, uint8_t c)
{
    OztermCell * video = terminal->screen_active->buffer;

    if ('\n' == c)
    {
        if (terminal->screen_active->cursor_row == terminal->scroll_bottom)
        {
            // We're at bottom of scroll region — scroll up
            ozterm_scroll_up_region(terminal, 1);
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
    else if (c == '\t')
    {
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
                ozterm_scroll_up_region(terminal, 1);
            }
            else
            {
                terminal->screen_active->cursor_row++;
            }
        }

        video += (terminal->screen_active->cursor_row * terminal->column_count + terminal->screen_active->cursor_column);

        video->character = c;
        video->color = terminal->color;

        //at this point it is added to buffer already


        //visual update
        if (terminal->set_character_function)
        {
            terminal->set_character_function(terminal, terminal->screen_active->cursor_row, terminal->screen_active->cursor_column, c);
        }

        ozterm_move_cursor(terminal, terminal->screen_active->cursor_row, terminal->screen_active->cursor_column + 1);
    }
}

void ozterm_put_character(Ozterm* terminal, uint8_t c)
{
    static enum ParseState { STATE_NORMAL, STATE_ESC, STATE_CSI, STATE_OSC, STATE_G0, STATE_G1 } parse_state = STATE_NORMAL;
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
                case 'J':
                    if (strcmp(effective_param, "2") == 0)
                        ozterm_clear(terminal);
                    break;
                case 'K':
                    ozterm_clear_line_right(terminal);
                    break;
                case 'm': {
                    handled = 1;  // will reset to 0 only if nothing matches
                    char* p = param_buf;
                    while (p && *p)
                    {
                        int val = strtol(p, &p, 10);

                        switch (val)
                        {
                            case 0:
                                //terminal_reset_attrs(terminal);
                                break;
                            case 1:
                                //terminal_set_bold(terminal, true);
                                //printf("Bold\n");
                                break;
                            case 22:
                                //terminal_set_bold(terminal, false);
                                break;
                            case 31:
                                //ozterm_set_fg_color(terminal, RED);
                                break;
                            default:
                                // Optional: printf("Unhandled SGR: %d\n", val);
                                break;
                        }

                        if (*p == ';') p++;  // skip to next param
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
                        handled = 0;
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
                    if (y < terminal->scroll_top || y > terminal->scroll_bottom)
                        break;

                    OztermCell* buf = terminal->screen_active->buffer;
                    int lines_to_move = terminal->scroll_bottom - y;
                    for (int i = 0; i < lines_to_move; ++i)
                    {
                        memcpy(buf + (y + i) * terminal->column_count,
                            buf + (y + i + 1) * terminal->column_count,
                            sizeof(OztermCell) * terminal->column_count);
                    }

                    // Clear the last line
                    int last = terminal->scroll_bottom;
                    for (int j = 0; j < terminal->column_count; ++j)
                    {
                        buf[last * terminal->column_count + j].character = ' ';
                        buf[last * terminal->column_count + j].color = terminal->color;
                    }
                    break;
                }

                case 'L':
                {
                    int y = terminal->screen_active->cursor_row;
                    int count = (p1 > 0 ? p1 : 1);
                    if (y < terminal->scroll_top || y > terminal->scroll_bottom) break;
                    if (y + count > terminal->scroll_bottom + 1)
                        count = terminal->scroll_bottom - y + 1;
                    OztermCell* buf = terminal->screen_active->buffer;
                    for (int i = terminal->scroll_bottom; i >= y + count; --i)
                    {
                        memcpy(buf + i * terminal->column_count, buf + (i - count) * terminal->column_count, sizeof(OztermCell) * terminal->column_count);
                    }
                    for (int i = y; i < y + count; ++i)
                    {
                        for (int j = 0; j < terminal->column_count; ++j)
                        {
                            int index = i * terminal->column_count + j;
                            buf[index].character = ' ';
                            buf[index].color = terminal->color;
                        }
                    }
                    break;
                }
                case 'S':
                {
                    int count = (p1 > 0 ? p1 : 1);
                    OztermCell* buf = terminal->screen_active->buffer;
                    for (int i = terminal->scroll_top; i <= terminal->scroll_bottom - count; ++i)
                    {
                        memcpy(buf + i * terminal->column_count, buf + (i + count) * terminal->column_count, sizeof(OztermCell) * terminal->column_count);
                    }
                    for (int i = terminal->scroll_bottom - count + 1; i <= terminal->scroll_bottom; ++i)
                    {
                        for (int j = 0; j < terminal->column_count; ++j)
                        {
                            int index = i * terminal->column_count + j;
                            buf[index].character = ' ';
                            buf[index].color = terminal->color;
                        }
                    }
                    break;
                }
                case 'T':
                {
                    int count = (p1 > 0 ? p1 : 1);
                    OztermCell* buf = terminal->screen_active->buffer;
                    for (int i = terminal->scroll_bottom; i >= terminal->scroll_top + count; --i)
                    {
                        memcpy(buf + i * terminal->column_count, buf + (i - count) * terminal->column_count, sizeof(OztermCell) * terminal->column_count);
                    }
                    for (int i = terminal->scroll_top; i < terminal->scroll_top + count; ++i)
                    {
                        for (int j = 0; j < terminal->column_count; ++j)
                        {
                            int index = i * terminal->column_count + j;
                            buf[index].character = ' ';
                            buf[index].color = terminal->color;
                        }
                    }
                    break;
                }
                default:
                    handled = 0;
                    break;
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

void ozterm_move_cursor(Ozterm* terminal, int16_t row, int16_t column)
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

void ozterm_move_cursor_diff(Ozterm* terminal, int16_t row, int16_t column)
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
        case OZTERM_KEY_UP:       size = write_csi_sequence(seq, sizeof(seq), 1, 'A', mod_value); break;
        case OZTERM_KEY_DOWN:     size = write_csi_sequence(seq, sizeof(seq), 1, 'B', mod_value); break;
        case OZTERM_KEY_LEFT:     size = write_csi_sequence(seq, sizeof(seq), 1, 'D', mod_value); break;
        case OZTERM_KEY_RIGHT:    size = write_csi_sequence(seq, sizeof(seq), 1, 'C', mod_value); break;
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