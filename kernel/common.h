#ifndef COMMON_H
#define COMMON_H

#define enableInterrupts() asm volatile("sti")
#define disableInterrupts() asm volatile("cli")
#define halt() asm volatile("hlt")

typedef unsigned long long 	uint64;
typedef signed long long	int64;
typedef unsigned int   uint32;
typedef          int   int32;
typedef unsigned short uint16;
typedef          short int16;
typedef unsigned char  uint8;
typedef          char  int8;
typedef unsigned int  size_t;
typedef int64 suseconds_t;

#define BOOL uint8
#define TRUE 1
#define FALSE 0
#define NULL 0
#define CHECK_BIT(value, pos) ((value) & (1 << (pos)))

#define	KERN_PAGE_DIRECTORY			0x00001000

//16M is identity mapped as below.
//First 12M we don't touch. Kernel code and runtime initrd are there.
//4M is reserved for 4K page directories.
#define RESERVED_AREA           0x01000000 //16 mb
#define KERN_PD_AREA_BEGIN      0x00C00000 //12 mb
#define KERN_PD_AREA_END        0x01000000 //16 mb


#define GFX_MEMORY              0x01000000 //16 mb

#define KERN_HEAP_BEGIN 		0x02000000 //32 mb
#define KERN_HEAP_END    		0x40000000 // 1 gb

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

#define KERNELMEMORY_PAGE_COUNT 256 //First 1GB kernel-space (first 256 entries in the page directory)

#define	KERN_STACK_SIZE		PAGESIZE_4K

//KERN_HEAP_END ends and this one starts
#define	USER_OFFSET         	0x40000000
#define	USER_MMAP_START     	0x80000000 //This is just for mapping starts searching vmem from here not to conflict with sbrk. It can start from USER_OFFSET if sbrk not used!
#define	MEMORY_END              0xFFC00000 //After this address is not usable. Because Page Tables sit there!

#define	SIZE_2MB        	0x200000 //2MB

#define	USER_STACK 			0xF0000000

void outb(uint16 port, uint8 value);
void outw(uint16 port, uint16 value);
uint8 inb(uint16 port);
uint16 inw(uint16 port);

#define PANIC(msg) panic(msg, __FILE__, __LINE__);
#define WARNING(msg) warning(msg, __FILE__, __LINE__);
#define ASSERT(b) ((b) ? (void)0 : panic_assert(__FILE__, __LINE__, #b))

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

void warning(const char *message, const char *file, uint32 line);
void panic(const char *message, const char *file, uint32 line);
void panic_assert(const char *file, uint32 line, const char *desc);

void* memset(uint8 *dest, uint8 val, uint32 len);
void* memcpy(uint8 *dest, const uint8 *src, uint32 len);
void* memmove(void* dest, const void* src, uint32 n);
int memcmp(const void* p1, const void* p2, uint32 c);

int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, int length);
char *strcpy(char *dest, const char *src);
char *strcpyNonNull(char *dest, const char *src);
char *strncpy(char *dest, const char *src, uint32 num);
char* strcat(char *dest, const char *src);
int strlen(const char *src);
int strFirstIndexOf(const char *src, char c);
int sprintf(char* buffer, const char *format, ...);

void printkf(const char *format, ...);

int atoi(char *str);
void itoa(char *buf, int base, int d);

uint32 rand();

uint32 readEip();
uint32 readEsp();
uint32 readCr3();
uint32 getCpuFlags();
BOOL isInterruptsEnabled();

void beginCriticalSection();
void endCriticalSection();


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
