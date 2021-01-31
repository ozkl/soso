#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <soso.h>

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
