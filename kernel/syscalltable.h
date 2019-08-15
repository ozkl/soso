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
    SYS_managePipe,
    SYS_readDir,
    SYS_getUptimeMilliseconds,
    SYS_sleepMilliseconds,
    SYS_executeOnTTY,
    SYS_manageMessage,
    SYS_UNUSED,

    SYS_mmap,
    SYS_munmap,
    SYS_shm_open,
    SYS_shm_unlink,
    SYS_ftruncate,
    SYS_posix_openpt,
    SYS_ptsname_r,

    SYSCALL_COUNT
};

#endif // SYSCALLTABLE_H
