#include "alloc.h"
#include "keymap.h"
#include "fs.h"
#include "console.h"
#include "fbterminal.h"
#include "vgaterminal.h"
#include "terminal.h"

static void master_read_ready(TtyDev* tty, uint32 size);

Terminal* Terminal_create(TtyDev* tty, BOOL graphicMode)
{
    Terminal* terminal = kmalloc(sizeof(Terminal));
    memset((uint8*)terminal, 0, sizeof(Terminal));

    terminal->tty = tty;
    if (graphicMode)
    {
        fbterminal_setup(terminal);
    }
    else
    {
        vgaterminal_setup(terminal);
    }

    terminal->buffer = kmalloc(terminal->tty->winsize.ws_row * terminal->tty->winsize.ws_col * 2);
    terminal->currentColumn = 0;
    terminal->currentLine = 0;
    terminal->color = 0x0A;

    Terminal_clear(terminal);

    terminal->openedMaster = open_fs_forProcess(NULL, tty->masterNode, 0);
    tty->privateData = terminal;

    tty->masterReadReady = master_read_ready;

    return terminal;
}

void Terminal_destroy(Terminal* terminal)
{
    close_fs(terminal->openedMaster);

    kfree(terminal->buffer);
    kfree(terminal);
}

void Terminal_print(Terminal* terminal, int row, int column, const char* text)
{
    unsigned char * video = terminal->buffer;

    video += (row * terminal->tty->winsize.ws_col + column) * 2;
    while(*text != 0)
    {
        *video++ = *text++;
        *video++ = terminal->color;

        //TODO: check buffer end
    }
}

//One line
void Terminal_scrollUp(Terminal* terminal)
{
    unsigned char * videoLine = terminal->buffer;
    unsigned char * videoLineNext = terminal->buffer;
    int line = 0;
    int column = 0;

    for (line = 0; line < terminal->tty->winsize.ws_row - 1; ++line)
    {
        for (column = 0; column < terminal->tty->winsize.ws_col; ++column)
        {
            videoLine = terminal->buffer + (line * terminal->tty->winsize.ws_col + column) * 2;
            videoLineNext = terminal->buffer + ((line + 1) * terminal->tty->winsize.ws_col + column) * 2;

            videoLine[0] = videoLineNext[0];
            videoLine[1] = videoLineNext[1];
        }
    }

    //Last line should be empty.
    unsigned char * lastLine = terminal->buffer + ((terminal->tty->winsize.ws_row - 1) * terminal->tty->winsize.ws_col) * 2;
    for (int i = 0; i < terminal->tty->winsize.ws_col * 2; i += 2)
    {
        lastLine[i] = 0;
        lastLine[i + 1] = terminal->color;
    }

    if (gActiveTerminal == terminal)
    {
        if (terminal->refreshFunction)
        {
            terminal->refreshFunction(terminal);
        }
    }
}

void Terminal_clear(Terminal* terminal)
{
    unsigned char * video = terminal->buffer;
    int i = 0;

    for (i = 0; i < terminal->tty->winsize.ws_row * terminal->tty->winsize.ws_col; ++i)
    {
        *video++ = 0;
        *video++ = terminal->color;
    }

    Terminal_moveCursor(terminal, 0, 0);
}

