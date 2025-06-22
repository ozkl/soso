#include "vmm.h"
#include "common.h"
#include "alloc.h"
#include "isr.h"
#include "process.h"
#include "list.h"
#include "log.h"
#include "serial.h"

extern uint32_t* kernel_page_directory;
uint32_t *g_kernel_page_directory = (uint32_t *)&kernel_page_directory;
uint32_t g_kernel_page_directory_physical = 0;
uint8_t g_physical_page_frame_bitmap[RAM_AS_4K_PAGES / 8];

static int g_total_page_count = 0;

static void handle_page_fault(Registers *regs);

static void *get_physical_address(void * v_address)
{
    uint32_t pd_index = (uint32_t)v_address >> 22;
    uint32_t pt_index = (uint32_t)v_address >> 12 & 0x03FF;

    uint32_t *pd = (uint32_t*)0xFFFFF000;
    if ((pd[pd_index] & PG_PRESENT) == PG_PRESENT)
    {
        uint32_t *pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);
        if ((pt[pt_index] & PG_PRESENT) == PG_PRESENT)
        {
            return (void *)((pt[pt_index] & ~0xFFF) + ((uint32_t)v_address & 0xFFF));
        }
    }

    return NULL;
}

static uint32_t get_physical_address_of_page_directory()
{
    uint32_t* pd = (uint32_t*)0xFFFFF000;  // Recursive mapping: VA of the page directory
    const uint32_t mask = 0xFFFFF000;
    return pd[1023] & mask;           // Mask out flags, keep only the physical address
}

void vmm_initialize(uint32_t high_mem)
{
    uint32_t pg = 0;
    uint32_t i = 0;

    g_kernel_page_directory_physical = ((uint32_t)g_kernel_page_directory - KERNEL_VIRTUAL_BASE);

    interrupt_register(14, handle_page_fault);

    g_total_page_count = (high_mem * 1024) / PAGESIZE_4K;

    //Mark all memory space available
    for (pg = 0; pg < g_total_page_count / 8; ++pg)
    {
        g_physical_page_frame_bitmap[pg] = 0;
    }

    //Mark physically unexistent memory as used or unavailable
    for (pg = g_total_page_count / 8; pg < RAM_AS_4K_PAGES / 8; ++pg)
    {
        g_physical_page_frame_bitmap[pg] = 0xFF;
    }

    uint32_t end_index_4m = PAGE_INDEX_4M(g_modules_end_physical);
    
    //Map first n pages to KERNEL_VIRTUAL_BASE for accessing data that GRUB loaded
    uint32_t index_kernel_base = PAGE_INDEX_4M(KERNEL_VIRTUAL_BASE);
    for (i = 0; i <= end_index_4m; ++i)
    {
        g_kernel_page_directory[i + index_kernel_base] = (i * PAGESIZE_4M | (PG_PRESENT | PG_WRITE | PG_4MB));//add PG_USER for accesing kernel code in user mode
        uint32_t v_address = (i + index_kernel_base) * PAGESIZE_4M;
    }

    //Mark those pages reserved
    for (pg = PAGE_INDEX_4K(0x0); pg < (int)(PAGE_INDEX_4K(g_modules_end_physical)); ++pg)
    {
        SET_PAGEFRAME_USED(g_physical_page_frame_bitmap, pg);
    }


    g_kern_heap_begin = (index_kernel_base + end_index_4m + 1) * PAGESIZE_4M;


    serial_printf("g_kernel_page_directory=%x\n", g_kernel_page_directory);
    serial_printf("g_kernel_page_directory_physical=%x\n", g_kernel_page_directory_physical);

    serial_printf("g_kern_heap_begin=%x\n", g_kern_heap_begin);
    

    //Recursive page directory strategy
    g_kernel_page_directory[1023] = g_kernel_page_directory_physical | PG_PRESENT | PG_WRITE;
    CHANGE_PD(g_kernel_page_directory_physical);
    serial_printf("calc_physical=%x\n", get_physical_address_of_page_directory());

    /*
    //Enable paging
    asm("	mov %0, %%eax \n \
        mov %%eax, %%cr3 \n \
        mov %%cr4, %%eax \n \
        or %2, %%eax \n \
        mov %%eax, %%cr4 \n \
        mov %%cr0, %%eax \n \
        or %1, %%eax \n \
        mov %%eax, %%cr0"::"m"(g_kernel_page_directory), "i"(PAGING_FLAG), "i"(PSE_FLAG));

    */

    initialize_kernel_heap();
}

