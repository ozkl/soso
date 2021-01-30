#ifndef ISR_H
#define ISR_H

#include "common.h"

#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

typedef struct Registers
{
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; //pushed by pusha
    uint32_t interruptNumber, errorCode;             //if applicable
    uint32_t eip, cs, eflags, userEsp, ss;           //pushed by the CPU
} Registers;

typedef void (*IsrFunction)(Registers*);

extern IsrFunction g_interrupt_handlers[];

extern uint32_t g_isr_count;
extern uint32_t g_irq_count;

void interrupt_register(uint8_t n, IsrFunction handler);


#endif //ISR_H
