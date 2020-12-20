#include "common.h"
#include "screen.h"
#include "alloc.h"
#include "vmm.h"
#include "process.h"
#include "debugprint.h"
#include "serial.h"

#define KMALLOC_MINSIZE		16

extern uint32 *gKernelPageDirectory;

static char *gKernelHeap = NULL;
static uint32 gKernelHeapUsed = 0;


void initializeKernelHeap()
{
    gKernelHeap = (char *) KERN_HEAP_BEGIN;

    ksbrkPage(1);
}

void *ksbrkPage(int n)
{
    struct MallocHeader *chunk;
    uint32 p_addr;
    int i;

    if ((gKernelHeap + (n * PAGESIZE_4K)) > (char *) KERN_HEAP_END) {
        //Screen_PrintF("ERROR: ksbrk(): no virtual memory left for kernel heap !\n");
        return (char *) -1;
    }

    chunk = (struct MallocHeader *) gKernelHeap;

    for (i = 0; i < n; i++)
    {
        p_addr = acquirePageFrame4K();

        if ((int)(p_addr) < 0)
        {
            PANIC("PANIC: ksbrkPage(): no free page frame available !");
            return (char *) -1;
        }

        addPageToPd(gKernelHeap, p_addr, 0); //add PG_USER to allow user programs to read kernel heap

        gKernelHeap += PAGESIZE_4K;
    }

    chunk->size = PAGESIZE_4K * n;
    chunk->used = 0;

    return chunk;
}

void *kmalloc(uint32 size)
{
    if (size==0)
        return 0;

    unsigned long realsize;
    struct MallocHeader *chunk, *other;

    if ((realsize = sizeof(struct MallocHeader) + size) < KMALLOC_MINSIZE)
    {
        realsize = KMALLOC_MINSIZE;
    }

    chunk = (struct MallocHeader *) KERN_HEAP_BEGIN;
    while (chunk->used || chunk->size < realsize)
    {
        if (chunk->size == 0)
        {
            printkf("\nPANIC: kmalloc(): corrupted chunk on %x with null size (heap %x) !\nSystem halted\n", chunk, gKernelHeap);

            PANIC("kmalloc()");

            return 0;
        }

        chunk = (struct MallocHeader *)((char *)chunk + chunk->size);

        if (chunk == (struct MallocHeader *) gKernelHeap)
        {
            if ((int)(ksbrkPage((realsize / PAGESIZE_4K) + 1)) < 0)
            {
                PANIC("kmalloc(): no memory left for kernel !\nSystem halted\n");

                return 0;
            }
        }
        else if (chunk > (struct MallocHeader *) gKernelHeap)
        {
            printkf("\nPANIC: kmalloc(): chunk on %x while heap limit is on %x !\nSystem halted\n", chunk, gKernelHeap);

            PANIC("kmalloc()");

            return 0;
        }
    }


    if (chunk->size - realsize < KMALLOC_MINSIZE)
    {
        chunk->used = 1;
    }
    else
    {
        other = (struct MallocHeader *)((char *) chunk + realsize);
        other->size = chunk->size - realsize;
        other->used = 0;

        chunk->size = realsize;
        chunk->used = 1;
    }

    gKernelHeapUsed += realsize;

    return (char *) chunk + sizeof(struct MallocHeader);
}

void kfree(void *v_addr)
{
    if (v_addr==(void*)0)
        return;

    struct MallocHeader *chunk, *other;

    chunk = (struct MallocHeader *)((uint32)v_addr - sizeof(struct MallocHeader));
    chunk->used = 0;

    gKernelHeapUsed -= chunk->size;

    //Merge free block with next free block
    while ((other = (struct MallocHeader *)((char *)chunk + chunk->size))
           && other < (struct MallocHeader *)gKernelHeap
           && other->used == 0)
    {
        chunk->size += other->size;
    }
}

static void sbrkPage(Process* process, int pageCount)
{
    if (pageCount > 0)
    {
        for (int i = 0; i < pageCount; ++i)
        {
            if ((process->brkNextUnallocatedPageBegin + PAGESIZE_4K) > (char*)(MEMORY_END - PAGESIZE_4K))
            {
                return;
            }

            uint32 p_addr = acquirePageFrame4K();

            if ((int)(p_addr) < 0)
            {
                //PANIC("sbrkPage(): no free page frame available !");
                return;
            }

            addPageToPd(process->brkNextUnallocatedPageBegin, p_addr, PG_USER | PG_OWNED);

            SET_PAGEFRAME_USED(process->mmappedVirtualMemory, PAGE_INDEX_4K((uint32)process->brkNextUnallocatedPageBegin));

            process->brkNextUnallocatedPageBegin += PAGESIZE_4K;
        }
    }
    else if (pageCount < 0)
    {
        pageCount *= -1;

        for (int i = 0; i < pageCount; ++i)
        {
            if (process->brkNextUnallocatedPageBegin - PAGESIZE_4K >= process->brkBegin)
            {
                process->brkNextUnallocatedPageBegin -= PAGESIZE_4K;

                //This also releases the page frame
                removePageFromPd(process->brkNextUnallocatedPageBegin);

                SET_PAGEFRAME_UNUSED(process->mmappedVirtualMemory, (uint32)process->brkNextUnallocatedPageBegin);
            }
        }
    }
}

void initializeProgramBreak(Process* process, uint32 size)
{
    process->brkBegin = (char*) USER_OFFSET;
    process->brkEnd = process->brkBegin;
    process->brkNextUnallocatedPageBegin = process->brkBegin;

    //Userland programs (their code, data,..) start from USER_OFFSET
    //Lets allocate some space for them by moving program break.

    sbrk(process, size);
}

void *sbrk(Process* process, int nBytes)
{
    //Screen_PrintF("sbrk:1: pid:%d nBytes:%d\n", process->pid, nBytes);

    char* previousBreak = process->brkEnd;

    if (nBytes > 0)
    {
        int remainingInThePage = process->brkNextUnallocatedPageBegin - process->brkEnd;

        //Screen_PrintF("sbrk:2: remainingInThePage:%d\n", remainingInThePage);

        if (nBytes > remainingInThePage)
        {
            int bytesNeededInNewPages = nBytes - remainingInThePage;
            int neededNewPageCount = ((bytesNeededInNewPages-1) / PAGESIZE_4K) + 1;

            //Screen_PrintF("sbrk:3: neededNewPageCount:%d\n", neededNewPageCount);

            uint32 freePages = getFreePageCount();
            if ((uint32)neededNewPageCount + 1 > freePages)
            {
                return (void*)-1;
            }

            sbrkPage(process, neededNewPageCount);
        }
    }
    else if (nBytes < 0)
    {
        char* currentPageBegin = process->brkNextUnallocatedPageBegin - PAGESIZE_4K;

        int remainingInThePage = process->brkEnd - currentPageBegin;

        //Screen_PrintF("sbrk:4: remainingInThePage:%d\n", remainingInThePage);

        if (-nBytes > remainingInThePage)
        {
            int bytesInPreviousPages = -nBytes - remainingInThePage;
            int neededNewPageCount = ((bytesInPreviousPages-1) / PAGESIZE_4K) + 1;

            //Screen_PrintF("sbrk:5: neededNewPageCount:%d\n", neededNewPageCount);

            sbrkPage(process, -neededNewPageCount);
        }
    }

    process->brkEnd += nBytes;

    return previousBreak;
}

uint32 getKernelHeapUsed()
{
    return gKernelHeapUsed;
}
