#ifndef SCREEN_H
#define SCREEN_H

#include "common.h"

void Screen_CopyFrom(uint8* buffer);
void Screen_CopyTo(uint8* buffer);
void Screen_Print(int row, int column, const char* text);
void Screen_SetActiveColor(uint8 color);
void Screen_ApplyColor(uint8 color);
void Screen_Clear();
void Screen_PutChar(char c);
void Screen_PrintF(const char *format, ...);
void Screen_SetCursorVisible(BOOL visible);
void Screen_MoveCursor(uint16 line, uint16 column);
void Screen_GetCursor(uint16* line, uint16* column);
void Screen_ScrollUp();
void Screen_PrintInterruptsEnabled();


#endif //SCREEN_H
