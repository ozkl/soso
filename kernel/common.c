#include "common.h"
#include "serial.h"
#include "console.h"
#include "terminal.h"
#include "process.h"
#include "log.h"

static BOOL g_interrupts_were_enabled = FALSE;

// Write a byte out to the specified port.
void outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (value));
}

void outw(uint16_t port, uint16_t value)
{
    asm volatile ("outw %1, %0" : : "dN" (port), "a" (value));
}

uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

uint16_t inw(uint16_t port)
{
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

// Copy len bytes from src to dest.
void* memcpy(uint8_t *dest, const uint8_t *src, uint32_t len)
{
    const uint8_t *sp = (const uint8_t *)src;
    uint8_t *dp = (uint8_t *)dest;
    for(; len != 0; len--) *dp++ = *sp++;

    return dest;
}

// Write len copies of val into dest.
void* memset(uint8_t *dest, uint8_t val, uint32_t len)
{
    uint8_t *temp = (uint8_t *)dest;
    for ( ; len != 0; len--) *temp++ = val;

    return dest;
}

void* memmove(void* dest, const void* src, uint32_t n)
{
    uint8_t* _dest;
    uint8_t* _src;

    if ( dest < src ) {
        _dest = ( uint8_t* )dest;
        _src = ( uint8_t* )src;

        while ( n-- ) {
            *_dest++ = *_src++;
        }
    } else {
        _dest = ( uint8_t* )dest + n;
        _src = ( uint8_t* )src + n;

        while ( n-- ) {
            *--_dest = *--_src;
        }
    }

    return dest;
}

int memcmp( const void* p1, const void* p2, uint32_t c )
{
    const uint8_t* su1, *su2;
    int8_t res = 0;

    for ( su1 = p1, su2 = p2; 0 < c; ++su1, ++su2, c-- ) {
        if ( ( res = *su1 - *su2 ) != 0 ) {
            break;
        }
    }

    return res;
}

// Compare two strings. Should return -1 if 
// str1 < str2, 0 if they are equal or 1 otherwise.
int strcmp(const char *str1, const char *str2)
{
      int i = 0;
      int failed = 0;
      while(str1[i] != '\0' && str2[i] != '\0')
      {
          if(str1[i] != str2[i])
          {
              failed = 1;
              break;
          }
          i++;
      }

      if ((str1[i] == '\0' && str2[i] != '\0') || (str1[i] != '\0' && str2[i] == '\0'))
      {
          failed = 1;
      }
  
      return failed;
}

int strncmp(const char *str1, const char *str2, int length)
{
    for (int i = 0; i < length; ++i)
    {
        if (str1[i] != str2[i])
        {
            return str1[i] - str2[i];
        }
    }

    return 0;
}

// Copy the NULL-terminated string src into dest, and
// return dest.
char *strcpy(char *dest, const char *src)
{
    do
    {
      *dest++ = *src++;
    }
    while (*src != 0);
    *dest = '\0';

    return dest;
}

char *strcpy_nonnull(char *dest, const char *src)
{
    do
    {
      *dest++ = *src++;
    }
    while (*src != 0);

    return dest;
}

//Copies the first num characters of source to destination. If the end of the source C string is found before num characters have been copied,
//destination is padded with zeros until a total of num characters have been written to it.
//No null-character is implicitly appended at the end of destination if source is longer than num.
//Thus, in this case, destination shall not be considered a null terminated C string.
char *strncpy(char *dest, const char *src, uint32_t num)
{
    BOOL source_ended = FALSE;
    for (uint32_t i = 0; i < num; ++i)
    {
        if (source_ended == FALSE && src[i] == '\0')
        {
            source_ended = TRUE;
        }

        if (source_ended)
        {
            dest[i] = '\0';
        }
        else
        {
            dest[i] = src[i];
        }
    }

    return dest;
}

char* strncpy_null(char *dest, const char *src, uint32_t num)
{
    BOOL source_ended = FALSE;
    for (uint32_t i = 0; i < num; ++i)
    {
        if (source_ended == FALSE && src[i] == '\0')
        {
            source_ended = TRUE;
        }

        if (source_ended)
        {
            dest[i] = '\0';
        }
        else
        {
            dest[i] = src[i];
        }

        if (i == num - 1)
        {
            dest[i] = '\0';
        }
    }

    return dest;
}

char* strcat(char *dest, const char *src)
{
    size_t i,j;
    for (i = 0; dest[i] != '\0'; i++)
        ;
    for (j = 0; src[j] != '\0'; j++)
        dest[i+j] = src[j];
    dest[i+j] = '\0';
    return dest;
}

int strlen(const char *src)
{
    int i = 0;
    if (NULL != src)
    {
        while (*src++)
            i++;
    }
    return i;
}

int str_first_index_of(const char *src, char c)
{
    int i = 0;
    while (src[i])
    {
        if (src[i] == c)
        {
            return i;
        }
        i++;
    }

    return -1;
}

uint32_t rand()
{
    static uint32_t x = 123456789;
    static uint32_t y = 362436069;
    static uint32_t z = 521288629;
    static uint32_t w = 88675123;

    uint32_t t;

    t = x ^ (x << 11);
    x = y; y = z; z = w;
    return w = w ^ (w >> 19) ^ t ^ (t >> 8);
}

int atoi(char *str)
{
    int result = 0;

    for (int i = 0; str[i] != '\0'; ++i)
    {
        result = result*10 + str[i] - '0';
    }

    return result;
}

void itoa (char *buf, int base, int d)
{
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;


    if (base == 'd' && d < 0)
    {
        *p++ = '-';
        buf++;
        ud = -d;
    }
    else if (base == 'x')
    {
        divisor = 16;
    }

    do
    {
        int remainder = ud % divisor;

        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'A' - 10;
    }
    while (ud /= divisor);


    *p = 0;

    //Reverse BUF.
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2)
    {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

typedef enum 
{
    PF_NONE = 0,
    PF_REPLACE_NEWLINE_RN = 1
} SprintfFlags;

int sprintf_va(char *buffer, uint32_t buffer_size, SprintfFlags flags, const char *format, __builtin_va_list vl)
{
    char c;
    char buf[20];
    char *p = NULL;

    uint32_t buffer_index = 0;

    while ((c = *format++) != 0)
    {
        if (c == '\n')
        {
            if (flags & PF_REPLACE_NEWLINE_RN)
            {
                buf[0] = '\r';
                buf[1] = '\n';
                buf[2] = '\0';
            }
            else
            {
                buf[0] = '\n';
                buf[1] = '\0';
            }
            p = buf;

            while (*p)
            {
                buffer[buffer_index++] = (*p++);
            }
        }
        else if (c == '%')
        {
            c = *format++;
            switch (c)
            {
            case 'p':
            case 'x':
                buf[0] = '0';
                buf[1] = 'x';
                itoa(buf + 2, c, __builtin_va_arg(vl, int));
                p = buf;
                goto string;
                break;
            case 'd':
            case 'u':
                itoa(buf, c, __builtin_va_arg(vl, int));
                p = buf;
                goto string;
                break;

            case 's':
                p = __builtin_va_arg(vl, char *);
                if (!p)
                    p = "(null)";

            string:
                while (*p)
                {
                    buffer[buffer_index++] = (*p++);
                }
                break;

            default:
                buffer[buffer_index++] = __builtin_va_arg(vl, int);
                break;
            }
        }
        else
        {
            buffer[buffer_index++] = c;
        }

        if (buffer_index >= buffer_size - 1)
        {
            break;
        }
    }

    buffer[buffer_index] = '\0';

    return buffer_index;
}

int sprintf(char* buffer, uint32_t buffer_size, const char *format, ...)
{
    int result = 0;

    __builtin_va_list vl;
    __builtin_va_start(vl, format);

    result = sprintf_va(buffer, buffer_size, PF_NONE, format, vl);

    __builtin_va_end(vl);

    return result;
}

void printkf(const char *format, ...)
{
    char buffer[1024];
    buffer[0] = 'k';
    buffer[1] = ':';
    buffer[2] = 0;

    __builtin_va_list vl;
    __builtin_va_start(vl, format);

    sprintf_va(buffer + 2, 1022, PF_REPLACE_NEWLINE_RN, format, vl);

    __builtin_va_end(vl);

    Terminal* terminal = NULL;

    /*
    if (g_current_thread &&
        g_current_thread->owner &&
        g_current_thread->owner->tty
        )
    {
        TtyDev* tty = (TtyDev*)g_current_thread->owner->tty->private_node_data;

        terminal = console_get_terminal_by_master(tty->master_node);
    }
    */

   //Now printkf always prints to first terminal

    if (!terminal)
    {
        terminal = console_get_terminal(0);
    }

    if (terminal)
    {
        terminal_put_text(terminal, (uint8_t*)buffer, 1024);
    }
}

void panic(const char *message, const char *file, uint32_t line)
{
    disable_interrupts();

    printkf("PANIC:%s:%d:%s\n", file, line, message);

    log_printf("PANIC:%s:%d:%s\n", file, line, message);

    halt();
}

void warning(const char *message, const char *file, uint32_t line)
{
    printkf("WARNING:%s:%d:%s\n", file, line, message);
}

void panic_assert(const char *file, uint32_t line, const char *desc)
{
    disable_interrupts();

    printkf("ASSERTION-FAILED:%s:%d:%s\n", file, line, desc);

    halt();
}

uint32_t read_esp()
{
    uint32_t stack_pointer;
    asm volatile("mov %%esp, %0" : "=r" (stack_pointer));

    return stack_pointer;
}

uint32_t read_cr3()
{
    uint32_t value;
    asm volatile("mov %%cr3, %0" : "=r" (value));

    return value;
}

uint32_t get_cpu_flags()
{
    uint32_t eflags = 0;

    asm("pushfl; pop %%eax; mov %%eax, %0": "=m"(eflags):);

    return eflags;
}

BOOL is_interrupts_enabled()
{
    uint32_t eflags = get_cpu_flags();

    uint32_t interruptFlag = 0x200; //9th flag

    return (eflags & interruptFlag) == interruptFlag;
}

void begin_critical_section()
{
    g_interrupts_were_enabled = is_interrupts_enabled();

    disable_interrupts();
}

void end_critical_section()
{
    if (g_interrupts_were_enabled)
    {
        enable_interrupts();
    }
}

BOOL check_user_access(void* pointer)
{
    //TODO: check MEMORY_END as well
    if ((uint32_t)pointer >= USER_OFFSET || pointer == NULL)
    {
        return TRUE;
    }

    return FALSE;
}

BOOL check_user_access_string_array(char *const array[])
{
    int i = 0;
    const char* a = array[0];
    while (NULL != a)
    {
        if (!check_user_access((char*)a))
        {
            return FALSE;
        }
        a = array[++i];
    }

    return TRUE;
}