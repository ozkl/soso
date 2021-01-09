#include "vmm.h"
#include "common.h"
#include "alloc.h"
#include "isr.h"
#include "process.h"
#include "list.h"
#include "debugprint.h"
#include "serial.h"

uint32 *gKernelPageDirectory = (uint32 *)KERN_PAGE_DIRECTORY;
uint8 gPhysicalPageFrameBitmap[RAM_AS_4K_PAGES / 8];

static int gTotalPageCount = 0;

static void handlePageFault(Registers *regs);
static void syncPageDirectoriesKernelMemory();

void initializeMemory(uint32 high_mem)
{
    int pg;
    unsigned long i;

    registerInterruptHandler(14, handlePageFault);

    gTotalPageCount = (high_mem * 1024) / PAGESIZE_4K;

    //Mark all memory space available
    for (pg = 0; pg < gTotalPageCount / 8; ++pg)
    {
        gPhysicalPageFrameBitmap[pg] = 0;
    }

    //Mark physically unexistent memory as used or unavailable
    for (pg = gTotalPageCount / 8; pg < RAM_AS_4K_PAGES / 8; ++pg)
    {
        gPhysicalPageFrameBitmap[pg] = 0xFF;
    }

    //Mark pages reserved for the kernel as used
    for (pg = PAGE_INDEX_4K(0x0); pg < (int)(PAGE_INDEX_4K(RESERVED_AREA)); ++pg)
    {
        SET_PAGEFRAME_USED(gPhysicalPageFrameBitmap, pg);
    }

    //Identity map for first 16MB
    //First identity pages are 4MB sized for ease
    for (i = 0; i < 4; ++i)
    {
        gKernelPageDirectory[i] = (i * PAGESIZE_4M | (PG_PRESENT | PG_WRITE | PG_4MB));//add PG_USER for accesing kernel code in user mode
    }

    for (i = 4; i < 1024; ++i)
    {
        gKernelPageDirectory[i] = 0;
    }

    //Recursive page directory strategy
    gKernelPageDirectory[1023] = (uint32)gKernelPageDirectory | PG_PRESENT | PG_WRITE;

    //zero out PD area
    memset((uint8*)KERN_PD_AREA_BEGIN, 0, KERN_PD_AREA_END - KERN_PD_AREA_BEGIN);

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

uint32 acquirePageFrame4K()
{
    int byte, bit;
    uint32 page = -1;

    int pid = -1;
    Thread* thread = getCurrentThread();
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
        if (gPhysicalPageFrameBitmap[byte] != 0xFF)
        {
            for (bit = 0; bit < 8; bit++)
            {
                if (!(gPhysicalPageFrameBitmap[byte] & (1 << bit)))
                {
                    page = 8 * byte + bit;
                    SET_PAGEFRAME_USED(gPhysicalPageFrameBitmap, page);

                    //Debug_PrintF("DEBUG: Acquired 4K Physical %x (pid:%d)\n", page * PAGESIZE_4K, pid);
                    //Serial_PrintF("DEBUG: Acquired 4K Physical %x (pid:%d)\n", page * PAGESIZE_4K, pid);

                    return (page * PAGESIZE_4K);
                }
            }
        }
    }

    PANIC("Memory is full!");
    return (uint32)-1;
}

void releasePageFrame4K(uint32 p_addr)
{
    //Debug_PrintF("DEBUG: Released 4K Physical %x\n", p_addr);
    //Serial_PrintF("DEBUG: Released 4K Physical %x\n", p_addr);

    SET_PAGEFRAME_UNUSED(gPhysicalPageFrameBitmap, p_addr);
}

uint32* acquirePageDirectory()
{
    uint32 address = KERN_PD_AREA_BEGIN;
    for (; address < KERN_PD_AREA_END; address += PAGESIZE_4K)
    {
        uint32* pd = (uint32*)address;

        if (*pd == NULL)
        {
            //Found an unused page directory

            //Let's initialize it. First we should sync with first 1GB part with kernel page directory to achive the same view.

            for (int i = 0; i < KERNELMEMORY_PAGE_COUNT; ++i)
            {
                pd[i] = gKernelPageDirectory[i]& ~PG_OWNED;
            }

            for (int i = KERNELMEMORY_PAGE_COUNT; i < 1024; ++i)
            {
                pd[i] = 0;
            }

            pd[1023] = address | PG_PRESENT | PG_WRITE;

            return pd;
        }
    }

    return NULL;
}