void unmap_first_4m()
{
    //unmap first 4M page previously mapped from boot.asm
    g_kernel_page_directory[0] = 0;
    
    CHANGE_PD(g_kernel_page_directory_physical);
}

uint32_t vmm_acquire_page_frame_4k()
{
    int byte, bit;
    uint32_t page = -1;

    int pid = -1;
    Thread* thread = thread_get_current();
    if (thread)
    {
        Process* process = thread->owner;
        if (process)
        {
            pid = process->pid;
        }
    }

    for (byte = 0; byte < RAM_AS_4K_PAGES / 8; byte++)
    {
        if (g_physical_page_frame_bitmap[byte] != 0xFF)
        {
            for (bit = 0; bit < 8; bit++)
            {
                if (!(g_physical_page_frame_bitmap[byte] & (1 << bit)))
                {
                    page = 8 * byte + bit;
                    SET_PAGEFRAME_USED(g_physical_page_frame_bitmap, page);

                    //log_printf("DEBUG: Acquired 4K Physical %x (pid:%d)\n", page * PAGESIZE_4K, pid);
                    //serial_printf("DEBUG: Acquired 4K Physical %x (pid:%d)\n", page * PAGESIZE_4K, pid);

                    return (page * PAGESIZE_4K);
                }
            }
        }
    }

    PANIC("Memory is full!");
    return (uint32_t)-1;
}

void vmm_release_page_frame_4k(uint32_t p_addr)
{
    //log_printf("DEBUG: Released 4K Physical %x\n", p_addr);
    //serial_printf("DEBUG: Released 4K Physical %x\n", p_addr);

    SET_PAGEFRAME_UNUSED(g_physical_page_frame_bitmap, p_addr);
}

//when this called, 0x0 address must have been unmapped already (unmap_first_4m())
uint32_t vmm_acquire_page_directory()
{
    uint32_t p_address = vmm_acquire_page_frame_4k();
    if (p_address == (int32_t)-1)
    {
        return NULL;
    }

    uint32_t* v_address = (uint32_t*)0x0;

    //map 0x00000000 to access temporarly
    BOOL mapped_success = vmm_add_page_to_pd((char*)v_address, p_address, 0);
    if (!mapped_success)
    {
        vmm_release_page_frame_4k(p_address);
        return NULL;
    }

    
    uint32_t* pd = v_address;


    //Let's initialize it. First we should sync with shared part of the kernel page directory to achive the same view.

    uint32_t index = PAGE_INDEX_4M(KERNEL_VIRTUAL_BASE);
    for (uint32_t i = 0; i < index; ++i)
    {
        pd[i] = 0;
    }

    for (uint32_t i = index; i < 1023; ++i)
    {
        pd[i] = g_kernel_page_directory[i]& ~PG_OWNED;
    }

    pd[1023] = p_address | PG_PRESENT | PG_WRITE;

    vmm_remove_page_from_pd((char*)v_address);

    return p_address;
}

