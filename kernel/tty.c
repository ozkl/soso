#include "tty.h"
#include "alloc.h"

#define TTY_LINE_COUNT 25
#define TTY_COLUMN_COUNT 80

Tty* createTty()
{
    Tty* tty = kmalloc(sizeof(Tty));
    tty->buffer = kmalloc(TTY_LINE_COUNT * TTY_COLUMN_COUNT * 2);
    tty->currentColumn = 0;
    tty->currentLine = 0;
    tty->color = 0x0A;

    memset(tty->lineBuffer, 0, TTY_LINEBUFFER_SIZE);
    tty->lineBufferIndex = 0;
    tty->keyBuffer = FifoBuffer_create(64);

    Tty_Clear(tty);

    return tty;
}

void destroyTty(Tty* tty)
{
    FifoBuffer_destroy(tty->keyBuffer);
    kfree(tty->buffer);
    kfree(tty);
}

void Tty_Print(Tty* tty, int row, int column, const char* text)
{
    unsigned char * video = tty->buffer;

    video += (row * TTY_COLUMN_COUNT + column) * 2;
    while(*text != 0)
    {
        *video++ = *text++;
        *video++ = tty->color;
    }
}

//One line
void Tty_ScrollUp(Tty* tty)
{
    unsigned char * videoLine = tty->buffer;
    unsigned char * videoLineNext = tty->buffer;
    int line = 0;
    int column = 0;

    for (line = 0; line < TTY_LINE_COUNT - 1; ++line)
    {
        for (column = 0; column < TTY_COLUMN_COUNT; ++column)
        {
            videoLine = tty->buffer + (line * TTY_COLUMN_COUNT + column) * 2;
            videoLineNext = tty->buffer + ((line + 1) * TTY_COLUMN_COUNT + column) * 2;

            videoLine[0] = videoLineNext[0];
            videoLine[1] = videoLineNext[1];
        }
    }

    //Last line should be empty.
    unsigned char * lastLine = tty->buffer + ((TTY_LINE_COUNT - 1) * TTY_COLUMN_COUNT) * 2;
    for (int i = 0; i < TTY_COLUMN_COUNT * 2; i += 2)
    {
        lastLine[i] = 0;
        lastLine[i + 1] = tty->color;
    }
}

void Tty_Clear(Tty* tty)
{
    unsigned char * video = tty->buffer;
    int i = 0;

    for (i = 0; i < TTY_LINE_COUNT * TTY_COLUMN_COUNT; ++i)
    {
        *video++ = 0;
        *video++ = tty->color;
    }

    tty->currentLine = 0;
    tty->currentColumn = 0;
}

void Tty_PutChar(Tty* tty, char c)
{
    unsigned char * video = tty->buffer;

    if ('\n' == c || '\r' == c)
    {
        ++tty->currentLine;
        tty->currentColumn = 0;

        if (tty->currentLine >= TTY_LINE_COUNT - 0)
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
            video = tty->buffer + (tty->currentLine * TTY_COLUMN_COUNT + tty->currentColumn) * 2;
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
                tty->currentColumn = TTY_COLUMN_COUNT - 1;
                c = '\0';
                video = tty->buffer + (tty->currentLine * TTY_COLUMN_COUNT + tty->currentColumn) * 2;
                video[0] = c;
                video[1] = tty->color;
                Tty_MoveCursor(tty, tty->currentLine, tty->currentColumn);
                return;
            }
        }
    }

    if (tty->currentColumn >= TTY_COLUMN_COUNT)
    {
        ++tty->currentLine;
        tty->currentColumn = 0;
    }

    if (tty->currentLine >= TTY_LINE_COUNT - 0)
    {
        --tty->currentLine;
        Tty_ScrollUp(tty);
    }

    video += (tty->currentLine * TTY_COLUMN_COUNT + tty->currentColumn) * 2;

    video[0] = c;
    video[1] = tty->color;

    ++tty->currentColumn;

    Tty_MoveCursor(tty, tty->currentLine, tty->currentColumn);
}

void Tty_MoveCursor(Tty* tty, uint16 line, uint16 column)
{
    tty->currentLine = line;
    tty->currentColumn = column;
}
