#include "alloc.h"
#include "keymap.h"
#include "fs.h"
#include "console.h"
#include "process.h"
#include "kernelterminal_fb.h"
#include "kernelterminal_vga.h"
#include "kernelterminal.h"

static BOOL g_graphic_mode = FALSE;

static void master_read_ready(TtyDev* tty, uint32_t size);

static void refresh_callback(Ozterm* term);
static void set_character_callback(Ozterm* term, int16_t row, int16_t column, OztermCell* cell);
static void move_cursor_callback(Ozterm* term, int16_t old_row, int16_t old_column, int16_t row, int16_t column);
static void write_to_master_callback(Ozterm* term, const uint8_t* data, int32_t size);

typedef struct CellColor
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} CellColor;

static CellColor g_ansi_colors[16] = {
    {0, 0, 0},       // Black
    {205, 0, 0},     // Red
    {0, 205, 0},     // Green
    {205, 205, 0},   // Yellow
    {0, 0, 238},     // Blue
    {205, 0, 205},   // Magenta
    {0, 205, 205},   // Cyan
    {229, 229, 229}, // White (light gray)

    {127, 127, 127}, // Bright Black (dark gray)
    {255, 0, 0},     // Bright Red
    {0, 255, 0},     // Bright Green
    {255, 255, 0},   // Bright Yellow
    {92, 92, 255},   // Bright Blue
    {255, 0, 255},   // Bright Magenta
    {0, 255, 255},   // Bright Cyan
    {255, 255, 255}  // Bright White
};

Terminal* terminal_create(BOOL graphic_mode)
{
    Terminal* terminal = kmalloc(sizeof(Terminal));
    memset((uint8_t*)terminal, 0, sizeof(Terminal));

    uint16_t row_count = graphic_mode ? fbterminal_get_row_count() : vgaterminal_get_row_count();
    uint16_t column_count = graphic_mode ? fbterminal_get_column_count() : vgaterminal_get_column_count();

    FileSystemNode* ttyNode = ttydev_create(column_count, row_count);
    TtyDev* tty = (TtyDev*)ttyNode->private_node_data;

    terminal->tty = tty;

    g_graphic_mode = graphic_mode;

    terminal->term = ozterm_create(terminal->tty->winsize.ws_row, terminal->tty->winsize.ws_col);
    ozterm_set_custom_data(terminal->term, terminal);
    ozterm_set_render_callbacks(terminal->term, refresh_callback, set_character_callback, move_cursor_callback);
    ozterm_set_write_to_master_callback(terminal->term, write_to_master_callback);

    OztermColor default_fg = {.use_rgb = 0, .index = 0, .red = 0, .green = 0, .blue = 0};
    OztermColor default_bg = {.use_rgb = 0, .index = 15, .red = 0, .green = 0, .blue = 0};
    ozterm_set_default_color(terminal->term, default_fg, default_bg);
    ozterm_reset_attributes(terminal->term);
    ozterm_clear_full(terminal->term);

    terminal->opened_master = fs_open_for_process(thread_get_first(), tty->master_node, 0);
    terminal->disabled = FALSE;
    tty->private_data = terminal;

    tty->master_read_ready = master_read_ready;

    return terminal;
}

void terminal_destroy(Terminal* terminal)
{
    fs_close(terminal->opened_master);

    ozterm_destroy(terminal->term);

    kfree(terminal);
}


void terminal_put_text(Terminal* terminal, const uint8_t* text, uint32_t size)
{
    if (terminal->disabled)
    {
        return;
    }

    ozterm_put_text(terminal->term, text, size);
}

void terminal_send_key(Terminal* terminal, uint8_t modifier, uint8_t character)
{
    if (terminal->disabled)
    {
        return;
    }

    if ((modifier & KM_LeftShift) == KM_LeftShift && (character == KEY_PAGEUP || character == KEY_PAGEDOWN))
    {
        int16_t offset = character == KEY_PAGEUP ? 1 : -1;

        ozterm_scroll(terminal->term, ozterm_get_scroll(terminal->term) + offset);
    }
    else
    {
        ozterm_send_key(terminal->term, modifier, character);
    }
}

static void master_read_ready(TtyDev* tty, uint32_t size)
{
    Terminal* terminal = (Terminal*)tty->private_data;

    uint8_t characters[128];
    int32_t bytes = 0;
    do
    {
        bytes = ttydev_master_read_nonblock(terminal->opened_master, 8, characters);

        if (bytes > 0)
        {
            ozterm_have_read_from_master(terminal->term, characters, bytes);
        }
    } while (bytes > 0);
}

static CellColor convert_color(OztermColor color, OztermColor * default_color)
{
    CellColor result;

    if (color.use_rgb)
    {
        result.r = color.red;
        result.g = color.green;
        result.b = color.blue;
        result.a = 255;
    }
    else
    {
        if (color.index >= 0 && color.index < sizeof(g_ansi_colors))
        {
            result = g_ansi_colors[color.index];
        }
        else
        {
            if (default_color)
            {
                result = convert_color(*default_color, NULL);
            }
        }
    }

    return result;
}

