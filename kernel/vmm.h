#ifndef VMM_H
#define VMM_H

#include "common.h"

    extern uint32 *gKernelPageDirectory;
    extern uint8 gPhysicalPageFrameBitmap[];

    extern uint8 gKernelPageHeapBitmap[];


    #define SET_PAGEFRAME_USED(pageIndex)	gPhysicalPageFrameBitmap[((uint32) pageIndex)/8] |= (1 << (((uint32) pageIndex)%8))
    #define SET_PAGEFRAME_UNUSED(p_addr)	gPhysicalPageFrameBitmap[((uint32) p_addr/PAGESIZE_4M)/8] &= ~(1 << (((uint32) p_addr/PAGESIZE_4M)%8))
    #define IS_PAGEFRAME_USED(pageIndex)	(gPhysicalPageFrameBitmap[((uint32) pageIndex)/8] & (1 << (((uint32) pageIndex)%8)))

    #define SET_PAGEHEAP_USED(pageIndex)	gKernelPageHeapBitmap[((uint32) pageIndex)/8] |= (1 << (((uint32) pageIndex)%8))
    #define SET_PAGEHEAP_UNUSED(p_addr)	gKernelPageHeapBitmap[((uint32) p_addr/PAGESIZE_4K)/8] &= ~(1 << (((uint32) p_addr/PAGESIZE_4K)%8))
    #define IS_PAGEHEAP_USED(pageIndex)	(gKernelPageHeapBitmap[((uint32) pageIndex)/8] & (1 << (((uint32) pageIndex)%8)))

    char* getPageFrame4M();
    void releasePageFrame4M(uint32 p_addr);

    uint32 *getPdFromReservedArea4K();
    void releasePdFromReservedArea4K(uint32 *);

    void initializeMemory(uint32 high_mem);

    uint32 *createPd();
    void destroyPd(uint32 *);
    uint32 *copyPd(uint32* pd);

    BOOL addPageToPd(uint32* pd, char *v_addr, char *p_addr, int flags);
    BOOL removePageFromPd(uint32* pd, char *v_addr, BOOL releasePageFrame);

    void enablePaging();
    void disablePaging();

    uint32 getTotalPageCount();
    uint32 getUsedPageCount();
    uint32 getFreePageCount();

#endif // VMM_H
