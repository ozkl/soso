#include "kernelterminal_vga.h"

#define SCREEN_ROW_COUNT 25
#define SCREEN_COLUMN_COUNT 80

static uint8_t * g_video_start = (uint8_t*)0xB8000;

static uint8_t g_color = 0x0A;

uint16_t vgaterminal_get_column_count()
{
    return SCREEN_COLUMN_COUNT;
}

uint16_t vgaterminal_get_row_count()
{
    return SCREEN_ROW_COUNT;
}

void vgaterminal_set_cursor_visible(BOOL visible)
{
    uint8_t cursor = inb(0x3d5);

    if (visible)
    {
        cursor &= ~0x20;//5th bit cleared when cursor visible
    }
    else
    {
        cursor |= 0x20;//5th bit set when cursor invisible
    }
    outb(0x3D5, cursor);
}

void vgaterminal_refresh(uint8_t* source_buffer)
{
    memcpy(g_video_start, source_buffer, SCREEN_ROW_COUNT * SCREEN_COLUMN_COUNT * 2);
}

void vgaterminal_set_character(uint16_t row, uint16_t column, uint8_t character)
{
    uint8_t * video = g_video_start + (row * SCREEN_COLUMN_COUNT + column) * 2;
    
    *video++ = character;
    *video++ = g_color;
}

void vgaterminal_move_cursor(uint16_t row, uint16_t column)
{
    uint16_t cursorLocation = row * SCREEN_COLUMN_COUNT + column;
    outb(0x3D4, 14);                  // Tell the VGA board we are setting the high cursor byte.
    outb(0x3D5, cursorLocation >> 8); // Send the high cursor byte.
    outb(0x3D4, 15);                  // Tell the VGA board we are setting the low cursor byte.
    outb(0x3D5, cursorLocation);      // Send the low cursor byte.
}