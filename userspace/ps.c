#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

#include <sys/syscall.h>

#define SOSO_MAX_OPENED_FILES 20
#define SOSO_PROCESS_NAME_MAX 32

typedef struct ThreadInfo
{
    uint32_t thread_id;
    uint32_t process_id;
    uint32_t state;
    uint32_t user_mode;

    uint32_t birth_time;
    uint32_t context_switch_count;
    uint32_t context_start_time;
    uint32_t context_end_time;
    uint32_t consumed_cpu_time_ms;
    uint32_t usage_cpu;
    uint32_t called_syscall_count;
} ThreadInfo;

typedef struct ProcInfo
{
    uint32_t process_id;
    int32_t parent_process_id;
    uint32_t fd[SOSO_MAX_OPENED_FILES];

    char name[SOSO_PROCESS_NAME_MAX];
    char tty[128];
    char working_directory[128];
} ProcInfo;

int32_t getthreads(ThreadInfo* threads, uint32_t max_count, uint32_t flags)
{
    return syscall(3004, threads, max_count, flags);
}

int32_t getprocs(ProcInfo* procs, uint32_t max_count, uint32_t flags)
{
    return syscall(3005, procs, max_count, flags);
}

int get_process_cpu_usage(uint32_t pid, ThreadInfo* threads, uint32_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        ThreadInfo* info = threads + i;

        if (info->process_id == pid)
        {
            return info->usage_cpu;
        }
    }
    
    return 0;
}

int main(int argc, char** argv)
{
    int seconds = 1;

    if (argc > 1)
    {
        sscanf(argv[1], "%d", &seconds);
    }

    ProcInfo procs[20];
    int proc_count = getprocs(procs, 20, 0);
    if (proc_count < 0)
    {
        printf("Not supported!\n");
        return 0;
    }

    ThreadInfo threads[20];
    int thread_count = getthreads(threads, 20, 0);

    printf("Process count: %d\n", proc_count);
    printf("PID NAME CPU(%%)\n");
    for (size_t i = 0; i < proc_count; i++)
    {
        ProcInfo* p = procs + i;

        uint32_t usage = get_process_cpu_usage(p->process_id, threads, thread_count);

        printf("%d %s %d\n", p->process_id, p->name, usage);
    }

    return 0;
}
