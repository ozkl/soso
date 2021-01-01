#include "alloc.h"
#include "keymap.h"
#include "terminal.h"

Terminal* Terminal_create(uint16 lineCount, uint16 columnCount)
{
    Terminal* tty = kmalloc(sizeof(Terminal));
    memset((uint8*)tty, 0, sizeof(Terminal));

    tty->lineCount = lineCount;
    tty->columnCount = columnCount;
    tty->buffer = kmalloc(tty->lineCount * tty->columnCount * 2);
    tty->currentColumn = 0;
    tty->currentLine = 0;
    tty->color = 0x0A;

    Terminal_clear(tty);

    return tty;
}

void Terminal_destroy(Terminal* tty)
{
    kfree(tty->buffer);
    kfree(tty);
}

void Terminal_print(Terminal* tty, int row, int column, const char* text)
{
    unsigned char * video = tty->buffer;

    video += (row * tty->columnCount + column) * 2;
    while(*text != 0)
    {
        *video++ = *text++;
        *video++ = tty->color;
    }
}

//One line
void Terminal_scrollUp(Terminal* tty)
{
    unsigned char * videoLine = tty->buffer;
    unsigned char * videoLineNext = tty->buffer;
    int line = 0;
    int column = 0;

    for (line = 0; line < tty->lineCount - 1; ++line)
    {
        for (column = 0; column < tty->columnCount; ++column)
        {
            videoLine = tty->buffer + (line * tty->columnCount + column) * 2;
            videoLineNext = tty->buffer + ((line + 1) * tty->columnCount + column) * 2;

            videoLine[0] = videoLineNext[0];
            videoLine[1] = videoLineNext[1];
        }
    }

    //Last line should be empty.
    unsigned char * lastLine = tty->buffer + ((tty->lineCount - 1) * tty->columnCount) * 2;
    for (int i = 0; i < tty->columnCount * 2; i += 2)
    {
        lastLine[i] = 0;
        lastLine[i + 1] = tty->color;
    }
}

void Terminal_clear(Terminal* tty)
{
    unsigned char * video = tty->buffer;
    int i = 0;

    for (i = 0; i < tty->lineCount * tty->columnCount; ++i)
    {
        *video++ = 0;
        *video++ = tty->color;
    }

    tty->currentLine = 0;
    tty->currentColumn = 0;
}

void Terminal_putChar(Terminal* tty, char c)
{
    unsigned char * video = tty->buffer;

    if ('\n' == c || '\r' == c)
    {
        ++tty->currentLine;
        tty->currentColumn = 0;

        if (tty->currentLine >= tty->lineCount - 0)
        {
            --tty->currentLine;
            Tty_ScrollUp(tty);
        }

        Tty_MoveCursor(tty, tty->currentLine, tty->currentColumn);
        return;
    }
    else if ('\b' == c)
    {
        if (tty->currentColumn > 0)
        {
            --tty->currentColumn;
            c = '\0';
            video = tty->buffer + (tty->currentLine * tty->columnCount + tty->currentColumn) * 2;
            video[0] = c;
            video[1] = tty->color;
            Tty_MoveCursor(tty, tty->currentLine, tty->currentColumn);
            return;
        }
        else if (tty->currentColumn == 0)
        {
            if (tty->currentLine > 0)
            {
                --tty->currentLine;
                tty->currentColumn = tty->columnCount - 1;
                c = '\0';
                video = tty->buffer + (tty->currentLine * tty->columnCount + tty->currentColumn) * 2;
                video[0] = c;
                video[1] = tty->color;
                Tty_MoveCursor(tty, tty->currentLine, tty->currentColumn);
                return;
            }
        }
    }

    if (tty->currentColumn >= tty->columnCount)
    {
        ++tty->currentLine;
        tty->currentColumn = 0;
    }

    if (tty->currentLine >= tty->lineCount - 0)
    {
        --tty->currentLine;
        Tty_ScrollUp(tty);
    }

    video += (tty->currentLine * tty->columnCount + tty->currentColumn) * 2;

    video[0] = c;
    video[1] = tty->color;

    ++tty->currentColumn;

    Tty_MoveCursor(tty, tty->currentLine, tty->currentColumn);
}

void Terminal_putText(Terminal* tty, const char* text)
{
    const char* c = text;
    while (*c)
    {
        Tty_PutChar(tty, *c);
        ++c;
    }
}

void Terminal_moveCursor(Terminal* tty, uint16 line, uint16 column)
{
    tty->currentLine = line;
    tty->currentColumn = column;
}

void Terminal_sendKey(Terminal* tty, uint8 character)
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
        seq[0] = character;
        size = 1;
        break;
    }

    //TODO: write to master seq and size
}
