#include "alloc.h"
#include "keymap.h"
#include "fs.h"
#include "console.h"
#include "fbterminal.h"
#include "vgaterminal.h"
#include "terminal.h"

static void master_read_ready(TtyDev* tty, uint32 size);

Terminal* terminal_create(TtyDev* tty, BOOL graphicMode)
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
    terminal->current_column = 0;
    terminal->current_line = 0;
    terminal->color = 0x0A;

    terminal_clear(terminal);

    terminal->openedMaster = fs_open_for_process(NULL, tty->master_node, 0);
    tty->privateData = terminal;

    tty->masterReadReady = master_read_ready;

    return terminal;
}

void terminal_destroy(Terminal* terminal)
{
    fs_close(terminal->openedMaster);

    kfree(terminal->buffer);
    kfree(terminal);
}

void terminal_print(Terminal* terminal, int row, int column, const char* text)
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
void terminal_scroll_up(Terminal* terminal)
{
    unsigned char * video_line = terminal->buffer;
    unsigned char * video_line_next = terminal->buffer;
    int line = 0;
    int column = 0;

    for (line = 0; line < terminal->tty->winsize.ws_row - 1; ++line)
    {
        for (column = 0; column < terminal->tty->winsize.ws_col; ++column)
        {
            video_line = terminal->buffer + (line * terminal->tty->winsize.ws_col + column) * 2;
            video_line_next = terminal->buffer + ((line + 1) * terminal->tty->winsize.ws_col + column) * 2;

            video_line[0] = video_line_next[0];
            video_line[1] = video_line_next[1];
        }
    }

    //Last line should be empty.
    unsigned char * last_line = terminal->buffer + ((terminal->tty->winsize.ws_row - 1) * terminal->tty->winsize.ws_col) * 2;
    for (int i = 0; i < terminal->tty->winsize.ws_col * 2; i += 2)
    {
        last_line[i] = 0;
        last_line[i + 1] = terminal->color;
    }

    if (g_active_terminal == terminal)
    {
        if (terminal->refreshFunction)
        {
            terminal->refreshFunction(terminal);
        }
    }
}

void terminal_clear(Terminal* terminal)
{
    unsigned char * video = terminal->buffer;
    int i = 0;

    for (i = 0; i < terminal->tty->winsize.ws_row * terminal->tty->winsize.ws_col; ++i)
    {
        *video++ = 0;
        *video++ = terminal->color;
    }

    terminal_move_cursor(terminal, 0, 0);
}

void terminal_put_character(Terminal* terminal, uint8 c)
{
    unsigned char * video = terminal->buffer;

    if ('\n' == c)
    {
        terminal_move_cursor(terminal, terminal->current_line + 1, terminal->current_column);

        if (terminal->current_line >= terminal->tty->winsize.ws_row - 1)
        {
            terminal_move_cursor(terminal, terminal->current_line - 1, terminal->current_column);
            terminal_scroll_up(terminal);
        }

        terminal_move_cursor(terminal, terminal->current_line, terminal->current_column);
        return;
    }
    if ('\r' == c)
    {
        terminal_move_cursor(terminal, terminal->current_line, 0);
        return;
    }
    else if ('\b' == c)
    {
        if (terminal->current_column > 0)
        {
            terminal_move_cursor(terminal, terminal->current_line, terminal->current_column - 1);
            c = '\0';
            video = terminal->buffer + (terminal->current_line * terminal->tty->winsize.ws_col + terminal->current_column) * 2;
            video[0] = c;
            video[1] = terminal->color;
            return;
        }
        else if (terminal->current_column == 0)
        {
            if (terminal->current_line > 0)
            {
                terminal_move_cursor(terminal, terminal->current_line - 1, terminal->tty->winsize.ws_col - 1);
                c = '\0';
                video = terminal->buffer + (terminal->current_line * terminal->tty->winsize.ws_col + terminal->current_column) * 2;
                video[0] = c;
                video[1] = terminal->color;
                return;
            }
        }
    }

    if (terminal->current_column >= terminal->tty->winsize.ws_col)
    {
        terminal_move_cursor(terminal, terminal->current_line + 1, 0);
    }

    if (terminal->current_line >= terminal->tty->winsize.ws_row - 1)
    {
        terminal_move_cursor(terminal, terminal->current_line - 1, terminal->current_column);
        terminal_scroll_up(terminal);
    }

    video += (terminal->current_line * terminal->tty->winsize.ws_col + terminal->current_column) * 2;

    video[0] = c;
    video[1] = terminal->color;

    if (g_active_terminal == terminal)
    {
        if (terminal->addCharacterFunction)
        {
            terminal->addCharacterFunction(terminal, c);
        }
    }

    terminal_move_cursor(terminal, terminal->current_line, terminal->current_column + 1);
}

void terminal_put_text(Terminal* terminal, const uint8* text, uint32 size)
{
    const uint8* c = text;
    uint32 i = 0;
    while (*c && i < size)
    {
        terminal_put_character(terminal, *c);
        ++c;
        ++i;
    }
}

void terminal_move_cursor(Terminal* terminal, uint16 line, uint16 column)
{
    if (line >= terminal->tty->winsize.ws_row)
    {
        line = terminal->tty->winsize.ws_row - 1;
    }

    if (column >= terminal->tty->winsize.ws_col)
    {
        column = terminal->tty->winsize.ws_col - 1;
    }

    if (g_active_terminal == terminal && terminal->moveCursorFunction)
    {
        terminal->moveCursorFunction(terminal, terminal->current_line, terminal->current_column, line, column);
    }

    terminal->current_line = line;
    terminal->current_column = column;
}

void terminal_send_key(Terminal* terminal, uint8 modifier, uint8 character)
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

    fs_write(terminal->openedMaster, size, seq);
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
            terminal_put_text(terminal, characters, bytes);
        }
    } while (bytes > 0);
}