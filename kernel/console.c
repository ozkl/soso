#include "terminal.h"
#include "device.h"
#include "vgatext.h"
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

static Terminal* gTerminals[TERMINAL_COUNT];
static uint32 gActiveTerminalIndex = 0;

static uint8 gKeyModifier = 0;


static BOOL console_open(File *file, uint32 flags);
static void console_close(File *file);
static int32 console_ioctl(File *file, int32 request, void * argp);

static uint8 getCharacterForScancode(KeyModifier modifier, uint8 scancode);
static void processScancode(uint8 scancode);

void initializeConsole(BOOL graphicMode)
{
    for (int i = 0; i < 10; ++i)
    {
        Terminal* terminal = NULL;
        if (graphicMode)
        {
            terminal = Terminal_create(768 / 16, 1024 / 9);
        }
        else
        {
            terminal = Terminal_create(25, 80);
        }

        
        gTerminals[i] = terminal;
    }

    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    sprintf(device.name, "console");
    device.deviceType = FT_CharacterDevice;
    device.open = console_open;
    device.close = console_close;
    device.ioctl = console_ioctl;
    registerDevice(&device);
}


void sendKeyToConsole(uint8 scancode)
{
    processScancode(scancode);

    uint8 character = getCharacterForScancode(gKeyModifier, scancode);

    uint8 keyRelease = (0x80 & scancode); //ignore release event

    if (character > 0 && keyRelease == 0)
    {
        Terminal_sendKey(gTerminals[gActiveTerminalIndex], character);
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

static void setActiveTty(uint32 index)
{
    if (index >= 0 && index < TERMINAL_COUNT)
    {
        gActiveTerminalIndex = index;

        Gfx_Fill(0xFFFFFFFF);

        //if (gTerminals[gActiveTerminalIndex]->flushScreen)
        {
            //gTerminals[gActiveTerminalIndex]->flushScreen(tty);
        }
    }

    //Serial_PrintF("line:%d column:%d\r\n", gActiveTty->currentLine, gActiveTty->currentColumn);
}

static uint8 getCharacterForScancode(KeyModifier modifier, uint8 scancode)
{
    //return gKeyboardLayout[scancode];
    if ((modifier & KM_LeftShift) || (modifier & KM_RightShift))
    {
        return gKeyShiftMap[scancode];
    }

    return gKeyMap[scancode];
}

static void processScancode(uint8 scancode)
{
    uint8 lastBit = scancode & 0x80;

    scancode &= 0x7F;

    if (lastBit)
    {
        //key release

        switch (scancode)
        {
        case KEY_LEFTSHIFT:
            gKeyModifier &= ~KM_LeftShift;
            break;
        case KEY_RIGHTSHIFT:
            gKeyModifier &= ~KM_RightShift;
            break;
        case KEY_CTRL:
            gKeyModifier &= ~KM_Ctrl;
            break;
        case KEY_ALT:
            gKeyModifier &= ~KM_Alt;
            break;
        }

        //Screen_PrintF("released: %x (%d)\n", scancode, scancode);
    }
    else
    {
        //key pressed

        switch (scancode)
        {
        case KEY_LEFTSHIFT:
            gKeyModifier |= KM_LeftShift;
            break;
        case KEY_RIGHTSHIFT:
            gKeyModifier |= KM_RightShift;
            break;
        case KEY_CTRL:
            gKeyModifier |= KM_Ctrl;
            break;
        case KEY_ALT:
            gKeyModifier |= KM_Alt;
            break;
        }

        //Screen_PrintF("pressed: %x (%d)\n", scancode, scancode);

        if ((gKeyModifier & KM_Ctrl) == KM_Ctrl)
        {
            int ttyIndex = scancode - KEY_F1;
            
            setActiveTty(ttyIndex);
        }
    }
}
