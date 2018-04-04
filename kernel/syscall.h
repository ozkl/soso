#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"
#include "process.h"

void initialiseSyscalls();

int trigger_syscall_open(const char *pathname, int flags);
int trigger_syscall_close(int fd);
int trigger_syscall_read(int fd, void *buf, int nbytes);
int trigger_syscall_write(int fd, void *buf, int nbytes);
int trigger_syscall_lseek(int fd, int offset, int whence);
int trigger_syscall_stat(const char *path, struct stat *buf);
int trigger_syscall_fstat(int fd, struct stat *buf);
int trigger_syscall_ioctl(int fd, int32 request, void *arg);
int trigger_syscall_exit();
void* trigger_syscall_sbrk(uint32 increment);
int trigger_syscall_fork();
int trigger_syscall_getpid();

int trigger_syscall_execute(const char *path, char *const argv[], char *const envp[]);
int trigger_syscall_execve(const char *path, char *const argv[], char *const envp[]);
int trigger_syscall_wait(int *wstatus);
int trigger_syscall_kill(int pid, int sig);
int trigger_syscall_mount(const char *source, const char *target, const char *fsType, unsigned long flags, void *data);
int trigger_syscall_unmount(const char *target);
int trigger_syscall_mkdir(const char *path, uint32 mode);
int trigger_syscall_rmdir(const char *path);
int trigger_syscall_getdents(int fd, char *buf, int nbytes);
int trigger_syscall_getWorkingDirectory(char *buf, int size);
int trigger_syscall_setWorkingDirectory(const char *path);
int trigger_syscall_managePipe(const char *pipeName, int operation, int data);
int trigger_syscall_readDir(int fd, void *dirent, int index);
int trigger_syscall_manageWindow(int command, int parameter1, int parameter2, int parameter3);
int trigger_syscall_getUptimeMilliseconds();
int trigger_syscall_sleepMilliseconds(int ms);
int trigger_syscall_executeOnTTY(const char *path, char *const argv[], char *const envp[], const char *ttyPath);
int trigger_syscall_getMessageQueue(int command, void* message);

#endif
