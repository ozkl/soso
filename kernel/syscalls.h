#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "common.h"

struct stat;

int syscall_open(const char *pathname, int flags);
int syscall_close(int fd);
int syscall_read(int fd, void *buf, int nbytes);
int syscall_write(int fd, void *buf, int nbytes);
int syscall_lseek(int fd, int offset, int whence);
int syscall_stat(const char *path, struct stat *buf);
int syscall_fstat(int fd, struct stat *buf);
int syscall_ioctl(int fd, int32 request, void *arg);
int syscall_exit();
void* syscall_sbrk(uint32 increment);
int syscall_fork();
int syscall_getpid();
int syscall_execute(const char *path, char *const argv[], char *const envp[]);
int syscall_execve(const char *path, char *const argv[], char *const envp[]);
int syscall_wait(int *wstatus);
int syscall_kill(int pid, int sig);
int syscall_mount(const char *source, const char *target, const char *fsType, unsigned long flags, void *data);
int syscall_unmount(const char *target);
int syscall_mkdir(const char *path, uint32 mode);
int syscall_rmdir(const char *path);
int syscall_getdents(int fd, char *buf, int nbytes);
int syscall_getWorkingDirectory(char *buf, int size);
int syscall_setWorkingDirectory(const char *path);
int syscall_managePipe(const char *pipeName, int operation, int data);
int syscall_readDir(int fd, void *dirent, int index);
int syscall_manageWindow(int command, int parameter1, int parameter2, int parameter3);

#endif // SYSCALLS_H
