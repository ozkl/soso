/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef KERNELTERMINAL_FB_H
#define KERNELTERMINAL_FB_H

#include "common.h"

uint16_t fbterminal_get_column_count();
uint16_t fbterminal_get_row_count();
void fbterminal_set_character(uint16_t row, uint16_t column, uint8_t character, uint32_t fg_color, uint32_t bg_color);

#endif //KERNELTERMINAL_FB_H