void Terminal_putChar(Terminal* terminal, uint8 c)
{
    unsigned char * video = terminal->buffer;

    if ('\n' == c)
    {
        Terminal_moveCursor(terminal, terminal->currentLine + 1, terminal->currentColumn);

        if (terminal->currentLine >= terminal->tty->winsize.ws_row - 1)
        {
            Terminal_moveCursor(terminal, terminal->currentLine - 1, terminal->currentColumn);
            Terminal_scrollUp(terminal);
        }

        Terminal_moveCursor(terminal, terminal->currentLine, terminal->currentColumn);
        return;
    }
    if ('\r' == c)
    {
        Terminal_moveCursor(terminal, terminal->currentLine, 0);
        return;
    }
    else if ('\b' == c)
    {
        if (terminal->currentColumn > 0)
        {
            Terminal_moveCursor(terminal, terminal->currentLine, terminal->currentColumn - 1);
            c = '\0';
            video = terminal->buffer + (terminal->currentLine * terminal->tty->winsize.ws_col + terminal->currentColumn) * 2;
            video[0] = c;
            video[1] = terminal->color;
            return;
        }
        else if (terminal->currentColumn == 0)
        {
            if (terminal->currentLine > 0)
            {
                Terminal_moveCursor(terminal, terminal->currentLine - 1, terminal->tty->winsize.ws_col - 1);
                c = '\0';
                video = terminal->buffer + (terminal->currentLine * terminal->tty->winsize.ws_col + terminal->currentColumn) * 2;
                video[0] = c;
                video[1] = terminal->color;
                return;
            }
        }
    }

    if (terminal->currentColumn >= terminal->tty->winsize.ws_col)
    {
        Terminal_moveCursor(terminal, terminal->currentLine + 1, 0);
    }

    if (terminal->currentLine >= terminal->tty->winsize.ws_row - 1)
    {
        Terminal_moveCursor(terminal, terminal->currentLine - 1, terminal->currentColumn);
        Terminal_scrollUp(terminal);
    }

    video += (terminal->currentLine * terminal->tty->winsize.ws_col + terminal->currentColumn) * 2;

    video[0] = c;
    video[1] = terminal->color;

    if (gActiveTerminal == terminal)
    {
        if (terminal->addCharacterFunction)
        {
            terminal->addCharacterFunction(terminal, c);
        }
    }

    Terminal_moveCursor(terminal, terminal->currentLine, terminal->currentColumn + 1);
}

void Terminal_putText(Terminal* terminal, const uint8* text, uint32 size)
{
    const uint8* c = text;
    uint32 i = 0;
    while (*c && i < size)
    {
        Terminal_putChar(terminal, *c);
        ++c;
        ++i;
    }
}

void Terminal_moveCursor(Terminal* terminal, uint16 line, uint16 column)
{
    if (line >= terminal->tty->winsize.ws_row)
    {
        line = terminal->tty->winsize.ws_row - 1;
    }

    if (column >= terminal->tty->winsize.ws_col)
    {
        column = terminal->tty->winsize.ws_col - 1;
    }

    if (gActiveTerminal == terminal && terminal->moveCursorFunction)
    {
        terminal->moveCursorFunction(terminal, terminal->currentLine, terminal->currentColumn, line, column);
    }

    terminal->currentLine = line;
    terminal->currentColumn = column;
}

void Terminal_sendKey(Terminal* terminal, uint8 modifier, uint8 character)
{
    uint8 seq[8];
    memset(seq, 0, 8);
    uint32 size = 0;

    switch (character) {
    case KEY_PAGEUP:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 53;
        seq[3] = 126;
        size = 4;
    }
        break;
    case KEY_PAGEDOWN:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 54;
        seq[3] = 126;
        size = 4;
    }
        break;
    case KEY_HOME:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 72;
        size = 3;
    }
        break;
    case KEY_END:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 70;
        size = 3;
    }
        break;
    case KEY_INSERT:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 50;
        seq[3] = 126;
        size = 4;
    }
        break;
    case KEY_DELETE:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 51;
        seq[3] = 126;
        size = 4;
    }
        break;
    case KEY_UP:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 65;
        size = 3;
    }
        break;
    case KEY_DOWN:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 66;
        size = 3;
    }
        break;
    case KEY_RIGHT:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 67;
        size = 3;
    }
        break;
    case KEY_LEFT:
    {
        seq[0] = 27;
        seq[1] = 91;
        seq[2] = 68;
        size = 3;
    }
        break;
    default:
        if (modifier == KM_Ctrl)
        {
            if (isgraph(character))
            {
                uint8 upper = toupper(character);
                character = C(upper);
            }
        }
        
        seq[0] = character;
        size = 1;
        
        break;
    }

    write_fs(terminal->openedMaster, size, seq);
}



static void master_read_ready(TtyDev* tty, uint32 size)
{
    Terminal* terminal = (Terminal*)tty->privateData;

    uint8 characters[128];
    int32 bytes = 0;
    do
    {
        bytes = TtyDev_master_read_nonblock(terminal->openedMaster, 8, characters);

        if (bytes > 0)
        {
            Terminal_putText(terminal, characters, bytes);
        }
    } while (bytes > 0);
}