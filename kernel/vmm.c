#include "vmm.h"
#include "common.h"
#include "screen.h"
#include "alloc.h"
#include "isr.h"
#include "process.h"
#include "debugprint.h"

uint32 *gKernelPageDirectory = (uint32 *)KERN_PAGE_DIRECTORY;
uint8 gPhysicalPageFrameBitmap[RAM_MAXPAGE / 8];
uint8 gKernelPageHeapBitmap[RAM_MAXPAGE / 8];

static int gTotalPageCount = 0;

static void handlePageFault(Registers *regs);
static void syncPageDirectoriesKernelMemory();

void initializeMemory(uint32 high_mem)
{
    int pg;
    unsigned long i;

    registerInterruptHandler(14, handlePageFault);

    gTotalPageCount = (high_mem * 1024) / PAGESIZE_4M;

    for (pg = 0; pg < gTotalPageCount / 8; ++pg)
    {
        gPhysicalPageFrameBitmap[pg] = 0;
    }

    for (pg = gTotalPageCount / 8; pg < RAM_MAXPAGE / 8; ++pg)
    {
        gPhysicalPageFrameBitmap[pg] = 0xFF;
    }

    //Pages reserved for the kernel
    for (pg = PAGE_INDEX_4M(0x0); pg < (int)(PAGE_INDEX_4M(RESERVED_AREA)); ++pg)
    {
        SET_PAGEFRAME_USED(pg);
    }

    //Heap pages reserved
    for (pg = 0; pg < RAM_MAXPAGE / 8; ++pg)
    {
        gKernelPageHeapBitmap[pg] = 0xFF;
    }

    for (pg = PAGE_INDEX_4K(KERN_PD_AREA_BEGIN); pg < (int)(PAGE_INDEX_4K(KERN_PD_AREA_END)); ++pg)
    {
        SET_PAGEHEAP_UNUSED(pg * PAGESIZE_4K);
    }

    //Identity map
    for (i = 0; i < 4; ++i)
    {
        gKernelPageDirectory[i] = (i * PAGESIZE_4M | (PG_PRESENT | PG_WRITE | PG_4MB));//add PG_USER for accesing kernel code in user mode
    }

    for (i = 4; i < 1024; ++i)
    {
        gKernelPageDirectory[i] = 0;
    }

    //Enable paging
    asm("	mov %0, %%eax \n \
        mov %%eax, %%cr3 \n \
        mov %%cr4, %%eax \n \
        or %2, %%eax \n \
        mov %%eax, %%cr4 \n \
        mov %%cr0, %%eax \n \
        or %1, %%eax \n \
        mov %%eax, %%cr0"::"m"(gKernelPageDirectory), "i"(PAGING_FLAG), "i"(PSE_FLAG));

    initializeKernelHeap();
}

char* getPageFrame4M()
{
    int byte, bit;
    uint32 page = -1;

    for (byte = 0; byte < RAM_MAXPAGE / 8; byte++)
    {
        if (gPhysicalPageFrameBitmap[byte] != 0xFF)
        {
            for (bit = 0; bit < 8; bit++)
            {
                if (!(gPhysicalPageFrameBitmap[byte] & (1 << bit)))
                {
                    page = 8 * byte + bit;
                    SET_PAGEFRAME_USED(page);
                    Debug_PrintF("DEBUG: got 4M on physical %x\n", page * PAGESIZE_4M);
                    return (char *) (page * PAGESIZE_4M);
                }
            }
        }
    }

    PANIC("Memory is full!");
    return (char *) -1;
}

void releasePageFrame4M(uint32 p_addr)
{
    Debug_PrintF("DEBUG: released 4M on physical %x\n", p_addr);

    SET_PAGEFRAME_UNUSED(p_addr);
}

uint32* getPdFromReservedArea4K()
{
    int byte, bit;
    int page = -1;

    //Screen_PrintF("DEBUG: getPdFromReservedArea4K() begin\n");

    for (byte = 0; byte < RAM_MAXPAGE / 8; byte++)
    {
        if (gKernelPageHeapBitmap[byte] != 0xFF)
        {
            for (bit = 0; bit < 8; bit++)
            {
                if (!(gKernelPageHeapBitmap[byte] & (1 << bit)))
                {
                    page = 8 * byte + bit;
                    SET_PAGEHEAP_USED(page);
                    //Screen_PrintF("DEBUG: getPdFromReservedArea4K() found pageIndex:%d\n", page);
                    return (uint32 *) (page * PAGESIZE_4K);
                }
            }
        }
    }

    PANIC("Reserved Page Directory Area is Full!!!");
    return (uint32 *) -1;
}

