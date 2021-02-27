//TODO: seperate files

#include "syscalls.h"
#include "fs.h"
#include "process.h"
#include "alloc.h"
#include "pipe.h"
#include "log.h"
#include "timer.h"
#include "sleep.h"
#include "spinlock.h"
#include "message.h"
#include "syscalltable.h"
#include "isr.h"
#include "sharedmemory.h"
#include "serial.h"
#include "vmm.h"
#include "list.h"
#include "ttydev.h"
#include "syscall_select.h"
#include "errno.h"
#include "ipc.h"
#include "socket.h"
#include "syscall_getthreads.h"

struct iovec {
               void  *iov_base;    /* Starting address */
               size_t iov_len;     /* Number of bytes to transfer */
           };

struct statx {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t pad1;
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    struct {
        int64_t tv_sec;
        uint32_t tv_nsec;
        int32_t pad;
    } stx_atime, stx_btime, stx_ctime, stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t spare[14];
};

struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned mask[2];
};

struct shmid_ds;

/**************
 * All of syscall entered with interrupts disabled!
 * A syscall can enable interrupts if it is needed.
 *
 **************/

static void handle_syscall(Registers* regs);

static void* g_syscall_table[SYSCALL_COUNT];

struct rusage;


int syscall_open(const char *pathname, int flags);
int syscall_close(int fd);
int syscall_read(int fd, void *buf, int nbytes);
int syscall_write(int fd, void *buf, int nbytes);
int syscall_lseek(int fd, int offset, int whence);
int syscall_stat(const char *path, struct stat *buf);
int syscall_fstat(int fd, struct stat *buf);
int syscall_ioctl(int fd, int32_t request, void *arg);
int syscall_exit();
void* syscall_sbrk(uint32_t increment);
int syscall_fork();
int syscall_getpid();
int syscall_execute(const char *path, char *const argv[], char *const envp[]);
int syscall_execve(const char *path, char *const argv[], char *const envp[]);
int syscall_wait(int *wstatus);
int syscall_kill(int pid, int sig);
int syscall_mount(const char *source, const char *target, const char *fs_type, unsigned long flags, void *data);
int syscall_unmount(const char *target);
int syscall_mkdir(const char *path, uint32_t mode);
int syscall_rmdir(const char *path);
int syscall_getdents(int fd, char *buf, int nbytes);
int syscall_getcwd(char *buf, size_t size);
int syscall_chdir(const char *path);
int syscall_manage_pipe(const char *pipe_name, int operation, int data);
int syscall_soso_read_dir(int fd, void *dirent, int index);
uint32_t syscall_get_uptime_ms();
int syscall_sleep_ms(int ms);
int syscall_execute_on_tty(const char *path, char *const argv[], char *const envp[], const char *tty_path);
int syscall_manage_message(int command, void* message);
int syscall_rt_sigaction(int signum, const struct k_sigaction *act, struct k_sigaction *oldact, uint32_t sigsetsize);
void* syscall_mmap(void *addr, int length, int flags, int prot, int fd, int offset);
int syscall_munmap(void *addr, int length);
int syscall_shm_open(const char *name, int oflag, int mode);
int syscall_unlink(const char *name);
int syscall_ftruncate(int fd, int size);
int syscall_posix_openpt(int flags);
int syscall_ptsname_r(int fd, char *buf, int buflen);
int syscall_printk(const char *str, int num);
int syscall_readv(int fd, const struct iovec *iovs, int iovcnt);
int syscall_writev(int fd, const struct iovec *iovs, int iovcnt);
int syscall_set_thread_area(void *p);
int syscall_set_tid_address(void* p);
int syscall_exit_group(int status);
int syscall_llseek(unsigned int fd, unsigned int offset_high,
            unsigned int offset_low, int64_t *result,
            unsigned int whence);

int syscall_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf);
int syscall_wait4(int pid, int *wstatus, int options, struct rusage *rusage);
int32_t syscall_clock_gettime64(int32_t clockid, struct timespec *tp);
int32_t syscall_clock_settime64(int32_t clockid, const struct timespec *tp);
int32_t syscall_clock_getres64(int32_t clockid, struct timespec *res);
int syscall_shmget(int32_t key, size_t size, int flag);
void * syscall_shmat(int shmid, const void *shmaddr, int shmflg);
int syscall_shmdt(const void *shmaddr);
int syscall_shmctl(int shmid, int cmd, struct shmid_ds *buf);
int syscall_nanosleep(const struct timespec *req, struct timespec *rem);

