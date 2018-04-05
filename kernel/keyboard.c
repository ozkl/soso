#include "keyboard.h"
#include "isr.h"
#include "common.h"
#include "screen.h"
#include "ttydriver.h"
#include "fs.h"
#include "device.h"
#include "alloc.h"
#include "devfs.h"
#include "list.h"

static uint8* gKeyBuffer = NULL;
static uint32 gKeyBufferWriteIndex = 0;
static uint32 gKeyBufferReadIndex = 0;

#define KEYBUFFER_SIZE 128

static BOOL keyboard_open(File *file, uint32 flags);
static void keyboard_close(File *file);
static int32 keyboard_read(File *file, uint32 size, uint8 *buffer);

typedef struct Reader
{
    uint32 readIndex;
} Reader;

static List* gReaders = NULL;

static void handleKeyboardInterrupt(Registers *regs);

void initializeKeyboard()
{
    Device device;
    memset((uint8*)&device, 0, sizeof(Device));
    strcpy(device.name, "keyboard");
    device.deviceType = FT_CharacterDevice;
    device.open = keyboard_open;
    device.close = keyboard_close;
    device.read = keyboard_read;

    gKeyBuffer = kmalloc(KEYBUFFER_SIZE);
    memset((uint8*)gKeyBuffer, 0, KEYBUFFER_SIZE);

    gReaders = List_Create();

    registerDevice(&device);

    registerInterruptHandler(IRQ1, handleKeyboardInterrupt);
}

static BOOL keyboard_open(File *file, uint32 flags)
{
    int readIndex = 0;
    if (gKeyBufferWriteIndex > 0)
    {
        readIndex = gKeyBufferWriteIndex;
    }
    file->privateData = (void*)readIndex;

    List_Append(gReaders, file);

    return TRUE;
}

static void keyboard_close(File *file)
{
    //Screen_PrintF("keyboard_close\n");

    List_RemoveFirstOccurrence(gReaders, file);
}

static int32 keyboard_read(File *file, uint32 size, uint8 *buffer)
{
    int readIndex = (int)file->privateData;

    while (readIndex == gKeyBufferWriteIndex)
    {
        file->thread->state = TS_WAITIO;
        file->thread->state_privateData = keyboard_read;
        enableInterrupts();
        halt();
    }

    disableInterrupts();

    buffer[0] = gKeyBuffer[readIndex];
    readIndex++;
    readIndex %= KEYBUFFER_SIZE;

    file->privateData = (void*)readIndex;

    return 1;
}

static void handleKeyboardInterrupt(Registers *regs)
{
    uint8 scancode = 0;
    do
    {
        scancode = inb(0x64);
    } while ((scancode & 0x01) == 0);

    scancode = inb(0x60);

    gKeyBuffer[gKeyBufferWriteIndex] = scancode;
    gKeyBufferWriteIndex++;
    gKeyBufferWriteIndex %= KEYBUFFER_SIZE;

    //Wake readers
    List_Foreach(n, gReaders)
    {
        File* file = n->data;

        if (file->thread->state == TS_WAITIO)
        {
            if (file->thread->state_privateData == keyboard_read)
            {
                file->thread->state = TS_RUN;
                file->thread->state_privateData = NULL;
            }
        }
    }

    sendKeyInputToTTY(getActiveTTY(), scancode);
}
