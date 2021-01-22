#ifndef VMM_H
#define VMM_H

#include "common.h"

typedef struct Process Process;
typedef struct List List;

extern uint32_t *g_kernel_page_directory;


#define SET_PAGEFRAME_USED(bitmap, page_index)	bitmap[((uint32_t) page_index)/8] |= (1 << (((uint32_t) page_index)%8))
#define SET_PAGEFRAME_UNUSED(bitmap, p_addr)	bitmap[((uint32_t) p_addr/PAGESIZE_4K)/8] &= ~(1 << (((uint32_t) p_addr/PAGESIZE_4K)%8))
#define IS_PAGEFRAME_USED(bitmap, page_index)	(bitmap[((uint32_t) page_index)/8] & (1 << (((uint32_t) page_index)%8)))

#define CHANGE_PD(pd) asm("mov %0, %%eax ;mov %%eax, %%cr3":: "m"(pd))
#define INVALIDATE(v_addr) asm("invlpg %0"::"m"(v_addr))

uint32_t vmm_acquire_page_frame_4k();
void vmm_release_page_frame_4k(uint32_t p_addr);

void vmm_initialize(uint32_t high_mem);

uint32_t *vmm_acquire_page_directory();
void vmm_destroy_page_directory_with_memory(uint32_t physical_pd);

BOOL vmm_add_page_to_pd(char *v_addr, uint32_t p_addr, int flags);
BOOL vmm_remove_page_from_pd(char *v_addr);

void enable_paging();
void disable_paging();

uint32_t vmm_get_total_page_count();
uint32_t vmm_get_used_page_count();
uint32_t vmm_get_free_page_count();

void vmm_initialize_process_pages(Process* process);
void* vmm_map_memory(Process* process, uint32_t v_address_search_start, uint32_t* p_address_array, uint32_t page_count, BOOL own);
BOOL vmm_unmap_memory(Process* process, uint32_t v_address, uint32_t page_count);

#endif // VMM_H