void syscalls_initialize()
{
    memset((uint8_t*)g_syscall_table, 0, sizeof(void*) * SYSCALL_COUNT);

    g_syscall_table[SYS_open] = syscall_open;
    g_syscall_table[SYS_close] = syscall_close;
    g_syscall_table[SYS_read] = syscall_read;
    g_syscall_table[SYS_write] = syscall_write;
    g_syscall_table[SYS_lseek] = syscall_lseek;
    g_syscall_table[SYS_stat] = syscall_stat;
    g_syscall_table[SYS_fstat] = syscall_fstat;
    g_syscall_table[SYS_ioctl] = syscall_ioctl;
    g_syscall_table[SYS_exit] = syscall_exit;
    g_syscall_table[SYS_sbrk] = syscall_sbrk;
    g_syscall_table[SYS_fork] = syscall_fork;
    g_syscall_table[SYS_getpid] = syscall_getpid;

    g_syscall_table[SYS_execute] = syscall_execute;
    g_syscall_table[SYS_execve] = syscall_execve;
    g_syscall_table[SYS_wait] = syscall_wait;
    g_syscall_table[SYS_kill] = syscall_kill;
    g_syscall_table[SYS_mount] = syscall_mount;
    g_syscall_table[SYS_unmount] = syscall_unmount;
    g_syscall_table[SYS_mkdir] = syscall_mkdir;
    g_syscall_table[SYS_rmdir] = syscall_rmdir;
    g_syscall_table[SYS_getdents] = syscall_getdents;
    g_syscall_table[SYS_getcwd] = syscall_getcwd;
    g_syscall_table[SYS_chdir] = syscall_chdir;
    g_syscall_table[SYS_manage_pipe] = syscall_manage_pipe;
    g_syscall_table[SYS_soso_read_dir] = syscall_soso_read_dir;
    g_syscall_table[SYS_get_uptime_ms] = syscall_get_uptime_ms;
    g_syscall_table[SYS_sleep_ms] = syscall_sleep_ms;
    g_syscall_table[SYS_execute_on_tty] = syscall_execute_on_tty;
    g_syscall_table[SYS_manage_message] = syscall_manage_message;
    g_syscall_table[SYS_rt_sigaction] = syscall_rt_sigaction;
    g_syscall_table[SYS_mmap] = syscall_mmap;
    g_syscall_table[SYS_munmap] = syscall_munmap;
    g_syscall_table[SYS_shm_open] = syscall_shm_open;
    g_syscall_table[SYS_unlink] = syscall_unlink;
    g_syscall_table[SYS_ftruncate] = syscall_ftruncate;
    g_syscall_table[SYS_posix_openpt] = syscall_posix_openpt;
    g_syscall_table[SYS_ptsname_r] = syscall_ptsname_r;
    g_syscall_table[SYS_printk] = syscall_printk;
    g_syscall_table[SYS_readv] = syscall_readv;
    g_syscall_table[SYS_writev] = syscall_writev;
    g_syscall_table[SYS_set_thread_area] = syscall_set_thread_area;
    g_syscall_table[SYS_set_tid_address] = syscall_set_tid_address;
    g_syscall_table[SYS_exit_group] = syscall_exit_group;
    g_syscall_table[SYS_llseek] = syscall_llseek;
    g_syscall_table[SYS_select] = syscall_select;
    g_syscall_table[SYS_statx] = syscall_statx;
    g_syscall_table[SYS_wait4] = syscall_wait4;
    g_syscall_table[SYS_clock_gettime64] = syscall_clock_gettime64;
    g_syscall_table[SYS_clock_settime64] = syscall_clock_settime64;
    g_syscall_table[SYS_clock_getres64] = syscall_clock_getres64;
    g_syscall_table[SYS_shmget] = syscall_shmget;
    g_syscall_table[SYS_shmat] = syscall_shmat;
    g_syscall_table[SYS_shmdt] = syscall_shmdt;
    g_syscall_table[SYS_shmctl] = syscall_shmctl;
    g_syscall_table[SYS_socket] = syscall_socket;
    g_syscall_table[SYS_socketpair] = syscall_socketpair;
    g_syscall_table[SYS_bind] = syscall_bind;
    g_syscall_table[SYS_connect] = syscall_connect;
    g_syscall_table[SYS_listen] = syscall_listen;
    g_syscall_table[SYS_accept4] = syscall_accept4;
    g_syscall_table[SYS_getsockopt] = syscall_getsockopt;
    g_syscall_table[SYS_setsockopt] = syscall_setsockopt;
    g_syscall_table[SYS_getsockname] = syscall_getsockname;
    g_syscall_table[SYS_getpeername] = syscall_getpeername;
    g_syscall_table[SYS_sendto] = syscall_sendto;
    g_syscall_table[SYS_sendmsg] = syscall_sendmsg;
    g_syscall_table[SYS_recvfrom] = syscall_recvfrom;
    g_syscall_table[SYS_recvmsg] = syscall_recvmsg;
    g_syscall_table[SYS_shutdown] = syscall_shutdown;
    g_syscall_table[SYS_accept] = syscall_accept;
    g_syscall_table[SYS_nanosleep] = syscall_nanosleep;
    g_syscall_table[SYS_getthreads] = syscall_getthreads;
    g_syscall_table[SYS_getprocs] = syscall_getprocs;

    // Register our syscall handler.
    interrupt_register (0x80, &handle_syscall);
}

