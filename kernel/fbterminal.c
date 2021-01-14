#include "gfx.h"
#include "fbterminal.h"

static void fbterminal_refreshTerminal(Terminal* terminal);
static void fbterminal_addCharacter(Terminal* terminal, uint8 character);
static void fbterminal_moveCursor(Terminal* terminal, uint16 oldLine, uint16 oldColumn, uint16 line, uint16 column);

void fbterminal_setup(Terminal* terminal)
{
    terminal->tty->winsize.ws_row = Gfx_GetHeight() / 16;
    terminal->tty->winsize.ws_col = Gfx_GetWidth() / 9;
    terminal->refreshFunction = fbterminal_refreshTerminal;
    terminal->addCharacterFunction = fbterminal_addCharacter;
    terminal->moveCursorFunction = fbterminal_moveCursor;
}

static void fbterminal_refreshTerminal(Terminal* terminal)
{
    for (uint32 r = 0; r < terminal->tty->winsize.ws_row; ++r)
    {
        for (uint32 c = 0; c < terminal->tty->winsize.ws_col; ++c)
        {
            uint8* characterPos = terminal->buffer + (r * terminal->tty->winsize.ws_col + c) * 2;

            uint8 chr = characterPos[0];
            uint8 color = characterPos[1];

            Gfx_PutCharAt(chr, c, r, 0, 0xFFFFFFFF);
        }
    }
}

static void fbterminal_addCharacter(Terminal* terminal, uint8 character)
{
    Gfx_PutCharAt(character, terminal->current_column, terminal->current_line, 0, 0xFFFFFFFF);
}

static void fbterminal_moveCursor(Terminal* terminal, uint16 oldLine, uint16 oldColumn, uint16 line, uint16 column)
{
    //restore old place
    uint8* character_old = terminal->buffer + (oldLine * terminal->tty->winsize.ws_col + oldColumn) * 2;
    Gfx_PutCharAt(*character_old, oldColumn, oldLine, 0, 0xFFFFFFFF);

    //put cursor to new place
    uint8* character_new = terminal->buffer + (line * terminal->tty->winsize.ws_col + column) * 2;
    Gfx_PutCharAt(*character_new, column, line, 0xFFFF0000, 0);
}