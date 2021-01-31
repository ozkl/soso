#ifndef SYSCALL_GETTHREADS_H
#define SYSCALL_GETTHREADS_H

#include "stdint.h"
#include "process.h"

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

int32_t syscall_getthreads(ThreadInfo* threads, uint32_t max_count, uint32_t flags);
int32_t syscall_getprocs(ProcInfo* procs, uint32_t max_count, uint32_t flags);

#endif //SYSCALL_GETTHREADS_H