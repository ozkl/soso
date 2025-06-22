#include "elf.h"
#include "common.h"
#include "alloc.h"
#include "vmm.h"

BOOL elf_is_valid(const char *elf_data)
{
    Elf32_Ehdr *hdr = (Elf32_Ehdr *) elf_data;

    if (hdr->e_ident[0] == 0x7f && hdr->e_ident[1] == 'E' &&
        hdr->e_ident[2] == 'L' && hdr->e_ident[3] == 'F')
    {
        return TRUE;
    }

    return FALSE;
}

static BOOL map_memory(Process * process, uint32_t v_address, uint32_t size)
{
    BOOL result = FALSE;

    uint32_t v_page_base = v_address & ~(PAGESIZE_4K - 1);
    uint32_t page_offset = v_address - v_page_base;
    uint32_t total_size = page_offset + size;
    uint32_t page_count = (total_size + PAGESIZE_4K - 1) / PAGESIZE_4K;
    
    uint32_t* p_address_array = kmalloc(sizeof(uint32_t) * page_count);
    for (uint32_t i = 0; i < page_count; ++i)
    {
        p_address_array[i] = vmm_acquire_page_frame_4k();
    }
    void* mapped = vmm_map_memory(process, (uint32_t)v_page_base, p_address_array, page_count, TRUE);
    if (NULL == mapped)
    {
        for (uint32_t i = 0; i < page_count; ++i)
        {
            vmm_release_page_frame_4k(p_address_array[i]);
        }
    }
    else
    {
        for (uint32_t i = 0; i < page_count; ++i)
        {
            uint32_t v_page_address = v_page_base + i * PAGESIZE_4K;
            INVALIDATE(v_page_address);
        }
        result = TRUE;
    }

    kfree(p_address_array);

    return result;
}

uint32_t elf_map_load(Process * process, const char *elf_data)
{
    uint32_t v_begin, v_end;
    Elf32_Ehdr *hdr;
    Elf32_Phdr *p_entry;
    Elf32_Scdr *s_entry;

    hdr = (Elf32_Ehdr *) elf_data;
    p_entry = (Elf32_Phdr *) (elf_data + hdr->e_phoff);

    s_entry = (Elf32_Scdr*) (elf_data + hdr->e_shoff);

    if (elf_is_valid(elf_data)==FALSE)
    {
        return 0;
    }

    //printkf("Elf Load - Entry: %x\n", hdr->e_entry);

    for (int pe = 0; pe < hdr->e_phnum; pe++, p_entry++)
    {
        //Read each entry

        if (p_entry->p_type == PT_LOAD)
        {
            v_begin = p_entry->p_vaddr;
            v_end = p_entry->p_vaddr + p_entry->p_memsz;
            

            if (v_end > USER_STACK)
            {
                //printkf("INFO: elf_load(): can't load executable above %x. Yours: %x\n", USER_STACK, v_end);
                //return 0;

                printkf("Warning: skipped to load %d(%x) bytes to %x\n", p_entry->p_filesz, p_entry->p_filesz, v_begin);
                continue;
            }

            //printkf("ELF: entry flags: %x (%d)\n", p_entry->p_flags, p_entry->p_flags);

            const char * entry_data = elf_data + p_entry->p_offset;
            uint32_t size = p_entry->p_filesz;
            if (p_entry->p_memsz > size)
            {
                //map for bigger size if mem bigger than file
                size = p_entry->p_memsz;
            }
            BOOL success = map_memory(process, v_begin, size);
            if (success)
            {
                memcpy((uint8_t*)v_begin, (uint8_t*)entry_data, p_entry->p_filesz);
                if (p_entry->p_memsz > p_entry->p_filesz)
                {
                    char* p = (char *)v_begin;
                    for (uint32_t i = p_entry->p_filesz; i < p_entry->p_memsz; i++)
                    {
                        p[i] = 0;
                    }
                }
            }

            //TODO: if not success
        }
    }

    //entry point
    return hdr->e_entry;
}

uint32_t elf_compute_phdr_runtime(const char *elf_data)
{
    uint32_t at_phdr = 0;

    Elf32_Ehdr * hdr = (Elf32_Ehdr *) elf_data;
    Elf32_Phdr * p_entry = (Elf32_Phdr *) (elf_data + hdr->e_phoff);


    if (elf_is_valid(elf_data)==FALSE)
    {
        return 0;
    }

    //printkf("Elf Load - Entry: %x\n", hdr->e_entry);

    for (int pe = 0; pe < hdr->e_phnum; pe++, p_entry++)
    {
        if (p_entry->p_type == PT_LOAD &&
                hdr->e_phoff >= p_entry->p_offset &&
                hdr->e_phoff < p_entry->p_offset + p_entry->p_filesz) {

                at_phdr = p_entry->p_vaddr + (hdr->e_phoff - p_entry->p_offset);
                break;
            }
    }

    return at_phdr;
}

uint32_t elf_get_begin_in_memory(const char *elf_data)
{
    uint32_t v_begin;
    Elf32_Ehdr *hdr;
    Elf32_Phdr *p_entry;

    hdr = (Elf32_Ehdr *) elf_data;
    p_entry = (Elf32_Phdr *) (elf_data + hdr->e_phoff);

    if (elf_is_valid(elf_data) == FALSE)
    {
        return 0;
    }

    uint32_t result = 0xFFFFFFFF;

    for (int pe = 0; pe < hdr->e_phnum; pe++, p_entry++)
    {
        //Read each entry

        if (p_entry->p_type == PT_LOAD)
        {
            v_begin = p_entry->p_vaddr;

            if (v_begin < result)
            {
                result = v_begin;
            }
        }
    }

    return result;
}

uint32_t elf_get_end_in_memory(const char *elf_data)
{
    uint32_t v_end;
    Elf32_Ehdr *hdr;
    Elf32_Phdr *p_entry;

    hdr = (Elf32_Ehdr *) elf_data;
    p_entry = (Elf32_Phdr *) (elf_data + hdr->e_phoff);

    if (elf_is_valid(elf_data) == FALSE)
    {
        return 0;
    }

    uint32_t result = 0;

    for (int pe = 0; pe < hdr->e_phnum; pe++, p_entry++)
    {
        //Read each entry

        if (p_entry->p_type == PT_LOAD)
        {
            v_end = p_entry->p_vaddr + p_entry->p_memsz;

            if (v_end > result)
            {
                result = v_end;
            }
        }
    }

    return result;
}
