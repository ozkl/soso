#ifndef KERNELTERMINAL_FB_H
#define KERNELTERMINAL_FB_H

#include "common.h"

uint16_t fbterminal_get_column_count();
uint16_t fbterminal_get_row_count();
void fbterminal_refresh(uint8_t* source_buffer);
void fbterminal_set_character(uint16_t row, uint16_t column, uint8_t character);
void fbterminal_move_cursor(uint8_t* source_buffer, uint16_t old_row, uint16_t old_column, uint16_t row, uint16_t column);

#endif //KERNELTERMINAL_FB_H