#ifndef TTYDRIVER_H
#define TTYDRIVER_H

#include "common.h"
#include "tty.h"

void initializeTTYs(BOOL graphicMode);
Tty* getActiveTTY();

void sendKeyInputToTTY(uint8 scancode);

//TODO: posix_openpt

#endif // TTYDRIVER_H
