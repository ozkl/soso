#include "terminal.h"
#include "device.h"
#include "serial.h"
#include "devfs.h"
#include "alloc.h"
#include "common.h"
#include "list.h"
#include "fifobuffer.h"
#include "gfx.h"
#include "debugprint.h"
#include "commonuser.h"
#include "termios.h"
#include "keymap.h"
#include "console.h"

static Terminal* g_terminals[TERMINAL_COUNT];
static uint32 g_active_terminal_index = 0;
Terminal* g_active_terminal = NULL;

static uint8 g_key_modifier = 0;


static BOOL console_open(File *file, uint32 flags);
static void console_close(File *file);
static int32 console_ioctl(File *file, int32 request, void * argp);

static uint8 get_character_for_scancode(KeyModifier modifier, uint8 scancode);
static void process_scancode(uint8 scancode);

static void set_active_terminal(uint32 index);

void console_initialize(BOOL graphicMode)
{
    for (int i = 0; i < 10; ++i)
    {
        Terminal* terminal = NULL;
        FileSystemNode* ttyNode = ttydev_create();
        if (ttyNode)
        {
            TtyDev* ttyDev = (TtyDev*)ttyNode->privateNodeData;
        
            terminal = terminal_create(ttyDev, graphicMode);
        }
        
        g_terminals[i] = terminal;
    }

    set_active_terminal(0);

    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    sprintf(device.name, "console");
    device.device_type = FT_CharacterDevice;
    device.open = console_open;
    device.close = console_close;
    device.ioctl = console_ioctl;
    devfs_register_device(&device);
}


void console_send_key(uint8 scancode)
{
    process_scancode(scancode);

    uint8 character = get_character_for_scancode(g_key_modifier, scancode);

    uint8 keyRelease = (0x80 & scancode); //ignore release event

    if (character > 0 && keyRelease == 0)
    {
        terminal_send_key(g_active_terminal, g_key_modifier, character);
    }

}

static BOOL console_open(File *file, uint32 flags)
{
    return TRUE;
}

static void console_close(File *file)
{
    
}

static int32 console_ioctl(File *file, int32 request, void * argp)
{
    //we can use ioctl for chvt multiplexing
    return -1;
}

static void set_active_terminal(uint32 index)
{
    if (index >= 0 && index < TERMINAL_COUNT)
    {
        g_active_terminal_index = index;
        g_active_terminal = g_terminals[g_active_terminal_index];

        Gfx_Fill(0xFFFFFFFF);

        if (g_active_terminal->refresh_function)
        {
            g_active_terminal->refresh_function(g_active_terminal);
        }

        if (g_active_terminal->move_cursor_function)
        {
            g_active_terminal->move_cursor_function(g_active_terminal, g_active_terminal->current_line, g_active_terminal->current_column, g_active_terminal->current_line, g_active_terminal->current_column);
        }
    }
}

static uint8 get_character_for_scancode(KeyModifier modifier, uint8 scancode)
{
    if ((modifier & KM_LeftShift) || (modifier & KM_RightShift))
    {
        return gKeyShiftMap[scancode];
    }

    return gKeyMap[scancode];
}

static void process_scancode(uint8 scancode)
{
    uint8 lastBit = scancode & 0x80;

    scancode &= 0x7F;

    if (lastBit)
    {
        //key release

        switch (scancode)
        {
        case KEY_LEFTSHIFT:
            g_key_modifier &= ~KM_LeftShift;
            break;
        case KEY_RIGHTSHIFT:
            g_key_modifier &= ~KM_RightShift;
            break;
        case KEY_CTRL:
            g_key_modifier &= ~KM_Ctrl;
            break;
        case KEY_ALT:
            g_key_modifier &= ~KM_Alt;
            break;
        }
    }
    else
    {
        //key pressed

        switch (scancode)
        {
        case KEY_LEFTSHIFT:
            g_key_modifier |= KM_LeftShift;
            break;
        case KEY_RIGHTSHIFT:
            g_key_modifier |= KM_RightShift;
            break;
        case KEY_CTRL:
            g_key_modifier |= KM_Ctrl;
            break;
        case KEY_ALT:
            g_key_modifier |= KM_Alt;
            break;
        }

        if ((g_key_modifier & KM_Ctrl) == KM_Ctrl)
        {
            int ttyIndex = scancode - KEY_F1;
            
            set_active_terminal(ttyIndex);
        }
    }
}
