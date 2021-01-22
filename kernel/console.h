#ifndef CONSOLE_H
#define CONSOLE_H

#include "common.h"

#define TERMINAL_COUNT 10

typedef struct Terminal Terminal;

extern Terminal* g_active_terminal;

void console_initialize(BOOL graphicMode);

void console_send_key(uint8_t scancode);

#endif // CONSOLE_H