void releasePdFromReservedArea4K(uint32 *v_addr)
{
    SET_PAGEHEAP_UNUSED(v_addr);
}

uint32 *createPd()
{
    int i;

    uint32* pd = getPdFromReservedArea4K();


    for (i = 0; i < KERNELMEMORY_PAGE_COUNT; ++i)
    {
        pd[i] = gKernelPageDirectory[i];
    }


    for (i = KERNELMEMORY_PAGE_COUNT; i < 1024; ++i)
    {
        pd[i] = 0;
    }

    return pd;
}

void destroyPd(uint32 *pd)
{
    for (int i = KERNELMEMORY_PAGE_COUNT; i < 1024; ++i)
    {
        uint32 p_addr = pd[i] & 0xFFC00000;

        if (p_addr)
        {
            releasePageFrame4M(p_addr);
        }

        pd[i] = 0;
    }

    releasePdFromReservedArea4K(pd);
}

uint32 *copyPd(uint32* pd)
{
    int i;

    uint32* newPd = getPdFromReservedArea4K();


    for (i = 0; i < KERNELMEMORY_PAGE_COUNT; ++i)
    {
        newPd[i] = gKernelPageDirectory[i];
    }

    disablePaging();

    for (i = KERNELMEMORY_PAGE_COUNT; i < 1024; ++i)
    {
        newPd[i] = 0;

        if ((pd[i] & PG_PRESENT) == PG_PRESENT)
        {
            uint32 pagePyhsical = pd[i] & 0xFFC00000;
            char* newPagePhysical = getPageFrame4M();

            memcpy((uint8*)newPagePhysical, (uint8*)pagePyhsical, PAGESIZE_4M);

            uint32 vAddr =  (i * 4) << 20;

            //Screen_PrintF("Copied page virtual %x\n", vAddr);

            addPageToPd(newPd, (char*)vAddr, newPagePhysical, PG_USER);
        }
    }

    enablePaging();

    return newPd;
}

//When calling this function:
//If it is intended to alloc kernel memory, v_addr must be < KERN_HEAP_END.
//If it is intended to alloc user memory, v_addr must be > KERN_HEAP_END.
BOOL addPageToPd(uint32* pd, char *v_addr, char *p_addr, int flags)
{
    uint32 *pde = NULL;

    //Screen_PrintF("DEBUG: addPageToPd(): v_addr:%x p_addr:%x flags:%x\n", v_addr, p_addr, flags);


    int index = (((uint32) v_addr & 0xFFC00000) >> 22);
    pde = pd + index;
    if ((*pde & PG_PRESENT) == PG_PRESENT)
    {
        //Already assigned!
        Debug_PrintF("ERROR: addPageToPd(): pde:%x is already assigned!!\n", pde);
        return FALSE;
    }

    //Screen_PrintF("addPageToPd(): index:%d pde:%x\n", index, pde);

    *pde = ((uint32) p_addr) | (PG_PRESENT | PG_4MB | PG_WRITE | flags);
    //Screen_PrintF("pde:%x *pde:%x\n", pde, *pde);

    SET_PAGEFRAME_USED(PAGE_INDEX_4M((uint32)p_addr));

    asm("invlpg %0"::"m"(v_addr));

    if (v_addr <= (char*)(KERN_HEAP_END - PAGESIZE_4M))
    {
        if (pd == gKernelPageDirectory)
        {
            syncPageDirectoriesKernelMemory();
        }
        else
        {
            PANIC("Attemped to allocate kernel memory to a page directory which is not the kernel page directory!!!\n");
        }
    }
    else
    {
        if (pd == gKernelPageDirectory)
        {
            //No panic here. Because we allow kernel to map anywhere!
        }
    }

    return TRUE;
}

