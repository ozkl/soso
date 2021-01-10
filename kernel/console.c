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
Terminal* gActiveTerminal = NULL;

static uint8 gKeyModifier = 0;


static BOOL console_open(File *file, uint32 flags);
static void console_close(File *file);
static int32 console_ioctl(File *file, int32 request, void * argp);

static uint8 getCharacterForScancode(KeyModifier modifier, uint8 scancode);
static void processScancode(uint8 scancode);

static void setActiveTerminal(uint32 index);

void initializeConsole(BOOL graphicMode)
{
    for (int i = 0; i < 10; ++i)
    {
        Terminal* terminal = NULL;
        FileSystemNode* ttyNode = createTTYDev();
        if (ttyNode)
        {
            TtyDev* ttyDev = (TtyDev*)ttyNode->privateNodeData;
        
            terminal = Terminal_create(ttyDev, graphicMode);
        }
        
        gTerminals[i] = terminal;
    }

    setActiveTerminal(0);

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
        Terminal_sendKey(gActiveTerminal, gKeyModifier, character);
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

static void setActiveTerminal(uint32 index)
{
    if (index >= 0 && index < TERMINAL_COUNT)
    {
        gActiveTerminalIndex = index;
        gActiveTerminal = gTerminals[gActiveTerminalIndex];

        Gfx_Fill(0xFFFFFFFF);

        if (gActiveTerminal->refreshFunction)
        {
            gActiveTerminal->refreshFunction(gActiveTerminal);
        }
    }
}

static uint8 getCharacterForScancode(KeyModifier modifier, uint8 scancode)
{
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

        if ((gKeyModifier & KM_Ctrl) == KM_Ctrl)
        {
            int ttyIndex = scancode - KEY_F1;
            
            setActiveTerminal(ttyIndex);
        }
    }
}