static void handle_syscall(Registers* regs)
{
    Thread* thread = thread_get_current();
    Process* process = thread->owner;

    ++thread->called_syscall_count;

    if (regs->eax >= SYSCALL_COUNT)
    {
        printkf("Unknown SYSCALL:%d (pid:%d)\n", regs->eax, process->pid);
        log_printf("Unknown SYSCALL:%d (pid:%d)\n", regs->eax, process->pid);

        regs->eax = -1;
        return;
    }

    void *location = g_syscall_table[regs->eax];

    if (NULL == location)
    {
        printkf("Unused SYSCALL:%d (pid:%d)\n", regs->eax, process->pid);
        log_printf("Unused SYSCALL:%d (pid:%d)\n", regs->eax, process->pid);

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

int syscall_printk(const char *str, int num)
{
    if (!check_user_access((char*)str))
    {
        return -EFAULT;
    }

    printkf(str, num);

    return 0;
}

int syscall_open(const char *pathname, int flags)
{
    if (!check_user_access((char*)pathname))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        FileSystemNode* node = fs_get_node_absolute_or_relative(pathname, process);
        if (node)
        {
            File* file = fs_open(node, flags);

            if (file)
            {
                return file->fd;
            }
        }
    }

    return -1;
}

int syscall_close(int fd)
{
    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                fs_close(file);

                return 0;
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_read(int fd, void *buf, int nbytes)
{
    if (!check_user_access(buf))
    {
        return -EFAULT;
    }

    //Screen_PrintF("syscall_read: begin - nbytes:%d\n", nbytes);

    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                //log_printf("syscall_read(%d): %s\n", process->pid, buf);

                //enable_interrupts();

                //Each handler is free to enable interrupts.
                //We don't enable them here.

                int ret = fs_read(file, nbytes, buf);

                return ret;
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_write(int fd, void *buf, int nbytes)
{
    if (!check_user_access(buf))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        //Screen_PrintF("syscall_write() called from process: %d. fd:%d\n", process->pid, fd);

        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                /*
                for (int i = 0; i < nbytes; ++i)
                {
                    log_printf("pid:%d syscall_write:buf[%d]=%c\n", process->pid, i, ((char*)buf)[i]);
                }
                */

                uint32_t writeResult = fs_write(file, nbytes, buf);

                return writeResult;
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_readv(int fd, const struct iovec *iovs, int iovcnt)
{
    if (!check_user_access((void*)iovs))
    {
        return -EFAULT;
    }

    int result = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        const struct iovec *iov = iovs + i;

        if (iov->iov_len > 0)
        {
            int bytes = syscall_read(fd, iov->iov_base, iov->iov_len);

            if (bytes < 0)
            {
                return  bytes;
            }

            result += bytes;
        }
    }

    return result;
}

int syscall_writev(int fd, const struct iovec *iovs, int iovcnt)
{
    if (!check_user_access((void*)iovs))
    {
        return -EFAULT;
    }

    int result = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        const struct iovec *iov = iovs + i;

        if (iov->iov_len > 0)
        {
            int bytes = syscall_write(fd, iov->iov_base, iov->iov_len);

            if (bytes < 0)
            {
                return  bytes;
            }

            result += bytes;
        }
    }

    return result;
}

int syscall_set_thread_area(void *p)
{
    if (!check_user_access(p))
    {
        return -EFAULT;
    }

    return 0;
}

int syscall_set_tid_address(void* p)
{
    if (!check_user_access(p))
    {
        return -EFAULT;
    }

    return thread_get_current()->threadId;
}

int syscall_exit_group(int status)
{
    return syscall_exit();
}

int syscall_lseek(int fd, int offset, int whence)
{
    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                int result = fs_lseek(file, offset, whence);

                return result;
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_llseek(unsigned int fd, unsigned int offset_high,
            unsigned int offset_low, int64_t *result,
            unsigned int whence)
{
    if (!check_user_access(result))
    {
        return -EFAULT;
    }

    //this syscall is used for large files in 32 bit systems for the offset (offset_high<<32) | offset_low

    Process* process = thread_get_current()->owner;
    //printkf("syscall_llseek() called from process: %d. fd:%d\n", process->pid, fd);

    if (offset_high != 0)
    {
        return -1;
    }

    int res = syscall_lseek(fd, offset_low, whence);

    if (res < 0)
    {
        return res;
    }

    *result = res;

    return  0;
}

int syscall_stat(const char *path, struct stat *buf)
{
    if (!check_user_access((char*)path))
    {
        return -EFAULT;
    }

    if (!check_user_access(buf))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        FileSystemNode* node = fs_get_node_absolute_or_relative(path, process);

        if (node)
        {
            return fs_stat(node, buf);
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_fstat(int fd, struct stat *buf)
{
    if (!check_user_access(buf))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                return fs_stat(file->node, buf);
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_ioctl(int fd, int32_t request, void *arg)
{
    //Important!!
    //We don't check_user_access() for arg here. Because it is not always a pointer.
    //So it is driver's responsibility to check_user_access(arg) for using it as a pointer.

    Process* process = thread_get_current()->owner;
    if (process)
    {
        //serial_printf("syscall_ioctl fd:%d request:%d(%x) arg:%d(%x) pid:%d\n", fd, request, request, arg, arg, process->pid);

        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                return fs_ioctl(file, request, arg);
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_exit()
{
    Thread* thread = thread_get_current();

    thread_signal(thread, SIGTERM);
    
    wait_for_schedule();

    return -1;
}

void* syscall_sbrk(uint32_t increment)
{
    //Screen_PrintF("syscall_sbrk() !!! inc:%d\n", increment);

    Process* process = thread_get_current()->owner;
    if (process)
    {
        return sbrk(process, increment);
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return (void*)-1;
}

int syscall_fork()
{
    //Not implemented

    return -1;
}

int syscall_getpid()
{
    Process* process = thread_get_current()->owner;
    if (process)
    {
        return process->pid;
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

//NON-posix
int syscall_execute(const char *path, char *const argv[], char *const envp[])
{
    if (!check_user_access((char*)path))
    {
        return -EFAULT;
    }

    if (!check_user_access_string_array(argv))
    {
        return -EFAULT;
    }

    if (!check_user_access_string_array(envp))
    {
        return -EFAULT;
    }

    int result = -1;

    Process* process = thread_get_current()->owner;
    if (process)
    {
        FileSystemNode* node = fs_get_node_absolute_or_relative(path, process);
        if (node)
        {
            File* f = fs_open(node, 0);
            if (f)
            {
                void* image = kmalloc(node->length);

                //Screen_PrintF("executing %s and its %d bytes\n", filename, node->length);

                int32_t bytes_read = fs_read(f, node->length, image);

                //Screen_PrintF("syscall_execute: fs_read returned %d bytes\n", bytes_read);

                if (bytes_read > 0)
                {
                    char* name = "UserProcess";
                    if (NULL != argv)
                    {
                        name = argv[0];
                    }
                    Process* new_process = process_create_from_elf_data(name, image, argv, envp, process, NULL);

                    if (new_process)
                    {
                        result = new_process->pid;
                    }
                }
                fs_close(f);

                kfree(image);
            }

        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return result;
}

int syscall_execute_on_tty(const char *path, char *const argv[], char *const envp[], const char *tty_path)
{
    if (!check_user_access((char*)path))
    {
        return -EFAULT;
    }

    if (!check_user_access_string_array(argv))
    {
        return -EFAULT;
    }

    if (!check_user_access_string_array(envp))
    {
        return -EFAULT;
    }

    if (!check_user_access((char*)tty_path))
    {
        return -EFAULT;
    }

    int result = -1;

    Process* process = thread_get_current()->owner;
    if (process)
    {
        FileSystemNode* node = fs_get_node_absolute_or_relative(path, process);
        FileSystemNode* tty_node = fs_get_node_absolute_or_relative(tty_path, process);
        if (node && tty_node)
        {
            File* f = fs_open(node, 0);
            if (f)
            {
                void* image = kmalloc(node->length);

                //Screen_PrintF("executing %s and its %d bytes\n", filename, node->length);

                int32_t bytesRead = fs_read(f, node->length, image);

                //Screen_PrintF("syscall_execute: fs_read returned %d bytes\n", bytesRead);

                if (bytesRead > 0)
                {
                    char* name = "UserProcess";
                    if (NULL != argv)
                    {
                        name = argv[0];
                    }
                    Process* new_process = process_create_from_elf_data(name, image, argv, envp, process, tty_node);

                    if (new_process)
                    {
                        result = new_process->pid;
                    }
                }
                fs_close(f);

                kfree(image);
            }

        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return result;
}

int syscall_execve(const char *path, char *const argv[], char *const envp[])
{
    if (!check_user_access((char*)path))
    {
        return -EFAULT;
    }

    if (!check_user_access_string_array(argv))
    {
        return -EFAULT;
    }

    if (!check_user_access_string_array(envp))
    {
        return -EFAULT;
    }

    Process* calling_process = thread_get_current()->owner;

    FileSystemNode* node = fs_get_node(path);
    if (node)
    {
        File* f = fs_open(node, 0);
        if (f)
        {
            void* image = kmalloc(node->length);

            if (fs_read(f, node->length, image) > 0)
            {
                disable_interrupts(); //just in case if a file operation left interrupts enabled.

                Process* new_process = process_create_ex("fromExecve", calling_process->pid, 0, NULL, image, argv, envp, NULL, calling_process->tty);

                fs_close(f);

                kfree(image);

                if (new_process)
                {
                    process_destroy(calling_process);

                    wait_for_schedule();

                    //unreachable
                }
            }

            fs_close(f);

            kfree(image);
        }
    }


    //if it all goes well, this line is unreachable
    return 0;
}

int syscall_wait(int *wstatus)
{
    if (!check_user_access(wstatus))
    {
        return -EFAULT;
    }

    //TODO: return pid of exited child. implement with sendsignal

    Thread* current_thread = thread_get_current();

    Process* process = current_thread->owner;
    if (process)
    {
        Thread* thread = thread_get_first();
        while (thread)
        {
            if (process == thread->owner->parent)
            {
                //We have a child process

                thread_change_state(current_thread, TS_WAITCHILD, NULL);

                enable_interrupts();
                while (current_thread->state == TS_WAITCHILD)
                {
                    halt();
                }

                break;
            }

            thread = thread->next;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_wait4(int pid, int *wstatus, int options, struct rusage *rusage)
{
    if (!check_user_access(wstatus))
    {
        return -EFAULT;
    }

    if (!check_user_access(rusage))
    {
        return -EFAULT;
    }

    Thread* current_thread = thread_get_current();

    Process* process = current_thread->owner;
    if (process)
    {
        Thread* thread = thread_get_first();
        while (thread)
        {
            if (process == thread->owner->parent)
            {
                if (pid < 0 || pid == (int)thread->owner->pid)
                {
                    thread_change_state(current_thread, TS_WAITCHILD, NULL);

                    enable_interrupts();
                    while (current_thread->state == TS_WAITCHILD)
                    {
                        halt();
                    }

                    break;
                }
            }

            thread = thread->next;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int32_t syscall_clock_gettime64(int32_t clockid, struct timespec *tp)
{
    if (!check_user_access(tp))
    {
        return -EFAULT;
    }

    return clock_gettime64(clockid, tp);
}

int32_t syscall_clock_settime64(int32_t clockid, const struct timespec *tp)
{
    if (!check_user_access((void*)tp))
    {
        return -EFAULT;
    }

    return clock_settime64(clockid, tp);
}

int32_t syscall_clock_getres64(int32_t clockid, struct timespec *res)
{
    if (!check_user_access(res))
    {
        return -EFAULT;
    }

    return clock_getres64(clockid, res);
}

int syscall_kill(int pid, int sig)
{
    Process* selfProcess = thread_get_current()->owner;

    if (process_signal(pid, sig))
    {
        return 0;
    }

    return -1;
}

int syscall_mount(const char *source, const char *target, const char *fs_type, unsigned long flags, void *data)//non-posix
{
    if (!check_user_access((char*)source))
    {
        return -EFAULT;
    }

    if (!check_user_access((char*)target))
    {
        return -EFAULT;
    }

    if (!check_user_access((char*)fs_type))
    {
        return -EFAULT;
    }

    if (!check_user_access(data))
    {
        return -EFAULT;
    }

    BOOL result = fs_mount(source, target, fs_type, flags, data);

    if (TRUE == result)
    {
        return 0;//on success
    }

    return -1;//on error
}

int syscall_unmount(const char *target)//non-posix
{
    if (!check_user_access((char*)target))
    {
        return -EFAULT;
    }

    FileSystemNode* targetNode = fs_get_node(target);

    if (targetNode)
    {
        if (targetNode->node_type == FT_MOUNT_POINT)
        {
            targetNode->node_type = FT_DIRECTORY;

            targetNode->mount_point = NULL;

            //TODO: check conditions, maybe busy. make clean up.

            return 0;//on success
        }
    }

    return -1;//on error
}

int syscall_mkdir(const char *path, uint32_t mode)
{
    if (!check_user_access((char*)path))
    {
        return -EFAULT;
    }

    char parent_path[128];
    const char* name = NULL;
    int length = strlen(path);
    for (int i = length - 1; i >= 0; --i)
    {
        if (path[i] == '/')
        {
            name = path + i + 1;
            strncpy(parent_path, path, i);
            parent_path[i] = '\0';
            break;
        }
    }

    if (strlen(parent_path) == 0)
    {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }

    if (strlen(name) == 0)
    {
        return -1;
    }

    //Screen_PrintF("mkdir: parent:[%s] name:[%s]\n", parentPath, name);

    FileSystemNode* target_node = fs_get_node(parent_path);

    if (target_node)
    {
        BOOL success = fs_mkdir(target_node, name, mode);
        if (success)
        {
            return 0;//on success
        }
    }

    return -1;//on error
}

int syscall_rmdir(const char *path)
{
    if (!check_user_access((char*)path))
    {
        return -EFAULT;
    }

    //TODO:
    //return 0;//on success

    return -1;//on error
}

int syscall_getdents(int fd, char *buf, int nbytes)
{
    if (!check_user_access(buf))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                //Screen_PrintF("syscall_getdents(%d): %s\n", process->pid, buf);

                int byte_counter = 0;

                int index = 0;
                FileSystemDirent* dirent = fs_readdir(file->node, index);

                while (NULL != dirent && (byte_counter + sizeof(FileSystemDirent) <= nbytes))
                {
                    memcpy((uint8_t*)buf + byte_counter, (uint8_t*)dirent, sizeof(FileSystemDirent));

                    byte_counter += sizeof(FileSystemDirent);

                    index += 1;
                    dirent = fs_readdir(file->node, index);
                }

                return byte_counter;
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_soso_read_dir(int fd, void *dirent, int index)
{
    if (!check_user_access(dirent))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                FileSystemDirent* dirent_fs = fs_readdir(file->node, index);

                if (dirent_fs)
                {
                    memcpy((uint8_t*)dirent, (uint8_t*)dirent_fs, sizeof(FileSystemDirent));

                    return 1;
                }
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_getcwd(char *buf, size_t size)
{
    if (!check_user_access(buf))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (process->working_directory)
        {
            return fs_get_node_path(process->working_directory, buf, size);
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_chdir(const char *path)
{
    if (!check_user_access((char*)path))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        FileSystemNode* node = fs_get_node_absolute_or_relative(path, process);

        if (node)
        {
            process->working_directory = node;

            return 0; //success
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_manage_pipe(const char *pipe_name, int operation, int data)
{
    if (!check_user_access((char*)pipe_name))
    {
        return -EFAULT;
    }

    int result = -1;

    switch (operation)
    {
    case 0:
        result = pipe_exists(pipe_name);
        break;
    case 1:
        result = pipe_create(pipe_name, data);
        break;
    case 2:
        result = pipe_destroy(pipe_name);
        break;
    }

    return result;
}

uint32_t syscall_get_uptime_ms()
{
    return get_uptime_milliseconds();
}

int syscall_sleep_ms(int ms)
{
    Thread* thread = thread_get_current();

    sleep_ms(thread, (uint32_t)ms);

    return 0;
}

int syscall_manage_message(int command, void* message)
{
    if (!check_user_access(message))
    {
        return -EFAULT;
    }

    Thread* thread = thread_get_current();

    int result = -1;

    switch (command)
    {
    case 0:
        result = message_get_queue_count(thread);
        break;
    case 1:
        message_send(thread, (SosoMessage*)message);
        result = 0;
        break;
    case 2:
        //make blocking
        result = message_get_next(thread, (SosoMessage*)message);
        break;
    default:
        break;
    }

    return result;
}

int syscall_rt_sigaction(int signum, const struct k_sigaction *act, struct k_sigaction *oldact, uint32_t sigsetsize)
{
    //TODO
    return -1;
}

void* syscall_mmap(void *addr, int length, int flags, int prot, int fd, int offset)
{
    uint32_t v_address_hint = (uint32_t)addr;

    if (v_address_hint < USER_OFFSET)
    {
        v_address_hint = USER_MMAP_START;
    }

    if (length <= 0)
    {
        return (void*)-1;
    }

    Process* process = thread_get_current()->owner;

    if (process)
    {
        if (fd < 0)
        {
            int needed_pages = PAGE_COUNT(length);
            uint32_t free_pages = vmm_get_free_page_count();
            //printkf("alloc from mmap length:%x neededPages:%d freePages:%d\n", length, neededPages, freePages);
            if ((uint32_t)needed_pages + 1 > free_pages)
            {
                return (void*)-1;
            }
            uint32_t* physical_array = (uint32_t*)kmalloc(needed_pages * sizeof(uint32_t));
            for (int i = 0; i < needed_pages; ++i)
            {
                uint32_t page_frame = vmm_acquire_page_frame_4k();
                physical_array[i] = page_frame;
            }

            void* mem = vmm_map_memory(process, v_address_hint, physical_array, needed_pages, TRUE);
            if (mem != NULL)
            {
                memset((uint8_t*)mem, 0, length);
            }
            else
            {
                for (int i = 0; i < needed_pages; ++i)
                {
                    vmm_release_page_frame_4k(physical_array[i]);
                }

                mem = (void*)-1;
            }
            kfree(physical_array);

            return mem;
        }
        else
        {
            if (fd < SOSO_MAX_OPENED_FILES)
            {
                File* file = process->fd[fd];

                if (file)
                {
                    void* ret = fs_mmap(file, length, offset, flags);

                    if (ret)
                    {
                        return ret;
                    }
                }
                else
                {
                    return (void*)-EBADF;
                }
            }
            else
            {
                return (void*)-EBADF;
            }
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return (void*)-1;
}

//WARNING: if length is smaller than previous mmap, what will do to those remaining pages?
int syscall_munmap(void *addr, int length)
{
    if (!check_user_access(addr))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;

    if (process)
    {
        if ((uint32_t)addr < USER_OFFSET)
        {
            return -1;
        }

        sharedmemory_unmap_if_exists(process, (uint32_t)addr);

        if (TRUE == vmm_unmap_memory(process, (uint32_t)addr, PAGE_COUNT(length)))
        {
            return 0;
        }

        return -1;
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_FOLLOW 0x400
#define AT_EACCESS 0x200
#define AT_NO_AUTOMOUNT 0x800
#define AT_EMPTY_PATH 0x1000
#define AT_STATX_SYNC_TYPE 0x6000
#define AT_STATX_SYNC_AS_STAT 0x0000
#define AT_STATX_FORCE_SYNC 0x2000
#define AT_STATX_DONT_SYNC 0x4000
#define AT_RECURSIVE 0x8000

int syscall_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf)
{
    if (!check_user_access((char*)pathname))
    {
        return -EFAULT;
    }

    if (!check_user_access(statxbuf))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        int path_len = strlen(pathname);

        FileSystemNode* node = NULL;

        if (path_len > 0)
        {
            if (pathname[0] == '/') //ignore dirfd. this is absolute path
            {
                node = fs_get_node(pathname);
            }
            else
            {
                if (dirfd == AT_FDCWD) //pathname is relative to Current Working Directory
                {
                    node = fs_get_node_relative_to_node(pathname, process->working_directory);
                }
                else if (dirfd >= 0 && dirfd < SOSO_MAX_OPENED_FILES)
                {
                    File* dir_fd_dir = process->fd[dirfd];
                    if ((dir_fd_dir->node->node_type & FT_DIRECTORY) == FT_DIRECTORY) //pathname is relative to the directory that dirfd refers to
                    {
                        node = fs_get_node_relative_to_node(pathname, dir_fd_dir->node);
                    }
                }
            }
        }
        else if (path_len == 0)
        {
            if ((flags & AT_EMPTY_PATH) == AT_EMPTY_PATH)
            {
                if (dirfd >= 0 && dirfd < SOSO_MAX_OPENED_FILES)
                {
                    node = process->fd[dirfd]->node;
                }
            }
        }

        if (node)
        {
            struct stat st;
            memset((uint8_t*)&st, 0, sizeof(st));

            int statResult = fs_stat(node, &st);

            statxbuf->stx_mode = st.st_mode;
            statxbuf->stx_size = st.st_size;

            return statResult;
        }


    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

#define O_CREAT 0x200

int syscall_shm_open(const char *name, int oflag, int mode)
{
    if (!check_user_access((char*)name))
    {
        return -EFAULT;
    }

    FileSystemNode* node = NULL;

    if ((oflag & O_CREAT) == O_CREAT)
    {
        node = sharedmemory_create(name);
    }
    else
    {
        node = sharedmemory_get_node(name);
    }

    if (node)
    {
        File* file = fs_open(node, oflag);

        if (file)
        {
            return file->fd;
        }
    }

    return -1;
}

int syscall_unlink(const char *name)
{
    if (!check_user_access((char*)name))
    {
        return -EFAULT;
    }

    Process* process = g_current_thread->owner;

    FileSystemNode* node = fs_get_node_absolute_or_relative(name, process);

    if (NULL == node)
    {
        //maybe it is a shared mem

        node = sharedmemory_get_node(name);
    }

    if (node && node->unlink)
    {
        return node->unlink(node, 0);
    }

    return -1;
}

int syscall_ftruncate(int fd, int size)
{
    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                return fs_ftruncate(file, size);
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

//Old way of shared memory, modern systems prefer shm_open/mmap
//For simplicity, we return the same key as an identifier for now!
int syscall_shmget(int32_t key, size_t size, int flag)
{
    FileSystemNode* node = NULL;

    //printkf("shmget(key:%d, size:%d, flag:%d)\n", key, size, flag);

    //Let's simulate shm_open

    char name[64];
    sprintf(name, 64, "%d", key);

    node = sharedmemory_get_node(name);

    if (node) //found existing
    {
        if ((flag & IPC_CREAT) && (flag & IPC_EXCL)) //was ensuring create new, so fail!
        {
            return -EEXIST;
        }
    }
    else
    {
        if (flag & IPC_CREAT)
        {
            node = sharedmemory_create(name);
        }
    }
    
    if (node)
    {
        Process* process = thread_get_current()->owner;
        if (process)
        {
            BOOL already_opened = FALSE;
            for (size_t i = 0; i < SOSO_MAX_OPENED_FILES; i++)
            {
                File* file = process->fd[i];

                if (file->node == node)
                {
                    already_opened = TRUE;
                    break;
                }
            }
            
            if (already_opened == FALSE)
            {
                File* file = fs_open(node, O_RDWR);
                if (file)
                {
                    syscall_ftruncate(file->fd, size);
                }
            }
        }
        return key;
    }

    return -1;
}

void * syscall_shmat(int shmid, const void *shmaddr, int shmflg)
{
    if (shmaddr != NULL)
    {
        //We don't support preferred address through this interface for now!
        return (void*)-EINVAL;
    }

    FileSystemNode* node = NULL;

    //printkf("shmat(shmid:%d, shmaddr:%x, shmflg:%d)\n", shmid, shmaddr, shmflg);

    char name[64];
    sprintf(name, 64, "%d", shmid);

    node = sharedmemory_get_node(name);

    if (node)
    {
        Process* process = thread_get_current()->owner;
        if (process)
        {
            for (size_t i = 0; i < SOSO_MAX_OPENED_FILES; i++)
            {
                File* file = process->fd[i];

                if (file->node == node)
                {
                    return syscall_mmap((void*)shmaddr, file->node->length, shmflg, 0, file->fd, 0);
                }
            }
        }
    }

    return (void*)-EINVAL;
}

int syscall_shmdt(const void *shmaddr)
{
    //TODO:
    return -EINVAL;
}

int syscall_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    //TODO:
    return -EINVAL;
}

int syscall_posix_openpt(int flags)
{
    Process* process = thread_get_current()->owner;
    if (process)
    {
        FileSystemNode* node = ttydev_create();
        if (node)
        {
            TtyDev* tty_dev = (TtyDev*)node->private_node_data;
            tty_dev->controlling_process = process->pid;
            tty_dev->foreground_process = process->pid;

            File* file = fs_open(node, flags);

            if (file)
            {
                return file->fd;
            }
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;
}

int syscall_ptsname_r(int fd, char *buf, int buflen)
{
    if (!check_user_access(buf))
    {
        return -EFAULT;
    }

    Process* process = thread_get_current()->owner;
    if (process)
    {
        if (fd < SOSO_MAX_OPENED_FILES)
        {
            File* file = process->fd[fd];

            if (file)
            {
                TtyDev* tty_dev = file->node->private_node_data;

                FileSystemNode* slave_node = tty_dev->slave_node;

                int result = fs_get_node_path(slave_node, buf, buflen);

                if (result >= 0)
                {
                    return 0;
                }
            }
            else
            {
                return -EBADF;
            }
        }
        else
        {
            return -EBADF;
        }
    }
    else
    {
        PANIC("Process is NULL!\n");
    }

    return -1;//on error
}

int syscall_nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!check_user_access(req))
    {
        return -EFAULT;
    }

    if (!check_user_access(rem))
    {
        return -EFAULT;
    }

    if (req)
    {
        sleep_ms(g_current_thread, req->tv_sec * 1000);

        return 0;
    }

    return -1;
}