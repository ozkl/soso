#include "gfx.h"
#include "fbterminal.h"

static void fbterminal_refreshTerminal(Terminal* terminal);
static void fbterminal_addCharacter(Terminal* terminal, uint8_t character);
static void fbterminal_moveCursor(Terminal* terminal, uint16_t old_line, uint16_t old_column, uint16_t line, uint16_t column);

void fbterminal_setup(Terminal* terminal)
{
    terminal->tty->winsize.ws_row = gfx_get_height() / 16;
    terminal->tty->winsize.ws_col = gfx_get_width() / 9;
    terminal->refresh_function = fbterminal_refreshTerminal;
    terminal->add_character_function = fbterminal_addCharacter;
    terminal->move_cursor_function = fbterminal_moveCursor;
}

static void fbterminal_refreshTerminal(Terminal* terminal)
{
    for (uint32_t r = 0; r < terminal->tty->winsize.ws_row; ++r)
    {
        for (uint32_t c = 0; c < terminal->tty->winsize.ws_col; ++c)
        {
            uint8_t* characterPos = terminal->buffer + (r * terminal->tty->winsize.ws_col + c) * 2;

            uint8_t chr = characterPos[0];
            uint8_t color = characterPos[1];

            gfx_put_char_at(chr, c, r, 0, 0xFFFFFFFF);
        }
    }
}

static void fbterminal_addCharacter(Terminal* terminal, uint8_t character)
{
    gfx_put_char_at(character, terminal->current_column, terminal->current_line, 0, 0xFFFFFFFF);
}

static void fbterminal_moveCursor(Terminal* terminal, uint16_t old_line, uint16_t old_column, uint16_t line, uint16_t column)
{
    //restore old place
    uint8_t* character_old = terminal->buffer + (old_line * terminal->tty->winsize.ws_col + old_column) * 2;
    gfx_put_char_at(*character_old, old_column, old_line, 0, 0xFFFFFFFF);

    //put cursor to new place
    uint8_t* character_new = terminal->buffer + (line * terminal->tty->winsize.ws_col + column) * 2;
    gfx_put_char_at(*character_new, column, line, 0xFFFF0000, 0);
}