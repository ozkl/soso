#include "ttydriver.h"
#include "device.h"
#include "screen.h"
#include "serial.h"
#include "devfs.h"
#include "alloc.h"
#include "common.h"
#include "list.h"
#include "fifobuffer.h"
#include "gfx.h"
#include "desktopenvironment.h"

static List* gTtyList = NULL;

static List* gReaderList = NULL;

static Tty* gActiveTty = NULL;

static uint8 gKeyModifier = 0;

typedef enum KeyModifier
{
    KM_LeftShift = 1,
    KM_RightShift = 2,
    KM_Ctrl = 4,
    KM_Alt = 8
} KeyModifier;

enum
{
    KEY_LEFTSHIFT = 0x2A,
    KEY_RIGHTSHIFT = 0x36,
    KEY_CTRL = 0x1D,
    KEY_ALT = 0x38,
    KEY_CAPSLOCK = 0x3A,
    KEY_F1 = 0x3B,
    KEY_F2 = 0x3C,
    KEY_F3 = 0x3D
};

// PC keyboard interface constants

#define KBSTATP         0x64    // kbd controller status port(I)
#define KBS_DIB         0x01    // kbd data in buffer
#define KBDATAP         0x60    // kbd data port(I)

#define NO              0

#define SHIFT           (1<<0)
#define CTL             (1<<1)
#define ALT             (1<<2)

#define CAPSLOCK        (1<<3)
#define NUMLOCK         (1<<4)
#define SCROLLLOCK      (1<<5)

#define E0ESC           (1<<6)

// Special keycodes
#define KEY_HOME        0xE0
#define KEY_END         0xE1
#define KEY_UP          0xE2
#define KEY_DN          0xE3
#define KEY_LF          0xE4
#define KEY_RT          0xE5
#define KEY_PGUP        0xE6
#define KEY_PGDN        0xE7
#define KEY_INS         0xE8
#define KEY_DEL         0xE9

// C('A') == Control-A
#define C(x) (x - '@')




static uint8 gKeyMap[256] =
{
  NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  // 0x10
  'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  // 0x20
  '\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',  // 0x30
  NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
  NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
  [0x9C] = '\n',      // KP_Enter
  [0xB5] = '/',       // KP_Div
  [0xC8] = KEY_UP,    [0xD0] = KEY_DN,
  [0xC9] = KEY_PGUP,  [0xD1] = KEY_PGDN,
  [0xCB] = KEY_LF,    [0xCD] = KEY_RT,
  [0x97] = KEY_HOME,  [0xCF] = KEY_END,
  [0xD2] = KEY_INS,   [0xD3] = KEY_DEL
};

static uint8 gKeyShiftMap[256] =
{
  NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
  'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
  '"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
  'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',  // 0x30
  NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
  NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
  [0x9C] = '\n',      // KP_Enter
  [0xB5] = '/',       // KP_Div
  [0xC8] = KEY_UP,    [0xD0] = KEY_DN,
  [0xC9] = KEY_PGUP,  [0xD1] = KEY_PGDN,
  [0xCB] = KEY_LF,    [0xCD] = KEY_RT,
  [0x97] = KEY_HOME,  [0xCF] = KEY_END,
  [0xD2] = KEY_INS,   [0xD3] = KEY_DEL
};

static BOOL tty_open(File *file, uint32 flags);
static void tty_close(File *file);
static int32 tty_read(File *file, uint32 size, uint8 *buffer);
static int32 tty_write(File *file, uint32 size, uint8 *buffer);

static uint8 getCharacterForScancode(KeyModifier modifier, uint8 scancode);
static void processScancode(uint8 scancode);

static void createDesktopEnvironmentTTY();
static void updateDesktop(Tty* tty);

void initializeTTYs(BOOL graphicMode)
{
    gTtyList = List_Create();

    gReaderList = List_Create();

    for (int i = 1; i <= 9; ++i)
    {
        Tty* tty = NULL;
        if (graphicMode)
        {
            tty = createTty(768 / 16, 1024 / 9, Gfx_FlushFromTty);
            tty->change = Gfx_ChangeTty;
        }
        else
        {
            tty = createTty(25, 80, Screen_FlushFromTty);
        }

        tty->color = 0x0A;

        List_Append(gTtyList, tty);

        Device device;
        memset((uint8*)&device, 0, sizeof(Device));
        sprintf(device.name, "tty%d", i);
        device.deviceType = FT_CharacterDevice;
        device.open = tty_open;
        device.close = tty_close;
        device.read = tty_read;
        device.write = tty_write;
        device.privateData = tty;
        registerDevice(&device);
    }

    gActiveTty = List_GetFirstNode(gTtyList)->data;

    if (graphicMode)
    {
        createDesktopEnvironmentTTY();
    }
}

static void createDesktopEnvironmentTTY()
{
    DesktopEnvironment* de = DE_GetDefault();
    uint16 width = DE_GetWidth(de);
    uint16 height = DE_GetHeight(de);

    Tty* tty = createTty(height / 16, width / 9, NULL);
    tty->color = 0x0A;
    tty->privateData = de;
    tty->update = updateDesktop;
    List_Append(gTtyList, tty);

    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    sprintf(device.name, "tty%s", "Desktop");
    device.deviceType = FT_CharacterDevice;
    device.privateData = tty;
    registerDevice(&device);
}

