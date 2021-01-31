#include <stdint.h>
#include "syscall.h"
#include "soso.h"


int32_t getthreads(ThreadInfo* threads, uint32_t max_count, uint32_t flags)
{
    return __syscall(SYS_getthreads, threads, max_count, flags);
}

int32_t getprocs(ProcInfo* procs, uint32_t max_count, uint32_t flags)
{
    return __syscall(SYS_getprocs, procs, max_count, flags);
}