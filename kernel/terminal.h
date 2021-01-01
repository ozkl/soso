#ifndef TERMINAL_H
#define TERMINAL_H

#include "common.h"
#include "fifobuffer.h"
#include "termios.h"

typedef struct Terminal
{
    uint16 lineCount;
    uint16 columnCount;
    uint8* buffer;
    uint16 currentLine;
    uint16 currentColumn;
    uint8 color;
    void* privateData;
    //TODO: function pointers of underlying text renderer
} Terminal;



Terminal* Terminal_create(uint16 lineCount, uint16 columnCount);
void Terminal_destroy(Terminal* tty);

void Terminal_print(Terminal* tty, int row, int column, const char* text);
void Terminal_clear(Terminal* tty);
void Terminal_putChar(Terminal* tty, char c);
void Terminal_putText(Terminal* tty, const char* text);
void Terminal_moveCursor(Terminal* tty, uint16 line, uint16 column);
void Terminal_scrollUp(Terminal* tty);

void Terminal_sendKey(Terminal* tty, uint8 character);

#endif // TERMINAL_H
