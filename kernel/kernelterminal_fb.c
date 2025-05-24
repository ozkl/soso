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

void fbterminal_refresh(uint8_t* source_buffer)
{
    uint16_t rows = fbterminal_get_row_count();
    uint16_t columns = fbterminal_get_column_count();

    for (uint32_t r = 0; r < rows; ++r)
    {
        for (uint32_t c = 0; c < columns; ++c)
        {
            uint8_t* characterPos = source_buffer + (r * columns + c) * 2;

            uint8_t chr = characterPos[0];
            uint8_t color = characterPos[1];

            gfx_put_char_at(chr, c, r, 0, 0xFFFFFFFF);
        }
    }
}

void fbterminal_set_character(uint16_t row, uint16_t column, uint8_t character)
{
    gfx_put_char_at(character, column, row, 0, 0xFFFFFFFF);
}

void fbterminal_move_cursor(uint8_t* source_buffer, uint16_t old_row, uint16_t old_column, uint16_t row, uint16_t column)
{
    uint16_t columns = fbterminal_get_column_count();

    //restore old place
    uint8_t* character_old = source_buffer + (old_row * columns + old_column) * 2;
    gfx_put_char_at(*character_old, old_column, old_row, 0, 0xFFFFFFFF);

    //put cursor to new place
    uint8_t* character_new = source_buffer + (row * columns + column) * 2;
    gfx_put_char_at(*character_new, column, row, 0xFFFF0000, 0);
}