BOOL removePageFromPd(uint32* pd, char *v_addr)
{
    int index = (((uint32) v_addr & 0xFFC00000) >> 20) / 4;
    uint32* pde = pd + index;
    if ((*pde & PG_PRESENT) == PG_PRESENT)
    {
        uint32 p_addr = *pde & 0xFFC00000;

        releasePageFrame4M(p_addr);

        *pde = 0;

        asm("invlpg %0"::"m"(v_addr));

        if (v_addr <= (char*)(KERN_HEAP_END - PAGESIZE_4M))
        {
            if (pd == gKernelPageDirectory)
            {
                syncPageDirectoriesKernelMemory();
            }
            else
            {
                PANIC("Attemped to allocate kernel memory to a page directory which is not the kernel page directory!!!\n");
            }
        }
        else
        {
            if (pd == gKernelPageDirectory)
            {
                PANIC("Attemped to allocate user memory to the kernel page directory!!!\n");
            }
        }

        return TRUE;
    }

    return FALSE;
}

static void syncPageDirectoriesKernelMemory()
{
    //get page directory list
    //it can be easier to traverse proccesses(and access its pd) here
    for (int byte = 0; byte < RAM_MAXPAGE / 8; byte++)
    {
        if (gKernelPageHeapBitmap[byte] != 0xFF)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                if ((gKernelPageHeapBitmap[byte] & (1 << bit)))
                {
                    int page = 8 * byte + bit;

                    uint32* pd = (uint32*)(page * PAGESIZE_4K);

                    for (int i = 0; i < KERNELMEMORY_PAGE_COUNT; ++i)
                    {
                        pd[i] = gKernelPageDirectory[i];
                    }
                }
            }
        }
    }
}

uint32 getTotalPageCount()
{
    return gTotalPageCount;
}

uint32 getUsedPageCount()
{
    int count = 0;
    for (int i = 0; i < gTotalPageCount; ++i)
    {
        if(IS_PAGEFRAME_USED(i))
        {
            ++count;
        }
    }

    return count;
}

uint32 getFreePageCount()
{
    return gTotalPageCount - getUsedPageCount();
}

static void printPageFaultInfo(uint32 faultingAddress, Registers *regs)
{
    int present = regs->errorCode & 0x1;
    int rw = regs->errorCode & 0x2;
    int us = regs->errorCode & 0x4;
    int reserved = regs->errorCode & 0x8;
    int id = regs->errorCode & 0x10;

    Debug_PrintF("Page fault!!! When trying to %s %x - IP:%x\n", rw ? "write to" : "read from", faultingAddress, regs->eip);
    Debug_PrintF("The page was %s\n", present ? "present" : "not present");

    if (reserved)
    {
        Debug_PrintF("Reserved bit was set\n");
    }

    if (id)
    {
        Debug_PrintF("Caused by an instruction fetch\n");
    }

    Debug_PrintF("CPU was in %s\n", us ? "user-mode" : "supervisor mode");
}

static void handlePageFault(Registers *regs)
{
    // A page fault has occurred.

    // The faulting address is stored in the CR2 register.
    uint32 faultingAddress;
    asm volatile("mov %%cr2, %0" : "=r" (faultingAddress));

    //Debug_PrintF("page_fault()\n");
    //Debug_PrintF("stack of handler is %x\n", &faultingAddress);

    Thread* faultingThread = getCurrentThread();
    if (NULL != faultingThread)
    {
        Thread* mainThread = getMainKernelThread();

        if (mainThread == faultingThread)
        {
            printPageFaultInfo(faultingAddress, regs);

            PANIC("Page fault in Kernel main thread!!!");
        }
        else
        {
            printPageFaultInfo(faultingAddress, regs);

            Debug_PrintF("Faulting thread is %d\n", faultingThread->threadId);

            if (faultingThread->userMode)
            {
                Debug_PrintF("Destroying process %d\n", faultingThread->owner->pid);

                destroyProcess(faultingThread->owner);
            }
            else
            {
                Debug_PrintF("Destroying kernel thread %d\n", faultingThread->threadId);

                destroyThread(faultingThread);
            }

            waitForSchedule();
        }
    }
    else
    {
        printPageFaultInfo(faultingAddress, regs);

        PANIC("Page fault!!!");
    }
}
