//This file will be removed with TTY subsystem update!

#ifndef TTYDRIVER_H
#define TTYDRIVER_H

#include "common.h"
#include "tty.h"
#include "fs.h"

void initializeTTYs(BOOL graphicMode);
Tty* getActiveTTY();

void sendKeyInputToTTY(Tty* tty, uint8 scancode);

BOOL isValidTTY(Tty* tty);

#endif // TTYDRIVER_H
