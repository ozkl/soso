#ifndef ELF_H
#define ELF_H

#include "common.h"

/*
 * ELF HEADER
 */
typedef struct {
    unsigned char e_ident[16];	/* ELF identification */
    uint16_t e_type;		/* 2 (exec file) */
    uint16_t e_machine;		/* 3 (intel architecture) */
    uint32_t e_version;		/* 1 */
    uint32_t e_entry;		/* starting point */
    uint32_t e_phoff;		/* program header table offset */
    uint32_t e_shoff;		/* section header table offset */
    uint32_t e_flags;		/* various flags */
    uint16_t e_ehsize;		/* ELF header (this) size */

    uint16_t e_phentsize;	/* program header table entry size */
    uint16_t e_phnum;		/* number of entries */

    uint16_t e_shentsize;	/* section header table entry size */
    uint16_t e_shnum;		/* number of entries */

    uint16_t e_shstrndx;		/* index of the section name string table */
} Elf32_Ehdr;

/*
 * ELF identification
 */
#define	EI_MAG0		0
#define	EI_MAG1		1
#define	EI_MAG2		2
#define	EI_MAG3		3
#define	EI_CLASS	4
#define	EI_DATA		5
#define	EI_VERSION	6
#define EI_PAD		7

/* EI_MAG */
#define	ELFMAG0		0x7f
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'

/* EI_CLASS */
#define	ELFCLASSNONE	0	/* invalid class */
#define	ELFCLASS32	1	/* 32-bit objects */
#define	ELFCLASS64	2	/* 64-bit objects */

/* EI_DATA */
#define	ELFDATANONE	0	/* invalide data encoding */
#define	ELFDATA2LSB	1	/* least significant byte first (0x01020304 is 0x04 0x03 0x02 0x01) */
#define	ELFDATA2MSB	2	/* most significant byte first (0x01020304 is 0x01 0x02 0x03 0x04) */

/* EI_VERSION */
#define	EV_CURRENT	1
#define	ELFVERSION	EV_CURRENT

/*
 * PROGRAM HEADER
 */
typedef struct {
    uint32_t p_type;		/* type of segment */
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

/* p_type */
#define	PT_NULL             0
#define	PT_LOAD             1
#define	PT_DYNAMIC          2
#define	PT_INTERP           3
#define	PT_NOTE             4
#define	PT_SHLIB            5
#define	PT_PHDR             6
#define	PT_LOPROC  0x70000000
#define	PT_HIPROC  0x7fffffff

/* p_flags */
#define PF_X	0x1
#define PF_W	0x2
#define PF_R	0x4



enum eElfSectionTypes {
    SHT_NULL,	//0
    SHT_PROGBITS,	//1
    SHT_SYMTAB,	//2
    SHT_STRTAB,	//3
    SHT_RELA,	//4
    SHT_HASH,	//5
    SHT_DYNAMIC,	//6
    SHT_NOTE,	//7
    SHT_NOBITS,	//8
    SHT_REL,	//9
    SHT_SHLIB,	//A
    SHT_DYNSYM,	//B
    SHT_LAST,	//C
    SHT_LOPROC = 0x70000000,
    SHT_HIPROC = 0x7fffffff,
    SHT_LOUSER = 0x80000000,
    SHT_HIUSER = 0xffffffff
};


typedef struct {
    uint32_t		name;
    uint32_t	type;
    uint32_t	flags;
    uint32_t	address;
    uint32_t	offset;
    uint32_t	size;
    uint32_t	link;
    uint32_t	info;
    uint32_t	addralign;
    uint32_t	entsize;
} Elf32_Scdr;





enum {
    R_386_NONE=0,	// none
    R_386_32,	// S+A
    R_386_PC32,	// S+A-P
    R_386_GOT32,	// G+A-P
    R_386_PLT32,	// L+A-P
    R_386_COPY,	// none
    R_386_GLOB_DAT,	// S
    R_386_JMP_SLOT,	// S
    R_386_RELATIVE,	// B+A
    R_386_GOTOFF,	// S+A-GOT
    R_386_GOTPC,	// GOT+A-P
    R_386_LAST	// none
};

typedef struct {
    uint16_t	d_tag;
    uint32_t	d_val;	//Also d_ptr
} Elf32_dyn;


enum {
    DT_NULL,	//!< Marks End of list
    DT_NEEDED,	//!< Offset in strtab to needed library
    DT_PLTRELSZ,	//!< Size in bytes of PLT
    DT_PLTGOT,	//!< Address of PLT/GOT
    DT_HASH,	//!< Address of symbol hash table
    DT_STRTAB,	//!< String Table address
    DT_SYMTAB,	//!< Symbol Table address
    DT_RELA,	//!< Relocation table address
    DT_RELASZ,	//!< Size of relocation table
    DT_RELAENT,	//!< Size of entry in relocation table
    DT_STRSZ,	//!< Size of string table
    DT_SYMENT,	//!< Size of symbol table entry
    DT_INIT,	//!< Address of initialisation function
    DT_FINI,	//!< Address of termination function
    DT_SONAME,	//!< String table offset of so name
    DT_RPATH,	//!< String table offset of library path
    DT_SYMBOLIC,//!< Reverse order of symbol searching for library, search libs first then executable
    DT_REL,		//!< Relocation Entries (Elf32_Rel instead of Elf32_Rela)
    DT_RELSZ,	//!< Size of above table (bytes)
    DT_RELENT,	//!< Size of entry in above table
    DT_PLTREL,	//!< Relocation entry of PLT
    DT_DEBUG,	//!< Debugging Entry - Unknown contents
    DT_TEXTREL,	//!< Indicates that modifcations to a non-writeable segment may occur
    DT_JMPREL,	//!< Address of PLT only relocation entries
    DT_LOPROC = 0x70000000,	//!< Low Definable
    DT_HIPROC = 0x7FFFFFFF	//!< High Definable
};

//Auxiliary Vector entry
typedef struct {
  uint32_t a_type;
  union {
      uint32_t a_val;
  } a_un;
} Elf32_auxv_t;

//Auxiliary Vector entry types
#define AT_NULL		0
#define AT_IGNORE	1
#define AT_EXECFD	2
#define AT_PHDR		3
#define AT_PHENT	4
#define AT_PHNUM	5
#define AT_PAGESZ	6
#define AT_BASE		7
#define AT_FLAGS	8
#define AT_ENTRY	9
#define AT_NOTELF	10
#define AT_UID		11
#define AT_EUID		12
#define AT_GID		13
#define AT_EGID		14
#define AT_CLKTCK	17


#define AT_PLATFORM	15
#define AT_HWCAP	16




#define AT_FPUCW	18


#define AT_DCACHEBSIZE	19
#define AT_ICACHEBSIZE	20
#define AT_UCACHEBSIZE	21



#define AT_IGNOREPPC	22

#define	AT_SECURE	23

#define AT_BASE_PLATFORM 24

#define AT_RANDOM	25

#define AT_HWCAP2	26

#define AT_EXECFN	31



#define AT_SYSINFO	32
#define AT_SYSINFO_EHDR	33



#define AT_L1I_CACHESHAPE	34
#define AT_L1D_CACHESHAPE	35
#define AT_L2_CACHESHAPE	36
#define AT_L3_CACHESHAPE	37

#define AUX_CNT 38

BOOL elf_is_valid(const char *elfData);
uint32_t elf_load(const char *elfData);
uint32_t elf_get_end_in_memory(const char *elfData);

#endif // ELF_H