void vmm_destroy_page_directory_with_memory(uint32_t physical_pd)
{
    begin_critical_section();

    uint32_t* pd = (uint32_t*)0xFFFFF000;

    uint32_t cr3 = read_cr3();

    CHANGE_PD(physical_pd);

    uint32_t index = PAGE_INDEX_4M(KERNEL_VIRTUAL_BASE);
    for (int pd_index = 0; pd_index < index; ++pd_index)
    {
        if ((pd[pd_index] & PG_PRESENT) == PG_PRESENT)
        {
            uint32_t* pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);

            for (int pt_index = 0; pt_index < 1024; ++pt_index)
            {
                if ((pt[pt_index] & PG_PRESENT) == PG_PRESENT)
                {
                    if ((pt[pt_index] & PG_OWNED) == PG_OWNED)
                    {
                        uint32_t physicalFrame = pt[pt_index] & ~0xFFF;

                        vmm_release_page_frame_4k(physicalFrame);
                    }
                }
                pt[pt_index] = 0;
            }

            if ((pd[pd_index] & PG_OWNED) == PG_OWNED)
            {
                uint32_t physicalFramePT = pd[pd_index] & ~0xFFF;
                vmm_release_page_frame_4k(physicalFramePT);
            }
        }

        pd[pd_index] = 0;
    }

    end_critical_section();
    //return to caller's Page Directory
    CHANGE_PD(cr3);
}

//When calling this function:
//If it is intended to alloc kernel memory, v_addr must be > KERNEL_VIRTUAL_BASE.
//If it is intended to alloc user memory, v_addr must be < KERNEL_VIRTUAL_BASE.
//Works for active Page Directory!
BOOL vmm_add_page_to_pd(char *v_addr, uint32_t p_addr, int flags)
{
    // Both addresses are page-aligned.

    //serial_printf("vmm_add_page_to_pd v_addr:%x p_addr:%x\n", v_addr, p_addr);

    int pd_index = (((uint32_t) v_addr) >> 22);
    int pt_index = (((uint32_t) v_addr) >> 12) & 0x03FF;

    uint32_t* pd = (uint32_t*)0xFFFFF000;

    uint32_t* pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);

    uint32_t cr3 = 0;

    if (v_addr >= (char*)(KERNEL_VIRTUAL_BASE))
    {
        cr3 = read_cr3();

        CHANGE_PD(g_kernel_page_directory_physical);
    }

    //serial_printf("vmm_add_page_to_pd 1");
    if ((pd[pd_index] & PG_PRESENT) != PG_PRESENT)
    {
        //serial_printf("vmm_add_page_to_pd 2");
        uint32_t tablePhysical = vmm_acquire_page_frame_4k();

        //serial_printf("vmm_add_page_to_pd 3");
        pd[pd_index] = (tablePhysical) | (flags & 0xFFF) | (PG_PRESENT | PG_WRITE);

        //serial_printf("vmm_add_page_to_pd 4");

        INVALIDATE(v_addr);

        //serial_printf("vmm_add_page_to_pd 5");

        //Zero out table as it may contain thrash data from previously allocated page frame
        for (int i = 0; i < 1024; ++i)
        {
            pt[i] = 0;
        }
    }

    if ((pt[pt_index] & PG_PRESENT) == PG_PRESENT)
    {
        //serial_printf("vmm_add_page_to_pd 6");

        if (0 != cr3)
        {
            //restore
            CHANGE_PD(cr3);
        }

        return FALSE;
    }

    pt[pt_index] = (p_addr) | (flags & 0xFFF) | (PG_PRESENT | PG_WRITE);

    //serial_printf("vmm_add_page_to_pd 7");

    INVALIDATE(v_addr);

    //serial_printf("vmm_add_page_to_pd 8");

    if (0 != cr3)
    {
        //restore
        CHANGE_PD(cr3);
    }

    return TRUE;
}