void destroyPageDirectoryWithMemory(uint32 physicalPd)
{
    beginCriticalSection();

    uint32* pd = (uint32*)0xFFFFF000;

    uint32 cr3 = readCr3();

    CHANGE_PD(physicalPd);

    //this 1023 is very important
    //we must not touch pd[1023] since PD is mapped to itself. Otherwise we corrupt the whole system's memory.
    for (int pdIndex = KERNELMEMORY_PAGE_COUNT; pdIndex < 1023; ++pdIndex)
    {
        if ((pd[pdIndex] & PG_PRESENT) == PG_PRESENT)
        {
            uint32* pt = ((uint32*)0xFFC00000) + (0x400 * pdIndex);

            for (int ptIndex = 0; ptIndex < 1024; ++ptIndex)
            {
                if ((pt[ptIndex] & PG_PRESENT) == PG_PRESENT)
                {
                    if ((pt[ptIndex] & PG_OWNED) == PG_OWNED)
                    {
                        uint32 physicalFrame = pt[ptIndex] & ~0xFFF;

                        releasePageFrame4K(physicalFrame);
                    }
                }
                pt[ptIndex] = 0;
            }

            if ((pd[pdIndex] & PG_OWNED) == PG_OWNED)
            {
                uint32 physicalFramePT = pd[pdIndex] & ~0xFFF;
                releasePageFrame4K(physicalFramePT);
            }
        }

        pd[pdIndex] = 0;
    }

    endCriticalSection();
    //return to caller's Page Directory
    CHANGE_PD(cr3);
}

//When calling this function:
//If it is intended to alloc kernel memory, v_addr must be < KERN_HEAP_END.
//If it is intended to alloc user memory, v_addr must be > KERN_HEAP_END.
//Works for active Page Directory!
BOOL addPageToPd(char *v_addr, uint32 p_addr, int flags)
{
    // Both addresses are page-aligned.

    //Serial_PrintF("addPageToPd v_addr:%x p_addr:%x\n", v_addr, p_addr);

    int pdIndex = (((uint32) v_addr) >> 22);
    int ptIndex = (((uint32) v_addr) >> 12) & 0x03FF;

    uint32* pd = (uint32*)0xFFFFF000;

    uint32* pt = ((uint32*)0xFFC00000) + (0x400 * pdIndex);

    //Serial_PrintF("addPageToPd 1");
    if ((pd[pdIndex] & PG_PRESENT) != PG_PRESENT)
    {
        //Serial_PrintF("addPageToPd 2");
        uint32 tablePhysical = acquirePageFrame4K();

        //Serial_PrintF("addPageToPd 3");
        pd[pdIndex] = (tablePhysical) | (flags & 0xFFF) | (PG_PRESENT | PG_WRITE);

        //Serial_PrintF("addPageToPd 4");

        INVALIDATE(v_addr);

        //Serial_PrintF("addPageToPd 5");

        //Zero out table as it may contain thrash data from previously allocated page frame
        for (int i = 0; i < 1024; ++i)
        {
            pt[i] = 0;
        }
    }


    if ((pt[ptIndex] & PG_PRESENT) == PG_PRESENT)
    {
        //Serial_PrintF("addPageToPd 6");
        return FALSE;
    }

    pt[ptIndex] = (p_addr) | (flags & 0xFFF) | (PG_PRESENT | PG_WRITE);

    //Serial_PrintF("addPageToPd 7");

    INVALIDATE(v_addr);

    //Serial_PrintF("addPageToPd 8");

    if (v_addr < (char*)(KERN_HEAP_END))
    {
        //If this is the kernel page directory, sync others for first 1GB
        if ((pd[1023] & 0xFFFFF000) == (uint32)gKernelPageDirectory)
        {
            syncPageDirectoriesKernelMemory();
        }
    }

    return TRUE;
}

//Works for active Page Directory!
BOOL removePageFromPd(char *v_addr)
{
    int pdIndex = (((uint32) v_addr) >> 22);
    int ptIndex = (((uint32) v_addr) >> 12) & 0x03FF;

    uint32* pd = (uint32*)0xFFFFF000;


    if ((pd[pdIndex] & PG_PRESENT) == PG_PRESENT)
    {
        uint32* pt = ((uint32*)0xFFC00000) + (0x400 * pdIndex);

        uint32 physicalFrame = pt[ptIndex] & ~0xFFF;

        if ((pt[ptIndex] & PG_OWNED) == PG_OWNED)
        {
            releasePageFrame4K(physicalFrame);
        }

        pt[ptIndex] = 0;

        BOOL allUnmapped = TRUE;
        for (int i = 0; i < 1024; ++i)
        {
            if (pt[i] != 0)
            {
                allUnmapped = FALSE;
                break;
            }
        }

        if (allUnmapped)
        {
            //All page table entries are unmapped.
            //Lets destroy this page table and remove it from PD

            uint32 physicalFramePT = pd[pdIndex] & ~0xFFF;

            if ((pd[pdIndex] & PG_OWNED) == PG_OWNED)
            {
                pd[pdIndex] = 0;

                releasePageFrame4K(physicalFramePT);
            }
        }



        INVALIDATE(v_addr);

        if (v_addr < (char*)(KERN_HEAP_END))
        {
            //If this is the kernel page directory, sync others for first 1GB
            if ((pd[1023] & 0xFFFFF000) == (uint32)gKernelPageDirectory)
            {
                syncPageDirectoriesKernelMemory();
            }
        }

        return TRUE;
    }

    return FALSE;
}

