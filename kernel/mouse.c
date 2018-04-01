#include "mouse.h"
#include "isr.h"
#include "common.h"


static void handleMouseInterrupt(Registers *regs);

void initializeMouse()
{
    printkf("mouse init\n");
    registerInterruptHandler(IRQ12, handleMouseInterrupt);

    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
}

static void handleMouseInterrupt(Registers *regs)
{
    uint8 scancode = 0;
    do
    {
        scancode = inb(0x64);
    } while ((scancode & 0x01) == 0);

    scancode = inb(0x60);

    printkf("mouse:%d\n", scancode);
}