//Works for active Page Directory!
BOOL vmm_remove_page_from_pd(char *v_addr)
{
    int pd_index = (((uint32_t) v_addr) >> 22);
    int pt_index = (((uint32_t) v_addr) >> 12) & 0x03FF;

    uint32_t* pd = (uint32_t*)0xFFFFF000;

    uint32_t cr3 = 0;

    if (v_addr >= (char*)(KERNEL_VIRTUAL_BASE))
    {
        cr3 = read_cr3();

        CHANGE_PD(g_kernel_page_directory_physical);
    }


    if ((pd[pd_index] & PG_PRESENT) == PG_PRESENT)
    {
        uint32_t* pt = ((uint32_t*)0xFFC00000) + (0x400 * pd_index);

        uint32_t physical_frame = pt[pt_index] & ~0xFFF;

        if ((pt[pt_index] & PG_OWNED) == PG_OWNED)
        {
            vmm_release_page_frame_4k(physical_frame);
        }

        pt[pt_index] = 0;

        BOOL all_unmapped = TRUE;
        for (int i = 0; i < 1024; ++i)
        {
            if (pt[i] != 0)
            {
                all_unmapped = FALSE;
                break;
            }
        }

        if (all_unmapped)
        {
            //All page table entries are unmapped.
            //Lets destroy this page table and remove it from PD

            uint32_t physical_frame_pt = pd[pd_index] & ~0xFFF;

            if ((pd[pd_index] & PG_OWNED) == PG_OWNED)
            {
                pd[pd_index] = 0;

                vmm_release_page_frame_4k(physical_frame_pt);
            }
        }

        INVALIDATE(v_addr);

        if (0 != cr3)
        {
            //restore
            CHANGE_PD(cr3);
        }

        return TRUE;
    }

    if (0 != cr3)
    {
        //restore
        CHANGE_PD(cr3);
    }

    return FALSE;
}

uint32_t vmm_get_total_page_count()
{
    return g_total_page_count;
}

uint32_t vmm_get_used_page_count()
{
    int count = 0;
    for (int i = 0; i < g_total_page_count; ++i)
    {
        if(IS_PAGEFRAME_USED(g_physical_page_frame_bitmap, i))
        {
            ++count;
        }
    }

    return count;
}

uint32_t vmm_get_free_page_count()
{
    return g_total_page_count - vmm_get_used_page_count();
}

static void print_page_fault_info(uint32_t faulting_address, Registers *regs)
{
    int present = regs->errorCode & 0x1;
    int rw = regs->errorCode & 0x2;
    int us = regs->errorCode & 0x4;
    int reserved = regs->errorCode & 0x8;
    int id = regs->errorCode & 0x10;

    log_printf("Page fault!!! When trying to %s %x - IP:%x\n", rw ? "write to" : "read from", faulting_address, regs->eip);

    log_printf("The page was %s\n", present ? "present" : "not present");

    if (reserved)
    {
        log_printf("Reserved bit was set\n");
    }

    if (id)
    {
        log_printf("Caused by an instruction fetch\n");
    }

    log_printf("CPU was in %s\n", us ? "user-mode" : "supervisor mode");
}

static void print_thread_syscall_info(Thread* thread)
{
    log_printf("Thread[%d] syscall is %d, syscall state is %d, user_ip:%x\nArgs: %x %x %x %x %x\n",
            thread->threadId, thread->last_syscall.number, thread->last_syscall.state, thread->last_syscall.user_ip,
            thread->last_syscall.arguments[0], thread->last_syscall.arguments[1],
            thread->last_syscall.arguments[2], thread->last_syscall.arguments[3],
            thread->last_syscall.arguments[4]);
}

