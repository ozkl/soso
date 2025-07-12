#ifndef SYSCALLTABLE_H
#define SYSCALLTABLE_H

//C library must be compatible with these numbers.
enum
{
    SYS_exit = 1,
    SYS_fork = 2,
    SYS_read = 3,
    SYS_write = 4,
    SYS_open = 5,
    SYS_close = 6,
    SYS_unlink = 10,
    SYS_execve = 11,
    SYS_chdir = 12,
    SYS_lseek = 19,
    SYS_getpid = 20,
    SYS_mount = 21,
    SYS_unmount = 22,
    SYS_kill = 37,
    SYS_mkdir = 39,
    SYS_rmdir = 40,
    SYS_brk = 45,
    SYS_ioctl = 54,
    SYS_mmap = 90,
    SYS_munmap = 91,
    SYS_ftruncate = 93,
    SYS_stat = 106,
    SYS_fstat = 108,
    SYS_wait4 = 114,
    SYS_llseek = 140,
    SYS_select = 142,
    SYS_readv = 145,
    SYS_writev = 146,
    SYS_nanosleep = 162,
    SYS_rt_sigaction = 174,
    SYS_getcwd = 183,
    SYS_mmap2 = 192,
    SYS_getdents64 = 220,
    SYS_set_thread_area = 243,
    SYS_exit_group = 252,
    SYS_set_tid_address = 258,

    SYS_socket = 359,
    SYS_socketpair = 360,
    SYS_bind = 361,
    SYS_connect = 362,
    SYS_listen = 363,
    SYS_accept4 = 364,
    SYS_getsockopt = 365,
    SYS_setsockopt = 366,
    SYS_getsockname = 367,
    SYS_getpeername = 368,
    SYS_sendto = 369,
    SYS_sendmsg = 370,
    SYS_recvfrom = 371,
    SYS_recvmsg = 372,
    SYS_shutdown = 373,

    SYS_statx = 383,

    SYS_shmget = 395,
    SYS_shmctl = 396,
    SYS_shmat = 397,
    SYS_shmdt = 398,

    SYS_clock_gettime64 = 403,
    SYS_clock_settime64 = 404,
    SYS_clock_getres64 = 406,

    
    //soso specific
    //TODO: make them linux compatible and delete below
    //we can keep some of them like SYS_getprocs vs.
    //but openpt and ptsname really should be implemented by /dev/ptmx
    
    SYS_posix_openpt = 3001,
    SYS_ptsname_r,
    SYS_accept,
    SYS_getthreads,
    SYS_getprocs,
    SYS_shm_open,
    SYS_execute,
    SYS_execute_on_tty,
    SYS_soso_read_dir,
    SYS_unused,
    SYS_unused2,
    SYS_manage_message,
    SYS_manage_pipe,
    SYS_wait,
    SYS_printk,

    SYSCALL_COUNT
};

#endif // SYSCALLTABLE_H
