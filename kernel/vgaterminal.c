#include "vgaterminal.h"

#define SCREEN_LINE_COUNT 25
#define SCREEN_COLUMN_COUNT 80

static uint8 * g_video_start = (uint8*)0xB8000;

static uint8 g_color = 0x0A;

static void vgaterminal_refresh_terminal(Terminal* terminal);
static void vgaterminal_add_character(Terminal* terminal, uint8 character);
static void vgaterminal_move_cursor(Terminal* terminal, uint16 oldLine, uint16 oldColumn, uint16 line, uint16 column);

void vgaterminal_setup(Terminal* terminal)
{
    terminal->tty->winsize.ws_row = SCREEN_LINE_COUNT;
    terminal->tty->winsize.ws_col = SCREEN_COLUMN_COUNT;
    terminal->refreshFunction = vgaterminal_refresh_terminal;
    terminal->addCharacterFunction = vgaterminal_add_character;
    terminal->moveCursorFunction = vgaterminal_move_cursor;
}

static void vgaterminal_set_cursor_visible(BOOL visible)
{
    uint8 cursor = inb(0x3d5);

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

static void vgaterminal_add_character(Terminal* terminal, uint8 character)
{
    uint8 * video = g_video_start + (terminal->currentLine * SCREEN_COLUMN_COUNT + terminal->currentColumn) * 2;
    
    *video++ = character;
    *video++ = g_color;
}

static void vgaterminal_move_cursor(Terminal* terminal, uint16 oldLine, uint16 oldColumn, uint16 line, uint16 column)
{
    uint16 cursorLocation = line * SCREEN_COLUMN_COUNT + column;
    outb(0x3D4, 14);                  // Tell the VGA board we are setting the high cursor byte.
    outb(0x3D5, cursorLocation >> 8); // Send the high cursor byte.
    outb(0x3D4, 15);                  // Tell the VGA board we are setting the low cursor byte.
    outb(0x3D5, cursorLocation);      // Send the low cursor byte.
}