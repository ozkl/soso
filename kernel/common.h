#ifndef COMMON_H
#define COMMON_H

#include "stdint.h"

#define enable_interrupts() asm volatile("sti")
#define disable_interrupts() asm volatile("cli")
#define halt() asm volatile("hlt")

#define BOOL uint8_t
#define TRUE 1
#define FALSE 0
#define NULL 0
#define CHECK_BIT(value, pos) ((value) & (1 << (pos)))

#define BITMAP_DEFINE(bitmap, size)	uint8_t bitmap[size/8]
#define BITMAP_SET(bitmap, index)	bitmap[((uint32_t)index)/8] |=  (1 << (((uint32_t) index)%8))
#define BITMAP_UNSET(bitmap, index)	bitmap[((uint32_t)index)/8] &= ~(1 << (((uint32_t) index)%8))
#define BITMAP_CHECK(bitmap, index)	(bitmap[((uint32_t) index)/8] & (1 << (((uint32_t) index)%8)))

#define KERNEL_VIRTUAL_BASE         0xC0000000



extern uint32_t g_modules_end_physical;


extern uint32_t g_kern_heap_begin;

#define	PAGING_FLAG 		0x80000000	// CR0 - bit 31
#define PSE_FLAG			0x00000010	// CR4 - bit 4 //For 4M page support.
#define PG_PRESENT			0x00000001	// page directory / table
#define PG_WRITE			0x00000002
#define PG_USER				0x00000004
#define PG_4MB				0x00000080
#define PG_OWNED			0x00000200  // We use 9th bit for bookkeeping of owned pages (9-11th bits are available for OS)
#define	PAGESIZE_4K 		0x00001000
#define	PAGESIZE_4M			0x00400000
#define	RAM_AS_4K_PAGES		0x100000
#define	RAM_AS_4M_PAGES		1024
#define PAGE_COUNT(bytes)       (((bytes-1) / PAGESIZE_4K) + 1)
#define PAGE_INDEX_4K(addr)		((addr) >> 12)
#define PAGE_INDEX_4M(addr)		((addr) >> 22)

#define	KERN_STACK_SIZE		PAGESIZE_4K


#define	USER_MMAP_START     	0x80000000 //This is just for mapping starts searching vmem from here not to conflict with brk.
#define	USER_BRK_START      	0x70000000
#define	MEMORY_END              0xFFC00000 //After this address is not usable. Because Page Tables sit there!

#define GFX_MEMORY              0xFF400000

#define KERN_HEAP_END    		GFX_MEMORY

#define	USER_STACK 			0xA0000000

void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);

#define PANIC(msg) panic(msg, __FILE__, __LINE__);
#define WARNING(msg) warning(msg, __FILE__, __LINE__);
#define ASSERT(b) ((b) ? (void)0 : panic_assert(__FILE__, __LINE__, #b))

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void warning(const char *message, const char *file, uint32_t line);
void panic(const char *message, const char *file, uint32_t line);
void panic_assert(const char *file, uint32_t line, const char *desc);

void* memset(uint8_t *dest, uint8_t val, uint32_t len);
void* memcpy(uint8_t *dest, const uint8_t *src, uint32_t len);
void* memmove(void* dest, const void* src, uint32_t n);
int memcmp(const void* p1, const void* p2, uint32_t c);

int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, int length);
char *strcpy(char *dest, const char *src);
char *strcpy_nonnull(char *dest, const char *src);
char *strncpy(char *dest, const char *src, uint32_t num);
char* strncpy_null(char *dest, const char *src, uint32_t num);
char* strcat(char *dest, const char *src);
int strlen(const char *src);
char * strstr(const char * haystack, const char * needle);
char* strchr(const char* str, int ch);
int str_first_index_of(const char *src, char c);
void get_parent_path(const char *input, char *output, size_t output_size);
void get_basename(const char *input, char *output, size_t output_size);
int snprintf(char* buffer, uint32_t buffer_size, const char *format, ...);

void printkf(const char *format, ...);

int atoi(char *str);
void itoa(char *buf, int base, int d);
long strtol(const char *nptr, char **endptr, int base);

uint32_t rand();

uint32_t read_eip();
uint32_t read_esp();
uint32_t read_cr3();
uint32_t get_cpu_flags();
BOOL is_interrupts_enabled();

void begin_critical_section();
void end_critical_section();

BOOL check_user_access(void* pointer);
BOOL check_user_access_string_array(char *const array[]);


#define isalpha(a) ((((unsigned)(a)|32)-'a') < 26)
#define isdigit(a) (((unsigned)(a)-'0') < 10)
#define islower(a) (((unsigned)(a)-'a') < 26)
#define isupper(a) (((unsigned)(a)-'A') < 26)
#define isprint(a) (((unsigned)(a)-0x20) < 0x5f)
#define isgraph(a) (((unsigned)(a)-0x21) < 0x5e)
#define isspace(a) (a == ' ' || (unsigned)a-'\t' < 5)
#define iscntrl(a) ((unsigned)a < 0x20 || a == 0x7f)
#define tolower(a) ((a)|0x20)
#define toupper(a) ((a)&0x5f)

#endif // COMMON_H
