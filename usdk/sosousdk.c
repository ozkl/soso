#include <stdlib.h>
#include <string.h>

#include "sosousdk.h"
#include "../kernel/syscalltable.h"


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

