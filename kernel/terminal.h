#ifndef TERMINAL_H
#define TERMINAL_H

#include "common.h"
#include "fs.h"
#include "fifobuffer.h"
#include "termios.h"
#include "ttydev.h"

typedef struct Terminal Terminal;

typedef void (*TerminalRefresh)(Terminal* terminal);
typedef void (*TerminalAddCharacter)(Terminal* terminal, uint8_t character);
typedef void (*TerminalMoveCursor)(Terminal* terminal, uint16_t oldLine, uint16_t oldColumn, uint16_t line, uint16_t column);

typedef struct Terminal
{
    TtyDev* tty;
    uint8_t* buffer;
    uint16_t current_line;
    uint16_t current_column;
    uint8_t color;
    File* opened_master;
    BOOL disabled;
    TerminalRefresh refresh_function;
    TerminalAddCharacter add_character_function;
    TerminalMoveCursor move_cursor_function;
} Terminal;



Terminal* terminal_create(TtyDev* tty, BOOL graphic_mode);
void terminal_destroy(Terminal* terminal);

void terminal_print(Terminal* terminal, int row, int column, const char* text);
void terminal_clear(Terminal* terminal);
void terminal_put_character(Terminal* terminal, uint8_t c);
void terminal_put_text(Terminal* terminal, const uint8_t* text, uint32_t size);
void terminal_move_cursor(Terminal* terminal, uint16_t line, uint16_t column);
void terminal_scroll_up(Terminal* terminal);

void terminal_send_key(Terminal* terminal, uint8_t modifier, uint8_t character);

#endif // TERMINAL_H
