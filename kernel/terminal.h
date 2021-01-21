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
    uint16 current_line;
    uint16 current_column;
    uint8 color;
    File* opened_master;
    TerminalRefresh refresh_function;
    TerminalAddCharacter add_character_function;
    TerminalMoveCursor move_cursor_function;
} Terminal;



Terminal* terminal_create(TtyDev* tty, BOOL graphic_mode);
void terminal_destroy(Terminal* terminal);

void terminal_print(Terminal* terminal, int row, int column, const char* text);
void terminal_clear(Terminal* terminal);
void terminal_put_character(Terminal* terminal, uint8 c);
void terminal_put_text(Terminal* terminal, const uint8* text, uint32 size);
void terminal_move_cursor(Terminal* terminal, uint16 line, uint16 column);
void terminal_scroll_up(Terminal* terminal);

void terminal_send_key(Terminal* terminal, uint8 modifier, uint8 character);

#endif // TERMINAL_H
