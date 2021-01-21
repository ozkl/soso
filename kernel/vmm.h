#ifndef VMM_H
#define VMM_H

#include "common.h"

typedef struct Process Process;
typedef struct List List;

extern uint32 *g_kernel_page_directory;


#define SET_PAGEFRAME_USED(bitmap, page_index)	bitmap[((uint32) page_index)/8] |= (1 << (((uint32) page_index)%8))
#define SET_PAGEFRAME_UNUSED(bitmap, p_addr)	bitmap[((uint32) p_addr/PAGESIZE_4K)/8] &= ~(1 << (((uint32) p_addr/PAGESIZE_4K)%8))
#define IS_PAGEFRAME_USED(bitmap, page_index)	(bitmap[((uint32) page_index)/8] & (1 << (((uint32) page_index)%8)))

#define CHANGE_PD(pd) asm("mov %0, %%eax ;mov %%eax, %%cr3":: "m"(pd))
#define INVALIDATE(v_addr) asm("invlpg %0"::"m"(v_addr))

uint32 vmm_acquire_page_frame_4k();
void vmm_release_page_frame_4k(uint32 p_addr);

void vmm_initialize(uint32 high_mem);

uint32 *vmm_acquire_page_directory();
void vmm_destroy_page_directory_with_memory(uint32 physical_pd);

BOOL vmm_add_page_to_pd(char *v_addr, uint32 p_addr, int flags);
BOOL vmm_remove_page_from_pd(char *v_addr);

void enable_paging();
void disable_paging();

uint32 vmm_get_total_page_count();
uint32 vmm_get_used_page_count();
uint32 vmm_get_free_page_count();

void vmm_initialize_process_pages(Process* process);
void* vmm_map_memory(Process* process, uint32 v_address_search_start, uint32* p_address_array, uint32 page_count, BOOL own);
BOOL vmm_unmap_memory(Process* process, uint32 v_address, uint32 page_count);

#endif // VMM_H