static void syncPageDirectoriesKernelMemory()
{
    uint32 address = KERN_PD_AREA_BEGIN;
    for (; address < KERN_PD_AREA_END; address += PAGESIZE_4K)
    {
        uint32* pd = (uint32*)address;

        if (*pd != NULL)
        {
            //Found an in-use page directory

            for (int i = 0; i < KERNELMEMORY_PAGE_COUNT; ++i)
            {
                pd[i] = gKernelPageDirectory[i] & ~PG_OWNED;
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
        if(IS_PAGEFRAME_USED(gPhysicalPageFrameBitmap, i))
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

            Debug_PrintF("Faulting thread is %d and its state is %d\n", faultingThread->threadId, faultingThread->state);

            if (faultingThread->userMode)
            {
                if (faultingThread->state == TS_CRITICAL ||
                    faultingThread->state == TS_UNINTERRUPTIBLE)
                {
                    Debug_PrintF("CRITICAL!! process %d\n", faultingThread->owner->pid);

                    changeProcessState(faultingThread->owner, TS_SUSPEND);

                    changeThreadState(faultingThread, TS_DEAD, faultingAddress);
                }
                else
                {
                    //Debug_PrintF("Destroying process %d\n", faultingThread->owner->pid);

                    //destroyProcess(faultingThread->owner);

                    //TODO: state in RUN?

                    Debug_PrintF("Segmentation fault pid:%d\n", faultingThread->owner->pid);

                    signalThread(faultingThread, SIGSEGV);
                }
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

void initializeProcessPages(Process* process)
{
    int page = 0;

    for (page = 0; page < RAM_AS_4K_PAGES / 8; ++page)
    {
        process->mmappedVirtualMemory[page] = 0xFF;
    }

    for (page = PAGE_INDEX_4K(USER_OFFSET); page < (int)(PAGE_INDEX_4K(MEMORY_END)); ++page)
    {
        SET_PAGEFRAME_UNUSED(process->mmappedVirtualMemory, page * PAGESIZE_4K);
    }

    //Page Tables position marked as used. It is after MEMORY_END.
}

//if this fails (return NULL), the caller should clean up physical page frames
void* mapMemory(Process* process, uint32 vAddressSearchStart, uint32* pAddressArray, uint32 pageCount, BOOL own)
{
    int pageIndex = 0;

    if (NULL == pAddressArray || pageCount == 0)
    {
        return NULL;
    }

    uint32 foundAdjacent = 0;

    uint32 vMem = 0;

    for (pageIndex = PAGE_INDEX_4K(vAddressSearchStart); pageIndex < (int)(PAGE_INDEX_4K(MEMORY_END)); ++pageIndex)
    {
        if (IS_PAGEFRAME_USED(process->mmappedVirtualMemory, pageIndex))
        {
            foundAdjacent = 0;
            vMem = 0;
        }
        else
        {
            if (0 == foundAdjacent)
            {
                vMem = pageIndex * PAGESIZE_4K;
            }
            ++foundAdjacent;
        }

        if (foundAdjacent == pageCount)
        {
            break;
        }
    }

    //Debug_PrintF("mapMemory: needed:%d foundAdjacent:%d vMem:%x\n", neededPages, foundAdjacent, vMem);

    if (foundAdjacent == pageCount)
    {
        int ownFlag = 0;
        if (own)
        {
            ownFlag = PG_OWNED;
        }

        uint32 v = vMem;
        for (uint32 i = 0; i < pageCount; ++i)
        {
            uint32 p = pAddressArray[i];
            p = p & 0xFFFFF000;

            addPageToPd((char*)v, p, PG_USER | ownFlag);

            //Debug_PrintF("MMAPPED: %s(%d) virtual:%x -> physical:%x owned:%d\n", process->name, process->pid, v, p, own);

            SET_PAGEFRAME_USED(process->mmappedVirtualMemory, PAGE_INDEX_4K(v));

            v += PAGESIZE_4K;
        }

        return (void*)vMem;
    }

    return NULL;
}

BOOL unmapMemory(Process* process, uint32 vAddress, uint32 pageCount)
{
    if (pageCount == 0)
    {
        return FALSE;
    }

    uint32 pageIndex = 0;

    uint32 neededPages = pageCount;

    uint32 old = vAddress;
    vAddress &= 0xFFFFF000;

    printkf("pageFrame dealloc from munmap:%x aligned:%x\n", old, vAddress);

    uint32 startIndex = PAGE_INDEX_4K(vAddress);
    uint32 endIndex = startIndex + neededPages;

    BOOL result = FALSE;

    for (pageIndex = startIndex; pageIndex < endIndex; ++pageIndex)
    {
        if (IS_PAGEFRAME_USED(process->mmappedVirtualMemory, pageIndex))
        {
            char* vAddr = (char*)(pageIndex * PAGESIZE_4K);

            removePageFromPd(vAddr);

            Debug_PrintF("UNMAPPED: %s(%d) virtual:%x\n", process->name, process->pid, vAddr);

            SET_PAGEFRAME_UNUSED(process->mmappedVirtualMemory, vAddr);

            result = TRUE;
        }
    }

    return result;
}
