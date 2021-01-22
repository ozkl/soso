#include "vgaterminal.h"

#define SCREEN_LINE_COUNT 25
#define SCREEN_COLUMN_COUNT 80

static uint8_t * g_video_start = (uint8_t*)0xB8000;

static uint8_t g_color = 0x0A;

static void vgaterminal_refresh_terminal(Terminal* terminal);
static void vgaterminal_add_character(Terminal* terminal, uint8_t character);
static void vgaterminal_move_cursor(Terminal* terminal, uint16_t oldLine, uint16_t oldColumn, uint16_t line, uint16_t column);

void vgaterminal_setup(Terminal* terminal)
{
    terminal->tty->winsize.ws_row = SCREEN_LINE_COUNT;
    terminal->tty->winsize.ws_col = SCREEN_COLUMN_COUNT;
    terminal->refresh_function = vgaterminal_refresh_terminal;
    terminal->add_character_function = vgaterminal_add_character;
    terminal->move_cursor_function = vgaterminal_move_cursor;
}

static void vgaterminal_set_cursor_visible(BOOL visible)
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

static void vgaterminal_refresh_terminal(Terminal* terminal)
{
    memcpy(g_video_start, terminal->buffer, SCREEN_LINE_COUNT * SCREEN_COLUMN_COUNT * 2);
}

static void vgaterminal_add_character(Terminal* terminal, uint8_t character)
{
    uint8_t * video = g_video_start + (terminal->current_line * SCREEN_COLUMN_COUNT + terminal->current_column) * 2;
    
    *video++ = character;
    *video++ = g_color;
}

static void vgaterminal_move_cursor(Terminal* terminal, uint16_t oldLine, uint16_t oldColumn, uint16_t line, uint16_t column)
{
    uint16_t cursorLocation = line * SCREEN_COLUMN_COUNT + column;
    outb(0x3D4, 14);                  // Tell the VGA board we are setting the high cursor byte.
    outb(0x3D5, cursorLocation >> 8); // Send the high cursor byte.
    outb(0x3D4, 15);                  // Tell the VGA board we are setting the low cursor byte.
    outb(0x3D5, cursorLocation);      // Send the low cursor byte.
}