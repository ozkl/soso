#ifndef TERMINAL_H
#define TERMINAL_H

#include "common.h"
#include "fs.h"
#include "fifobuffer.h"
#include "termios.h"
#include "ttydev.h"

typedef struct Terminal Terminal;

typedef void (*TerminalRefresh)(Terminal* terminal);
typedef void (*TerminalAddCharacter)(Terminal* terminal, uint8 character);
typedef void (*TerminalMoveCursor)(Terminal* terminal, uint16 oldLine, uint16 oldColumn, uint16 line, uint16 column);

typedef struct Terminal
{
    TtyDev* tty;
    uint8* buffer;
    uint16 currentLine;
    uint16 currentColumn;
    uint8 color;
    File* openedMaster;
    TerminalRefresh refreshFunction;
    TerminalAddCharacter addCharacterFunction;
    TerminalMoveCursor moveCursorFunction;
} Terminal;



Terminal* Terminal_create(TtyDev* tty, BOOL graphicMode);
void Terminal_destroy(Terminal* terminal);

void Terminal_print(Terminal* terminal, int row, int column, const char* text);
void Terminal_clear(Terminal* terminal);
void Terminal_putChar(Terminal* terminal, uint8 c);
void Terminal_putText(Terminal* terminal, const uint8* text, uint32 size);
void Terminal_moveCursor(Terminal* terminal, uint16 line, uint16 column);
void Terminal_scrollUp(Terminal* terminal);

void Terminal_sendKey(Terminal* terminal, uint8 modifier, uint8 character);

#endif // TERMINAL_H
