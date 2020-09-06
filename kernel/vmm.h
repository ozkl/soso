#ifndef VMM_H
#define VMM_H

#include "common.h"

typedef struct Process Process;
typedef struct List List;

extern uint32 *gKernelPageDirectory;


#define SET_PAGEFRAME_USED(bitmap, pageIndex)	bitmap[((uint32) pageIndex)/8] |= (1 << (((uint32) pageIndex)%8))
#define SET_PAGEFRAME_UNUSED(bitmap, p_addr)	bitmap[((uint32) p_addr/PAGESIZE_4K)/8] &= ~(1 << (((uint32) p_addr/PAGESIZE_4K)%8))
#define IS_PAGEFRAME_USED(bitmap, pageIndex)	(bitmap[((uint32) pageIndex)/8] & (1 << (((uint32) pageIndex)%8)))

#define CHANGE_PD(pd) asm("mov %0, %%eax ;mov %%eax, %%cr3":: "m"(pd))
#define INVALIDATE(v_addr) asm("invlpg %0"::"m"(v_addr))

uint32 acquirePageFrame4K();
void releasePageFrame4K(uint32 p_addr);

void initializeMemory(uint32 high_mem);

uint32 *acquirePageDirectory();
void destroyPageDirectoryWithMemory(uint32 physicalPd);

BOOL addPageToPd(char *v_addr, uint32 p_addr, int flags);
BOOL removePageFromPd(char *v_addr);

void enablePaging();
void disablePaging();

uint32 getTotalPageCount();
uint32 getUsedPageCount();
uint32 getFreePageCount();

void initializeProcessPages(Process* process);
void* mapMemory(Process* process, uint32 vAddressSearchStart, uint32* pAddressArray, uint32 pageCount, BOOL own);
BOOL unmapMemory(Process* process, uint32 vAddress, uint32 pageCount);

#endif // VMM_H
