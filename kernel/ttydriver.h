#ifndef TTYDRIVER_H
#define TTYDRIVER_H

#include "common.h"
#include "tty.h"

void initializeTTYs(BOOL graphicMode);
Tty* getActiveTTY();

void sendKeyInputToTTY(Tty* tty, uint8 scancode);

BOOL createPT();

BOOL isValidTTY(Tty* tty);

#endif // TTYDRIVER_H
