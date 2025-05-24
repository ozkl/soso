#include "alloc.h"
#include "keymap.h"
#include "fs.h"
#include "console.h"
#include "kernelterminal_fb.h"
#include "kernelterminal_vga.h"
#include "kernelterminal.h"

static BOOL g_graphic_mode = FALSE;

static void master_read_ready(TtyDev* tty, uint32_t size);

static void refresh_callback(Ozterm* term);
static void set_character_callback(Ozterm* term, int16_t row, int16_t column, uint8_t character);
static void move_cursor_callback(Ozterm* term, int16_t old_row, int16_t old_column, int16_t row, int16_t column);
static void write_to_master_callback(Ozterm* term, const uint8_t* data, int32_t size);

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
    terminal->term->custom_data = terminal;
    terminal->term->refresh_function = refresh_callback;
    terminal->term->set_character_function = set_character_callback;
    terminal->term->move_cursor_function = move_cursor_callback;
    terminal->term->write_to_master_function = write_to_master_callback;

    terminal->opened_master = fs_open_for_process(NULL, tty->master_node, 0);
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

    ozterm_send_key(terminal->term, modifier, character);
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

static void refresh_callback(Ozterm* term)
{
    Terminal* terminal = (Terminal*)term->custom_data;

    if (g_active_terminal == terminal)
    {
        if (g_graphic_mode)
        {
            fbterminal_refresh(term->screen_active->buffer);
        }
        else
        {
            vgaterminal_refresh(term->screen_active->buffer);
        }
    }
}

static void set_character_callback(Ozterm* term, int16_t row, int16_t column, uint8_t character)
{
    Terminal* terminal = (Terminal*)term->custom_data;

    if (g_active_terminal == terminal)
    {
        if (g_graphic_mode)
        {
            fbterminal_set_character(row, column, character);
        }
        else
        {
            vgaterminal_set_character(row, column, character);
        }
    }
}

static void move_cursor_callback(Ozterm* term, int16_t old_row, int16_t old_column, int16_t row, int16_t column)
{
    Terminal* terminal = (Terminal*)term->custom_data;

    if (g_active_terminal == terminal)
    {
        if (g_graphic_mode)
        {
            fbterminal_move_cursor(term->screen_active->buffer, old_row, old_column, row, column);
        }
        else
        {
            vgaterminal_move_cursor(row, column);
        }
    }
}

static void write_to_master_callback(Ozterm* term, const uint8_t* data, int32_t size)
{
    Terminal* terminal = (Terminal*)term->custom_data;

    fs_write(terminal->opened_master, size, data);
}