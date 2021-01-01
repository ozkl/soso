#ifndef CONSOLE_H
#define CONSOLE_H

#include "common.h"

#define TERMINAL_COUNT 10

void initializeConsole(BOOL graphicMode);

void sendKeyToConsole(uint8 scancode);

#endif // CONSOLE_H
