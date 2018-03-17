#ifndef TTYDRIVER_H
#define TTYDRIVER_H

#include "common.h"

void initializeTTYs();

void sendKeyInputToTTY(uint8 scancode);

//TODO: posix_openpt

#endif // TTYDRIVER_H
