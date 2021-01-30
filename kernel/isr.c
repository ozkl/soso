#include "common.h"
#include "timer.h"
#include "isr.h"

IsrFunction g_interrupt_handlers[256];

uint32_t g_isr_count = 0;
uint32_t g_irq_count = 0;

void interrupt_register(uint8_t n, IsrFunction handler)
{
    g_interrupt_handlers[n] = handler;
}

void handle_isr(Registers regs)
{
    //Screen_PrintF("handle_isr interrupt no:%d\n", regs.int_no);

    g_isr_count++;

    uint8_t int_no = regs.interruptNumber & 0xFF;

    if (g_interrupt_handlers[int_no] != 0)
    {
        IsrFunction handler = g_interrupt_handlers[int_no];
        handler(&regs);
    }
    else
    {
        printkf("unhandled interrupt: %d\n", int_no);
        printkf("Tick: %d\n", get_system_tick_count());
        PANIC("unhandled interrupt");
    }
}

void handle_irq(Registers regs)
{
    g_irq_count++;
    
    // end of interrupt message
    if (regs.interruptNumber >= 40)
    {
        //slave PIC
        outb(0xA0, 0x20);
    }

    outb(0x20, 0x20);

    //Screen_PrintF("irq: %d\n", regs.int_no);

    if (g_interrupt_handlers[regs.interruptNumber] != 0)
    {
        IsrFunction handler = g_interrupt_handlers[regs.interruptNumber];
        handler(&regs);
    }
    else
    {
        //printkf("unhandled IRQ: %d\n", regs.interruptNumber);
    }
    
}
