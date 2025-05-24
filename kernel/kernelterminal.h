#ifndef KERNELTERMINAL_H
#define KERNELTERMINAL_H

#include "common.h"
#include "fs.h"
#include "fifobuffer.h"
#include "termios.h"
#include "ttydev.h"
#include "ozterm.h"

typedef struct Terminal Terminal;


typedef struct Terminal
{
    TtyDev* tty;
    File* opened_master;
    BOOL disabled;
    Ozterm* term;
} Terminal;



Terminal* terminal_create(BOOL graphic_mode);
void terminal_destroy(Terminal* terminal);


void terminal_clear(Terminal* terminal);
void terminal_put_character(Terminal* terminal, uint8_t c);
void terminal_put_text(Terminal* terminal, const uint8_t* text, uint32_t size);
void terminal_move_cursor(Terminal* terminal, uint16_t line, uint16_t column);
void terminal_scroll_up(Terminal* terminal);

void terminal_send_key(Terminal* terminal, uint8_t modifier, uint8_t character);

#endif // KERNELTERMINAL_H
