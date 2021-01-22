#include "elf.h"
#include "common.h"
#include "process.h"

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

uint32_t elf_load(const char *elf_data)
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

    for (int pe = 0; pe < hdr->e_phnum; pe++, p_entry++)
    {
        //Read each entry

        if (p_entry->p_type == PT_LOAD)
        {
            v_begin = p_entry->p_vaddr;
            v_end = p_entry->p_vaddr + p_entry->p_memsz;
            if (v_begin < USER_OFFSET)
            {
                //printkf("INFO: elf_load(): can't load executable below %x. Yours: %x\n", USER_OFFSET, v_begin);
                //return 0;
                printkf("Warning: skipped to load %d(%x) bytes to %x\n", p_entry->p_filesz, p_entry->p_filesz, v_begin);
                continue;
            }

            if (v_end > USER_STACK)
            {
                //printkf("INFO: elf_load(): can't load executable above %x. Yours: %x\n", USER_STACK, v_end);
                //return 0;

                printkf("Warning: skipped to load %d(%x) bytes to %x\n", p_entry->p_filesz, p_entry->p_filesz, v_begin);
                continue;
            }

            //printkf("ELF: entry flags: %x (%d)\n", p_entry->p_flags, p_entry->p_flags);


            memcpy((uint8_t *) v_begin, (uint8_t *) (elf_data + p_entry->p_offset), p_entry->p_filesz);
            if (p_entry->p_memsz > p_entry->p_filesz)
            {
                char* p = (char *) p_entry->p_vaddr;
                for (int i = p_entry->p_filesz; i < (int)(p_entry->p_memsz); i++)
                {
                    p[i] = 0;
                }
            }
        }
    }

    //entry point
    return hdr->e_entry;
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
