/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef KERNELTERMINAL_VGA_H
#define KERNELTERMINAL_VGA_H

#include "common.h"

uint16_t vgaterminal_get_column_count();
uint16_t vgaterminal_get_row_count();
void vgaterminal_set_cursor_visible(BOOL visible);
void vgaterminal_set_character(uint16_t row, uint16_t column, uint8_t character);
void vgaterminal_move_cursor(uint16_t row, uint16_t column);

#endif //KERNELTERMINAL_VGA_H