static BOOL is_color_same(OztermColor color1, OztermColor color2)
{
    if (color1.use_rgb && color2.use_rgb)
    {
        if (color1.red == color2.red && color1.green == color2.green && color1.blue == color2.blue)
        {
            return TRUE;
        }
    }
    else if (color1.use_rgb == 0 && color2.use_rgb == 0)
    {
        if (color1.index == color2.index)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void refresh_callback(Ozterm* term)
{
    Terminal* terminal = (Terminal*)ozterm_get_custom_data(term);

    if (terminal->disabled)
    {
        return;
    }

    if (g_active_terminal == terminal)
    {
        OztermColor default_fg;
        OztermColor default_bg;
        ozterm_get_default_color(term, &default_fg, &default_bg);
        int16_t row_count = ozterm_get_row_count(term);
        int16_t column_count = ozterm_get_column_count(term);
        for (int16_t row = 0; row < row_count; ++row)
        {
            OztermCell* row_data = ozterm_get_row_data(term, row);
            for (int16_t column = 0; column < column_count; ++column)
            {
                OztermCell* cell = row_data + column;

                CellColor color_fg = convert_color(cell->fg_color, &default_fg);
                CellColor color_bg = convert_color(cell->bg_color, &default_bg);
                
                if (g_graphic_mode)
                    fbterminal_set_character(row, column, cell->character, *(uint32_t*)&color_fg, *(uint32_t*)&color_bg);
                else
                    vgaterminal_set_character(row, column, cell->character);
            }
        }

        int16_t scroll = ozterm_get_scroll(term);
        if (scroll == 0)
        {
            int16_t cursor_row = ozterm_get_cursor_row(term);
            int16_t cursor_column = ozterm_get_cursor_column(term);
            move_cursor_callback(term, cursor_row, cursor_column, cursor_row, cursor_column);
        }
    }
}

static void set_character_callback(Ozterm* term, int16_t row, int16_t column, OztermCell* cell)
{
    Terminal* terminal = (Terminal*)ozterm_get_custom_data(term);

    if (terminal->disabled)
    {
        return;
    }

    if (g_active_terminal == terminal)
    {
        if (g_graphic_mode)
        {
            OztermColor default_fg;
            OztermColor default_bg;
            ozterm_get_default_color(term, &default_fg, &default_bg);

            CellColor color_fg = convert_color(cell->fg_color, &default_fg);
            CellColor color_bg = convert_color(cell->bg_color, &default_bg);
                
            fbterminal_set_character(row, column, cell->character, *(uint32_t*)&color_fg, *(uint32_t*)&color_bg);
        }
        else
        {
            vgaterminal_set_character(row, column, cell->character);
        }

        int16_t scroll = ozterm_get_scroll(term);
        if (scroll != 0)
        {
            ozterm_scroll(term, 0);
        }
    }
}

static void move_cursor_callback(Ozterm* term, int16_t old_row, int16_t old_column, int16_t row, int16_t column)
{
    Terminal* terminal = (Terminal*)ozterm_get_custom_data(term);

    if (terminal->disabled)
    {
        return;
    }

    if (g_active_terminal == terminal)
    {
        if (g_graphic_mode)
        {
            OztermColor default_fg;
            OztermColor default_bg;
            ozterm_get_default_color(term, &default_fg, &default_bg);

            OztermCell* old_row_data = ozterm_get_row_data(term, old_row);
            OztermCell* old_cell = old_row_data + old_column;
            
            CellColor color_fg = convert_color(old_cell->fg_color, &default_fg);
            CellColor color_bg = convert_color(old_cell->bg_color, &default_bg);

            fbterminal_set_character(old_row, old_column, old_cell->character, *(uint32_t*)&color_fg, *(uint32_t*)&color_bg);

            //old cursor pos character restored
            //now draw cursor

            OztermCell* row_data = ozterm_get_row_data(term, row);
            OztermCell* cell = row_data + column;

            if (is_color_same(cell->fg_color, cell->bg_color))
            {
                color_bg = convert_color(default_fg, NULL);
                color_fg = convert_color(default_bg, NULL);
            }
            else
            {
                color_fg = convert_color(cell->bg_color, NULL);
                color_bg = convert_color(cell->fg_color, NULL);
            }
            
            fbterminal_set_character(row, column, cell->character, *(uint32_t*)&color_fg, *(uint32_t*)&color_bg);
        }
        else
        {
            vgaterminal_move_cursor(row, column);
        }
    }
}

static void write_to_master_callback(Ozterm* term, const uint8_t* data, int32_t size)
{
    Terminal* terminal = (Terminal*)ozterm_get_custom_data(term);

    fs_write(terminal->opened_master, size, (uint8_t*)data);
}