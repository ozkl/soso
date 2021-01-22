#include "common.h"
#include "descriptortables.h"
#include "isr.h"
#include "process.h"
#include "debugprint.h"

extern void flush_gdt(uint32);
extern void flush_idt(uint32);
extern void flush_tss();


static void gdt_initialize();
static void idt_initialize();
static void set_gdt_entry(int32 num, uint32 base, uint32 limit, uint8 access, uint8 gran);
static void set_idt_entry(uint8 num, uint32 base, uint16 sel, uint8 flags);

GdtEntry g_gdt_entries[6];
GdtPointer g_gdt_pointer;
IdtEntry g_idt_entries[256];
IdtPointer g_idt_pointer;
Tss g_tss;

static void handle_double_fault(Registers *regs);
static void handle_general_protection_fault(Registers *regs);

void descriptor_tables_initialize()
{
    gdt_initialize();

    idt_initialize();

    memset((uint8*)&g_interrupt_handlers, 0, sizeof(IsrFunction)*256);

    interrupt_register(8, handle_double_fault);
    interrupt_register(13, handle_general_protection_fault);
}

static void gdt_initialize()
{
    g_gdt_pointer.limit = (sizeof(GdtEntry) * 7) - 1;
    g_gdt_pointer.base  = (uint32)&g_gdt_entries;

    set_gdt_entry(0, 0, 0, 0, 0);                // 0x00 Null segment
    set_gdt_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // 0x08 Code segment
    set_gdt_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // 0x10 Data segment
    set_gdt_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // 0x18 User mode code segment
    set_gdt_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // 0x20 User mode data segment

    //TSS
    memset((uint8*)&g_tss, 0, sizeof(g_tss));
    g_tss.debug_flag = 0x00;
    g_tss.io_map = 0x00;
    g_tss.esp0 = 0;//0x1FFF0;
    g_tss.ss0 = 0x10;//0x18;

    g_tss.cs   = 0x0B; //from ring 3 - 0x08 | 3 = 0x0B
    g_tss.ss = g_tss.ds = g_tss.es = g_tss.fs = g_tss.gs = 0x13; //from ring 3 = 0x10 | 3 = 0x13
    uint32 tss_base = (uint32) &g_tss;
    uint32 tss_limit = tss_base + sizeof(g_tss);
    set_gdt_entry(5, tss_base, tss_limit, 0xE9, 0x00);

    set_gdt_entry(6, 0, 0xFFFFFFFF, 0x80, 0xCF); // Thread Local Storage pointer segment

    flush_gdt((uint32)&g_gdt_pointer);
    flush_tss();
}

// Set the value of one GDT entry.
static void set_gdt_entry(int32 num, uint32 base, uint32 limit, uint8 access, uint8 gran)
{
    g_gdt_entries[num].base_low    = (base & 0xFFFF);
    g_gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    g_gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    g_gdt_entries[num].limit_low   = (limit & 0xFFFF);
    g_gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    
    g_gdt_entries[num].granularity |= gran & 0xF0;
    g_gdt_entries[num].access      = access;
}

void irq_timer();

