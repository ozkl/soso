#ifndef SCREEN_H
#define SCREEN_H

#include "common.h"
#include "tty.h"

void VGAText_flushFromTty(Tty* tty);
void VGAText_print(int row, int column, const char* text);
void VGAText_setActiveColor(uint8 color);
void VGAText_applyColor(uint8 color);
void VGAText_clear();
void VGAText_setCursorVisible(BOOL visible);
void VGAText_moveCursor(uint16 line, uint16 column);
void VGAText_getCursor(uint16* line, uint16* column);

#endif //SCREEN_H