Tty* getActiveTTY()
{
    return gActiveTty;
}

void sendKeyInputToTTY(uint8 scancode)
{
    beginCriticalSection();

    FifoBuffer_enqueue(gActiveTty->keyBuffer, &scancode, 1);

    processScancode(scancode);

    //Wake readers
    List_Foreach(n, gReaderList)
    {
        File* file = n->data;

        if (file->thread->state == TS_WAITIO)
        {
            if (file->thread->waitingIO_privateData == gActiveTty)
            {
                file->thread->state = TS_RUN;
                file->thread->waitingIO_privateData = NULL;
            }
        }
    }

    endCriticalSection();
}

static BOOL tty_open(File *file, uint32 flags)
{
    //Screen_PrintF("tty_open: pid:%d\n", file->process->pid);

    List_Append(gReaderList, file);

    return TRUE;
}

static void tty_close(File *file)
{
    List_RemoveFirstOccurrence(gReaderList, file);
}

static int32 tty_read(File *file, uint32 size, uint8 *buffer)
{
    if (size > 0)
    {
        while (TRUE)
        {
            disableInterrupts();

            Tty* tty = (Tty*)file->node->privateNodeData;

            //Block until this becomes active TTY
            //Block until key buffer has data
            while (FifoBuffer_isEmpty(tty->keyBuffer) || tty != gActiveTty)
            {
                file->thread->state = TS_WAITIO;
                file->thread->waitingIO_privateData = tty;
                enableInterrupts();
                halt();
            }

            disableInterrupts();

            uint8 scancode = 0;
            FifoBuffer_dequeue(tty->keyBuffer, &scancode, 1);

            uint8 character = getCharacterForScancode(gKeyModifier, scancode);

            //Screen_PrintF("usedBytes:%d --- %d:%d\n", gTtyDriverKeyBuffer->usedBytes, scancode, character);

            uint8 keyRelease = (0x80 & scancode); //ignore release event

            if (character > 0 && keyRelease == 0)
            {
                if (tty->lineBufferIndex >= TTY_LINEBUFFER_SIZE - 1)
                {
                    tty->lineBufferIndex = 0;
                }

                if (character == '\b')
                {
                    if (tty->lineBufferIndex > 0)
                    {
                        tty->lineBuffer[--tty->lineBufferIndex] = '\0';

                        tty_write(file, 1, &character);
                    }
                }
                else
                {
                    tty->lineBuffer[tty->lineBufferIndex++] = character;

                    tty_write(file, 1, &character);
                }



                if (character == '\n')
                {
                    int bytesToCopy = MIN(tty->lineBufferIndex, size);
                    memcpy(buffer, tty->lineBuffer, bytesToCopy);
                    //Serial_PrintF("%d bytes. lastchar:%d\r\n", bytesToCopy, character);
                    tty->lineBufferIndex = 0;

                    return bytesToCopy;
                }
            }
        }
    }

    return -1;
}

static int32 tty_write(File *file, uint32 size, uint8 *buffer)
{
    buffer[size] = '\0';

    Tty_PutText(file->node->privateNodeData, (const char*)buffer);

    if (gActiveTty == file->node->privateNodeData)
    {
        if (gActiveTty->flushScreen)
        {
            gActiveTty->flushScreen(gActiveTty);
        }
    }

    return size;
}

static void setActiveTty(Tty* tty)
{
    gActiveTty = tty;

    if (tty->change)
    {
        tty->change(tty);
    }

    if (tty->flushScreen)
    {
        tty->flushScreen(tty);
    }

    //Serial_PrintF("line:%d column:%d\r\n", gActiveTty->currentLine, gActiveTty->currentColumn);
}

static void updateDesktop(Tty* tty)
{
    DE_Update(tty, (DesktopEnvironment*)tty->privateData);
}

static uint8 getCharacterForScancode(KeyModifier modifier, uint8 scancode)
{
    //return gKeyboardLayout[scancode];
    if ((modifier & KM_LeftShift) == KM_LeftShift || (modifier & KM_RightShift) == KM_RightShift)
    {
        return gKeyShiftMap[scancode];
    }

    return gKeyMap[scancode];
}

static void applyModifierKeys(KeyModifier modifier, uint8 scancode)
{
    if ((modifier & KM_Ctrl) == KM_Ctrl)
    {
        int ttyIndex = scancode - KEY_F1;
        //printkf("TTY:%d\n", ttyIndex);
        int ttyCount = List_GetCount(gTtyList);
        if (ttyIndex >= 0 && ttyIndex < ttyCount)
        {
            int i = 0;
            List_Foreach(n, gTtyList)
            {
                if (ttyIndex == i)
                {
                    setActiveTty(n->data);
                    break;
                }
                ++i;
            }
        }
    }
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

        applyModifierKeys(gKeyModifier, scancode);
    }
}