static void idt_initialize()
{
    g_idt_pointer.limit = sizeof(IdtEntry) * 256 -1;
    g_idt_pointer.base  = (uint32)&g_idt_entries;

    memset((uint8*)&g_idt_entries, 0, sizeof(IdtEntry)*256);

    // Remap the irq table.
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);

    set_idt_entry( 0, (uint32)isr0 , 0x08, 0x8E);
    set_idt_entry( 1, (uint32)isr1 , 0x08, 0x8E);
    set_idt_entry( 2, (uint32)isr2 , 0x08, 0x8E);
    set_idt_entry( 3, (uint32)isr3 , 0x08, 0x8E);
    set_idt_entry( 4, (uint32)isr4 , 0x08, 0x8E);
    set_idt_entry( 5, (uint32)isr5 , 0x08, 0x8E);
    set_idt_entry( 6, (uint32)isr6 , 0x08, 0x8E);
    set_idt_entry( 7, (uint32)isr7 , 0x08, 0x8E);
    set_idt_entry( 8, (uint32)isr8 , 0x08, 0x8E);
    set_idt_entry( 9, (uint32)isr9 , 0x08, 0x8E);
    set_idt_entry(10, (uint32)isr10, 0x08, 0x8E);
    set_idt_entry(11, (uint32)isr11, 0x08, 0x8E);
    set_idt_entry(12, (uint32)isr12, 0x08, 0x8E);
    set_idt_entry(13, (uint32)isr13, 0x08, 0x8E);
    set_idt_entry(14, (uint32)isr14, 0x08, 0x8E);
    set_idt_entry(15, (uint32)isr15, 0x08, 0x8E);
    set_idt_entry(16, (uint32)isr16, 0x08, 0x8E);
    set_idt_entry(17, (uint32)isr17, 0x08, 0x8E);
    set_idt_entry(18, (uint32)isr18, 0x08, 0x8E);
    set_idt_entry(19, (uint32)isr19, 0x08, 0x8E);
    set_idt_entry(20, (uint32)isr20, 0x08, 0x8E);
    set_idt_entry(21, (uint32)isr21, 0x08, 0x8E);
    set_idt_entry(22, (uint32)isr22, 0x08, 0x8E);
    set_idt_entry(23, (uint32)isr23, 0x08, 0x8E);
    set_idt_entry(24, (uint32)isr24, 0x08, 0x8E);
    set_idt_entry(25, (uint32)isr25, 0x08, 0x8E);
    set_idt_entry(26, (uint32)isr26, 0x08, 0x8E);
    set_idt_entry(27, (uint32)isr27, 0x08, 0x8E);
    set_idt_entry(28, (uint32)isr28, 0x08, 0x8E);
    set_idt_entry(29, (uint32)isr29, 0x08, 0x8E);
    set_idt_entry(30, (uint32)isr30, 0x08, 0x8E);
    set_idt_entry(31, (uint32)isr31, 0x08, 0x8E);

    set_idt_entry(32, (uint32)irq_timer, 0x08, 0x8E);
    set_idt_entry(33, (uint32)irq1, 0x08, 0x8E);
    set_idt_entry(34, (uint32)irq2, 0x08, 0x8E);
    set_idt_entry(35, (uint32)irq3, 0x08, 0x8E);
    set_idt_entry(36, (uint32)irq4, 0x08, 0x8E);
    set_idt_entry(37, (uint32)irq5, 0x08, 0x8E);
    set_idt_entry(38, (uint32)irq6, 0x08, 0x8E);
    set_idt_entry(39, (uint32)irq7, 0x08, 0x8E);
    set_idt_entry(40, (uint32)irq8, 0x08, 0x8E);
    set_idt_entry(41, (uint32)irq9, 0x08, 0x8E);
    set_idt_entry(42, (uint32)irq10, 0x08, 0x8E);
    set_idt_entry(43, (uint32)irq11, 0x08, 0x8E);
    set_idt_entry(44, (uint32)irq12, 0x08, 0x8E);
    set_idt_entry(45, (uint32)irq13, 0x08, 0x8E);
    set_idt_entry(46, (uint32)irq14, 0x08, 0x8E);
    set_idt_entry(47, (uint32)irq15, 0x08, 0x8E);
    set_idt_entry(128, (uint32)isr128, 0x08, 0x8E);

    flush_idt((uint32)&g_idt_pointer);
}

static void set_idt_entry(uint8 num, uint32 base, uint16 sel, uint8 flags)
{
    g_idt_entries[num].base_lo = base & 0xFFFF;
    g_idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

    g_idt_entries[num].sel     = sel;
    g_idt_entries[num].always0 = 0;
    g_idt_entries[num].flags   = flags  | 0x60;
}

static void handle_double_fault(Registers *regs)
{
    printkf("Double fault!!! Error code:%d\n", regs->errorCode);

    PANIC("Double fault!!!");
}

static void handle_general_protection_fault(Registers *regs)
{
    log_printf("General protection fault!!! Error code:%d - IP:%x\n", regs->errorCode, regs->eip);

    Thread* faulting_thread = thread_get_current();
    if (NULL != faulting_thread)
    {
        Thread* main_thread = thread_get_first();

        if (main_thread == faulting_thread)
        {
            PANIC("General protection fault in Kernel main thread!!!");
        }
        else
        {
            log_printf("Faulting thread is %d and its state is %d\n", faulting_thread->threadId, faulting_thread->state);

            if (faulting_thread->user_mode)
            {
                if (faulting_thread->state == TS_CRITICAL ||
                    faulting_thread->state == TS_UNINTERRUPTIBLE)
                {
                    log_printf("CRITICAL!! process %d\n", faulting_thread->owner->pid);

                    process_change_state(faulting_thread->owner, TS_SUSPEND);

                    thread_change_state(faulting_thread, TS_DEAD, (void*)regs->errorCode);
                }
                else
                {
                    //log_printf("Destroying process %d\n", faulting_thread->owner->pid);

                    //destroyProcess(faulting_thread->owner);

                    //TODO: state in RUN?

                    log_printf("General protection fault %d\n", faulting_thread->owner->pid);

                    thread_signal(faulting_thread, SIGILL);
                }
            }
            else
            {
                log_printf("Destroying kernel thread %d\n", faulting_thread->threadId);

                thread_destroy(faulting_thread);
            }

            wait_for_schedule();
        }
    }
    else
    {
        PANIC("General protection fault!!!");
    }
}
