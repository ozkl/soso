#include "sosousdk.h"
#include "../kernel/syscalltable.h"
#include "../kernel/termios.h"


static int syscall0(int num)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num));
  return a;
}

static int syscall1(int num, int p1)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1));
  return a;
}

static int syscall2(int num, int p1, int p2)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2));
  return a;
}

static int syscall3(int num, int p1, int p2, int p3)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d"((int)p3));
  return a;
}

static int syscall4(int num, int p1, int p2, int p3, int p4)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d" ((int)p3), "S" ((int)p4));
  return a;
}

static int syscall5(int num, int p1, int p2, int p3, int p4, int p5)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d" ((int)p3), "S" ((int)p4), "D" ((int)p5));
  return a;
}

unsigned int getUptimeMilliseconds()
{
    return syscall0(SYS_getUptimeMilliseconds);
}

void sleepMilliseconds(unsigned int ms)
{
    syscall1(SYS_sleepMilliseconds, ms);
}

int executeOnTTY(const char *path, char *const argv[], char *const envp[], const char *ttyPath)
{
    return syscall4(SYS_executeOnTTY, (int)path, (int)argv, (int)envp, (int)ttyPath);
}

int syscall_ioctl(int fd, int request, void *arg)
{
    return syscall3(SYS_ioctl, fd, request, (int)arg);
}

void sendCharacterToTTY(int fd, char c)
{
    syscall_ioctl(fd, 0, (void*)(int)c);
}

int syscall_manageMessage(int command, void* message)
{
    return syscall2(SYS_manageMessage, command, (int)message);
}

int getMessageQueueCount()
{
    return syscall_manageMessage(0, 0);
}

void sendMessage(SosoMessage* message)
{
    syscall_manageMessage(1, message);
}

int getNextMessage(SosoMessage* message)
{
    return syscall_manageMessage(2, message);
}

int getTTYBufferSize(int fd)
{
    return syscall_ioctl(fd, 1, 0);
}

int getTTYBuffer(int fd, TtyUserBuffer* ttyBuffer)
{
    return syscall_ioctl(fd, 3, ttyBuffer);
}

int setTTYBuffer(int fd, TtyUserBuffer* ttyBuffer)
{
    return syscall_ioctl(fd, 2, ttyBuffer);
}

void* mmap(void *addr, int length, int flags, int fd, int offset)
{
    return (void*)syscall5(SYS_mmap, (int)addr, length, flags, fd, offset);
}

int munmap(void *addr, int length)
{
    return syscall2(SYS_munmap, (int)addr, length);
}

int shm_open(const char *name, int oflag, int mode)
{
    return syscall3(SYS_shm_open, (int)name, (int)oflag, (int)mode);
}

int shm_unlink(const char *name)
{
    return syscall1(SYS_shm_unlink, (int)name);
}

int ftruncate(int fd, int size)
{
    return syscall2(SYS_ftruncate, fd, size);
}

int tcgetattr(int fd, struct termios *termios_p)
{
  return syscall_ioctl(fd, TCGETS, (void*)termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
  int cmd;

  switch (optional_actions)
  {
    case TCSANOW:
      cmd = TCSETS;
    break;
    case TCSADRAIN:
      cmd = TCSETSW;
    break;
    case TCSAFLUSH:
      cmd = TCSETSF;
    break;
    default:
      //errno = EINVAL;
      return -1;
  }
  return syscall_ioctl(fd, cmd, (void*)termios_p);
}

#define PSF_FONT_MAGIC 0x864ab572

extern char _binary_font_psf_start;

typedef struct {
    unsigned int magic;         /* magic bytes to identify PSF */
    unsigned int version;       /* zero */
    unsigned int headersize;    /* offset of bitmaps in file, 32 */
    unsigned int flags;         /* 0 if there's no unicode table */
    unsigned int numglyph;      /* number of glyphs */
    unsigned int bytesperglyph; /* size of each glyph */
    unsigned int height;        /* height in pixels */
    unsigned int width;         /* width in pixels */
} PSF_font;

void drawCharAt(
    unsigned char* windowBuffer,
    /* note that this is int, not char as it's a unicode character */
    unsigned short int c,
    /* cursor position on screen, in characters not in pixels */
    int cx, int cy,
    int windowWidth, int windowHeight,
    /* foreground and background colors, say 0xFFFFFF and 0x000000 */
    unsigned int fg, unsigned int bg)
{
    /* cast the address to PSF header struct */
    PSF_font *font = (PSF_font*)&_binary_font_psf_start;
    /* we need to know how many bytes encode one row */
    int bytesperline=(font->width+7)/8;
    /* unicode translation */
    /*
    if(gUnicode != NULL) {
        c = gUnicode[c];
    }*/
    /* get the glyph for the character. If there's no
       glyph for a given character, we'll display the first glyph. */
    unsigned char *glyph =
     (unsigned char*)&_binary_font_psf_start +
     font->headersize +
     (c>0&&c<font->numglyph?c:0)*font->bytesperglyph;
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    int offs =
        (cy * font->height * windowWidth * 4) +
        (cx * (font->width+1) * 4);
    /* finally display pixels according to the bitmap */
    int x,y, line,mask;
    for(y=0;y<font->height;y++){
        /* save the starting position of the line */
        line=offs;
        mask=1<<(font->width-1);
        /* display a row */
        for(x=0;x<font->width;x++)
        {
            if (c == 0)
            {
                *((unsigned int*)(windowBuffer + line)) = bg;
            }
            else
            {
                *((unsigned int*)(windowBuffer + line)) = ((int)*glyph) & (mask) ? fg : bg;
            }

            /* adjust to the next pixel */
            mask >>= 1;
            line += 4;
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += windowWidth * 4;
    }
}
