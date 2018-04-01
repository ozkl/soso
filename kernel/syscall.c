#include "syscall.h"
#include "syscalls.h"
#include "syscalltable.h"
#include "isr.h"

#include "screen.h"

static void handleSyscall(Registers* regs);

static void* gSyscallTable[SYSCALL_COUNT];


void initialiseSyscalls()
{
    memset((uint8*)gSyscallTable, 0, sizeof(void*) * SYSCALL_COUNT);

    gSyscallTable[SYS_open] = syscall_open;
    gSyscallTable[SYS_close] = syscall_close;
    gSyscallTable[SYS_read] = syscall_read;
    gSyscallTable[SYS_write] = syscall_write;
    gSyscallTable[SYS_lseek] = syscall_lseek;
    gSyscallTable[SYS_stat] = syscall_stat;
    gSyscallTable[SYS_fstat] = syscall_fstat;
    gSyscallTable[SYS_ioctl] = syscall_ioctl;
    gSyscallTable[SYS_exit] = syscall_exit;
    gSyscallTable[SYS_sbrk] = syscall_sbrk;
    gSyscallTable[SYS_fork] = syscall_fork;
    gSyscallTable[SYS_getpid] = syscall_getpid;

    gSyscallTable[SYS_execute] = syscall_execute;
    gSyscallTable[SYS_execve] = syscall_execve;
    gSyscallTable[SYS_wait] = syscall_wait;
    gSyscallTable[SYS_kill] = syscall_kill;
    gSyscallTable[SYS_mount] = syscall_mount;
    gSyscallTable[SYS_unmount] = syscall_unmount;
    gSyscallTable[SYS_mkdir] = syscall_mkdir;
    gSyscallTable[SYS_rmdir] = syscall_rmdir;
    gSyscallTable[SYS_getdents] = syscall_getdents;
    gSyscallTable[SYS_getWorkingDirectory] = syscall_getWorkingDirectory;
    gSyscallTable[SYS_setWorkingDirectory] = syscall_setWorkingDirectory;
    gSyscallTable[SYS_managePipe] = syscall_managePipe;
    gSyscallTable[SYS_readDir] = syscall_readDir;
    gSyscallTable[SYS_manageWindow] = syscall_manageWindow;

    // Register our syscall handler.
    registerInterruptHandler (0x80, &handleSyscall);
}

static void handleSyscall(Registers* regs)
{
    if (regs->eax >= SYSCALL_COUNT)
    {
        return;
    }

    void *location = gSyscallTable[regs->eax];

    if (NULL == location)
    {
        regs->eax = -1;
        return;
    }

    //Screen_PrintF("We are in syscall_handler\n");
    //Screen_PrintInterruptsEnabled();

    //I think it is better to enable interrupts in syscall implementations if it is needed.

    int ret;
    asm volatile (" \
      pushl %1; \
      pushl %2; \
      pushl %3; \
      pushl %4; \
      pushl %5; \
      call *%6; \
      popl %%ebx; \
      popl %%ebx; \
      popl %%ebx; \
      popl %%ebx; \
      popl %%ebx; \
    " : "=a" (ret) : "r" (regs->edi), "r" (regs->esi), "r" (regs->edx), "r" (regs->ecx), "r" (regs->ebx), "r" (location));
    regs->eax = ret;
}


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

int trigger_syscall_open(const char *pathname, int flags)
{
    return syscall2(SYS_open, (int)pathname, flags);
}

int trigger_syscall_close(int fd)
{
    return syscall1(SYS_close, fd);
}

int trigger_syscall_read(int fd, void *buf, int nbytes)
{
    return syscall3(SYS_read, fd, (int)buf, nbytes);
}

int trigger_syscall_write(int fd, void *buf, int nbytes)
{
    return syscall3(SYS_write, fd, (int)buf, nbytes);
}

int trigger_syscall_lseek(int fd, int offset, int whence)
{
    return syscall3(SYS_lseek, fd, offset, whence);
}

int trigger_syscall_stat(const char *path, struct stat *buf)
{
    return syscall2(SYS_stat, (int32)path, (int32)buf);
}

int trigger_syscall_fstat(int fd, struct stat *buf)
{
    return syscall2(SYS_fstat, (int32)fd, (int32)buf);
}

int trigger_syscall_ioctl(int fd, int32 request, void *arg)
{
    return syscall3(SYS_ioctl, fd, request, (int)arg);
}

int trigger_syscall_exit()
{
    return syscall0(SYS_exit);
}

void* trigger_syscall_sbrk(uint32 increment)
{
    return (void*)syscall1(SYS_sbrk, increment);
}

int trigger_syscall_fork()
{
    return syscall0(SYS_fork);
}

int trigger_syscall_getpid()
{
    return syscall0(SYS_getpid);
}

int trigger_syscall_execute(const char *path, char *const argv[], char *const envp[])
{
    return syscall3(SYS_execute, (int)path, (int)argv, (int)envp);
}

int trigger_syscall_execve(const char *path, char *const argv[], char *const envp[])
{
    return syscall3(SYS_execve, (int)path, (int)argv, (int)envp);
}

int trigger_syscall_wait(int *wstatus)
{
    return syscall1(SYS_wait, (int)wstatus);
}

int trigger_syscall_kill(int pid, int sig)
{
    return syscall2(SYS_kill, pid, sig);
}

int trigger_syscall_mount(const char *source, const char *target, const char *fsType, unsigned long flags, void *data)
{
    return syscall5(SYS_mount, (int)source, (int)target, (int)fsType, (int)flags, (int)data);
}

int trigger_syscall_unmount(const char *target)
{
    return syscall1(SYS_unmount, (int)target);
}

int trigger_syscall_mkdir(const char *path, uint32 mode)
{
    return syscall2(SYS_mkdir, (int)path, (int)mode);
}

int trigger_syscall_rmdir(const char *path)
{
    return syscall1(SYS_rmdir, (int)path);
}

int trigger_syscall_getdents(int fd, char *buf, int nbytes)
{
    return syscall3(SYS_getdents, fd, (int)buf, nbytes);
}

int trigger_syscall_getWorkingDirectory(char *buf, int size)
{
    return syscall2(SYS_getWorkingDirectory, (int)buf, size);
}

int trigger_syscall_setWorkingDirectory(const char *path)
{
    return syscall1(SYS_setWorkingDirectory, (int)path);
}

int trigger_syscall_managePipe(const char *pipeName, int operation, int data)
{
    return syscall3(SYS_managePipe, (int)pipeName, operation, data);
}

int trigger_syscall_readDir(int fd, void *dirent, int index)
{
    return syscall3(SYS_readDir, fd, (int)dirent, index);
}

int trigger_syscall_manageWindow(int command, int parameter1, int parameter2, int parameter3)
{
    return syscall4(SYS_manageWindow, command, parameter1, parameter2, parameter3);
}
