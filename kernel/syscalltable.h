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

    SYS_execute,
    SYS_execve,
    SYS_wait,
    SYS_kill,
    SYS_mount,
    SYS_unmount,
    SYS_mkdir,
    SYS_rmdir,
    SYS_getdents,
    SYS_getcwd,
    SYS_chdir,
    SYS_manage_pipe,
    SYS_soso_read_dir,
    SYS_get_uptime_ms,
    SYS_sleep_ms,
    SYS_execute_on_tty,
    SYS_manage_message,
    SYS_rt_sigaction,

    SYS_mmap,
    SYS_munmap,
    SYS_shm_open,
    SYS_unlink,
    SYS_ftruncate,
    SYS_posix_openpt,
    SYS_ptsname_r,

    SYS_printk,

    SYS_readv,
    SYS_writev,
    SYS_set_thread_area,
    SYS_set_tid_address,
    SYS_exit_group,
    SYS_llseek,
    SYS_select,
    SYS_statx,
    SYS_wait4,
    SYS_clock_gettime64,
    SYS_clock_settime64,
    SYS_clock_getres64,
    
    SYS_shmget,
    SYS_shmat,
    SYS_shmdt,
    SYS_shmctl,

    SYS_socket,
    SYS_socketpair,
    SYS_bind,
    SYS_connect,
    SYS_listen,
    SYS_accept4,
    SYS_getsockopt,
    SYS_setsockopt,
    SYS_getsockname,
    SYS_getpeername,
    SYS_sendto,
    SYS_sendmsg,
    SYS_recvfrom,
    SYS_recvmsg,
    SYS_shutdown,
    SYS_accept,

    SYS_nanosleep,
    SYS_getthreads,
    SYS_getprocs,

    SYSCALL_COUNT
};

#endif // SYSCALLTABLE_H
