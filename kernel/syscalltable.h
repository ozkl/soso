#ifndef SYSCALLTABLE_H
#define SYSCALLTABLE_H

//This file will also be included by C library.
enum
{
    SYS_open,
    SYS_close,
    SYS_read,
    SYS_write,
    SYS_lseek,
    SYS_stat,
    SYS_fstat,
    SYS_ioctl,
    SYS_exit,
    SYS_sbrk,
    SYS_fork,
    SYS_getpid,

    //non-posix
    SYS_execute,
    SYS_execve,
    SYS_wait,
    SYS_kill,
    SYS_mount,
    SYS_unmount,
    SYS_mkdir,
    SYS_rmdir,
    SYS_getdents,
    SYS_getWorkingDirectory,
    SYS_setWorkingDirectory,

    SYSCALL_COUNT
};

#endif // SYSCALLTABLE_H
