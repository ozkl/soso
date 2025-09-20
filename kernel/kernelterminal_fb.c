/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "gfx.h"
#include "kernelterminal_fb.h"


uint16_t fbterminal_get_column_count()
{
    return gfx_get_width() / 9;
}

uint16_t fbterminal_get_row_count()
{
    return gfx_get_height() / 16;
}

void fbterminal_set_character(uint16_t row, uint16_t column, uint8_t character, uint32_t fg_color, uint32_t bg_color)
{
    gfx_put_char_at(character, column, row, fg_color, bg_color);
}

void fbterminal_put_text(uint16_t row, uint16_t column, uint8_t *text, uint16_t size, uint32_t fg_color, uint32_t bg_color)
{
    for (uint16_t i = 0; i < size; ++i)
    {
        gfx_put_char_at(text[i], column + i, row, fg_color, bg_color);
    }
}
