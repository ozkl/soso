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

BOOL elf_is_static(const char *elf_data)
{
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *) elf_data;
    const Elf32_Phdr *phdr = (const Elf32_Phdr *)(elf_data + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type == PT_DYNAMIC)
        {
            return FALSE;  // Dynamic section present -> dynamically linked
        }
    }

    return TRUE;  // No PT_DYNAMIC -> statically linked
}

BOOL elf_is_elf32_x86(const char *elf_data)
{
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *) elf_data;

    // Check ELF class is 32-bit
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
    {
        return FALSE;
    }

    // Check data encoding (little endian for i386)
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
    {
        return FALSE;
    }

    // Check machine type is x86
    if (ehdr->e_machine != EM_386)
    {
        return FALSE;
    }

    // Check it's a known type (optional: EXEC, DYN, REL)
    if (ehdr->e_type != ET_EXEC &&
        ehdr->e_type != ET_DYN &&
        ehdr->e_type != ET_REL)
    {
        return FALSE;
    }

    return TRUE;
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

    if (elf_is_valid(elf_data) == FALSE)
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
            BOOL success = vmm_map_memory_simple(process, v_begin, size);
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
