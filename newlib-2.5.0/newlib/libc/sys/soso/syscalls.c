#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../../../../kernel/syscalltable.h"

int syscall0(int num)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num));
  return a;
}

int syscall1(int num, int p1)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1));
  return a;
}

int syscall2(int num, int p1, int p2)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2));
  return a;
}

int syscall3(int num, int p1, int p2, int p3)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d"((int)p3));
  return a;
}

int syscall4(int num, int p1, int p2, int p3, int p4)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d" ((int)p3), "S" ((int)p4));
  return a;
}

int syscall5(int num, int p1, int p2, int p3, int p4, int p5)
{
  int a;
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d" ((int)p3), "S" ((int)p4), "D" ((int)p5));
  return a;
}

 
void _exit()
{
   syscall0(SYS_exit);
}

int close(int file)
{
   return syscall1(SYS_close, file);
}

int execve(char *name, char **argv, char **env)
{
   return syscall3(SYS_execve, name, argv, env);
}
int fork()
{
   return syscall0(SYS_fork);
}

int fstat(int fd, struct stat *buf)
{
    return syscall2(SYS_fstat, fd, buf);
}

int stat(const char *path, struct stat *buf)
{
    return syscall2(SYS_stat, path, buf);
}

int getpid()
{
   return syscall0(SYS_getpid);
}

int isatty(int file);

int kill(int pid, int sig)
{
   return syscall2(SYS_kill, pid, sig);
}

int link(char *old, char *new);

int lseek(int file, int ptr, int dir)
{
  return syscall3(SYS_lseek, file, ptr, dir);
}

int open(const char *name, int flags, ...)
{
  mode_t mode = 0;

  if (((flags & O_CREAT) == O_CREAT) /*|| ((flags & O_TMPFILE) == O_TMPFILE)*/)
  {
    va_list args;
    va_start(args, flags);
    mode = (mode_t)(va_arg(args, int));
    va_end(args);
  }
  return syscall3(SYS_open, name, flags, mode);
}

int read(int file, char *ptr, int len)
{
    return syscall3(SYS_read, file, ptr, len);
}

caddr_t sbrk(int incr)
{
    return syscall1(SYS_sbrk, incr);
}

int stat(const char *file, struct stat *st);

clock_t times(struct tms *buf);

int unlink(char *name);

int wait(int *status)
{
    return syscall1(SYS_wait, status);
}

int write(int file, char *ptr, int len)
{
    return syscall3(SYS_write, file, ptr, len);
}

int gettimeofday(struct timeval *p, void *z);

//alp
int getdents(int fd, char *buf, int nbytes)
{
    return syscall3(SYS_getdents, fd, (int)buf, nbytes);
}

int execute(const char *path, char *const argv[], char *const envp[])
{
    return syscall3(SYS_execute, (int)path, (int)argv, (int)envp);
}

int executep(const char *filename, char *const argv[], char *const envp[])
{
    const char* path = getenv("PATH");

    if (path)
    {
        const char* path2 = path;

        char* bb = path2;

        char single[128];
        while (bb != NULL)
        {
            char* token = strstr(bb, ":");

            if (NULL == token)
            {
                int l = strlen(bb);
                if (l > 0)
                {
                    token = bb + l;
                }
            }

            if (token)
            {
                int len = token - bb;

                if (len > 0)
                {
                    strncpy(single, bb, len);
                    single[len] = '\0';

                    //printf("%d:[%s]\n", len, single);

                    strcat(single, "/");
                    strcat(single, filename);

                    int result = execute(single, argv, envp);

                    if (result > 0)
                    {
                        return result;
                    }
                }

                bb = token + 1;
            }
            else
            {
                break;
            }
        }
    }

    return -1;
}

int getWorkingDirectory(char *buf, int size)
{
    return syscall2(SYS_getWorkingDirectory, (int)buf, size);
}

int setWorkingDirectory(const char *path)
{
    return syscall1(SYS_setWorkingDirectory, (int)path);
}