static void handle_page_fault(Registers *regs)
{
    // A page fault has occurred.

    // The faulting address is stored in the CR2 register.
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

    //log_printf("page_fault()\n");
    //log_printf("stack of handler is %x\n", &faulting_address);

    Thread* faulting_thread = thread_get_current();
    if (NULL != faulting_thread)
    {
        Thread* main_thread = thread_get_first();

        if (main_thread == faulting_thread)
        {
            print_page_fault_info(faulting_address, regs);

            PANIC("Page fault in Kernel main thread!!!");
        }
        else
        {
            print_page_fault_info(faulting_address, regs);

            log_printf("Faulting thread is %d and its state is %d\n", faulting_thread->threadId, faulting_thread->state);

            print_thread_syscall_info(faulting_thread);

            if (faulting_thread->user_mode)
            {
                if (faulting_thread->state == TS_CRITICAL ||
                    faulting_thread->state == TS_UNINTERRUPTIBLE)
                {
                    log_printf("CRITICAL!! process %d\n", faulting_thread->owner->pid);

                    process_change_state(faulting_thread->owner, TS_SUSPEND);

                    thread_change_state(faulting_thread, TS_DEAD, (void*)faulting_address);
                }
                else
                {
                    //log_printf("Destroying process %d\n", faulting_thread->owner->pid);

                    //destroyProcess(faulting_thread->owner);

                    //TODO: state in RUN?

                    log_printf("Segmentation fault pid:%d\n", faulting_thread->owner->pid);

                    thread_signal(faulting_thread, SIGSEGV);
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
        print_page_fault_info(faulting_address, regs);

        PANIC("Page fault!!!");
    }
}

void vmm_initialize_process_pages(Process* process)
{
    int page = 0;

    for (page = 0; page < RAM_AS_4K_PAGES / 8; ++page)
    {
        process->mmapped_virtual_memory[page] = 0xFF;
    }

    for (page = PAGE_INDEX_4K(0); page < (int)(PAGE_INDEX_4K(KERNEL_VIRTUAL_BASE)); ++page)
    {
        SET_PAGEFRAME_UNUSED(process->mmapped_virtual_memory, page * PAGESIZE_4K);
    }

    //Page Tables position marked as used. It is after MEMORY_END.
}

//if this fails (return NULL), the caller should clean up physical page frames
void* vmm_map_memory(Process* process, uint32_t v_address_search_start, uint32_t* p_address_array, uint32_t page_count, BOOL own)
{
    int page_index = 0;

    if (NULL == p_address_array || page_count == 0)
    {
        return NULL;
    }

    uint32_t found_adjacent = 0;

    uint32_t v_mem = 0;

    for (page_index = PAGE_INDEX_4K(v_address_search_start); page_index < (int)(PAGE_INDEX_4K(KERNEL_VIRTUAL_BASE)); ++page_index)
    {
        if (IS_PAGEFRAME_USED(process->mmapped_virtual_memory, page_index))
        {
            found_adjacent = 0;
            v_mem = 0;
        }
        else
        {
            if (0 == found_adjacent)
            {
                v_mem = page_index * PAGESIZE_4K;
            }
            ++found_adjacent;
        }

        if (found_adjacent == page_count)
        {
            break;
        }
    }

    //log_printf("vmm_map_memory: needed:%d foundAdjacent:%d v_mem:%x\n", neededPages, foundAdjacent, v_mem);

    if (found_adjacent == page_count)
    {
        int own_flag = 0;
        if (own)
        {
            own_flag = PG_OWNED;
        }

        uint32_t v = v_mem;
        for (uint32_t i = 0; i < page_count; ++i)
        {
            uint32_t p = p_address_array[i];
            p = p & 0xFFFFF000;

            vmm_add_page_to_pd((char*)v, p, PG_USER | own_flag);

            //log_printf("MMAPPED: %s(%d) virtual:%x -> physical:%x owned:%d\n", process->name, process->pid, v, p, own);

            SET_PAGEFRAME_USED(process->mmapped_virtual_memory, PAGE_INDEX_4K(v));

            v += PAGESIZE_4K;
        }

        return (void*)v_mem;
    }

    return NULL;
}

BOOL vmm_unmap_memory(Process* process, uint32_t v_address, uint32_t page_count)
{
    if (page_count == 0)
    {
        return FALSE;
    }

    uint32_t page_index = 0;

    uint32_t needed_pages = page_count;

    uint32_t old = v_address;
    v_address &= 0xFFFFF000;

    //log_printf("pageFrame dealloc from munmap:%x aligned:%x\n", old, v_address);

    uint32_t start_index = PAGE_INDEX_4K(v_address);
    uint32_t end_index = start_index + needed_pages;

    BOOL result = FALSE;

    for (page_index = start_index; page_index < end_index; ++page_index)
    {
        if (IS_PAGEFRAME_USED(process->mmapped_virtual_memory, page_index))
        {
            char* v_addr = (char*)(page_index * PAGESIZE_4K);

            vmm_remove_page_from_pd(v_addr);

            //log_printf("UNMAPPED: %s(%d) virtual:%x\n", process->name, process->pid, v_addr);

            SET_PAGEFRAME_UNUSED(process->mmapped_virtual_memory, v_addr);

            result = TRUE;
        }
    }

    return result;
}
