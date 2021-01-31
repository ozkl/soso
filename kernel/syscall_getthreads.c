#include "process.h"
#include "common.h"
#include "errno.h"
#include "syscall_getthreads.h"


int32_t syscall_getthreads(ThreadInfo* threads, uint32_t max_count, uint32_t flags)
{
    if (!check_user_access(threads))
    {
        return -EFAULT;
    }

    //interrupts are disabled

    memset((uint8_t*)threads, 0, sizeof(ThreadInfo) * max_count);

    ThreadInfo* info = threads;

    Thread* t = thread_get_first();
    uint32_t i = 0;
    while (t && i < max_count)
    {
        info->thread_id = t->threadId;
        info->process_id = t->owner->pid;
        info->state = t->state;
        info->user_mode = t->user_mode;
        info->birth_time = t->birth_time;
        info->context_switch_count = t->context_switch_count;
        info->context_start_time = t->context_start_time;
        info->context_end_time = t->context_end_time;
        info->consumed_cpu_time_ms = t->consumed_cpu_time_ms;
        info->usage_cpu = t->usage_cpu;
        info->called_syscall_count = t->called_syscall_count;

        t = t->next;
        i++;
        info++;
    }

    return i;
}


static BOOL exists_in_info_list(uint32_t pid, ProcInfo* procs, uint32_t count)
{
    ProcInfo* info = procs;

    uint32_t i = 0;

    while (i < count)
    {
        if (info->process_id == pid)
        {
            return TRUE;
        }
        i++;
        info++;
    }

    return FALSE;
}

int32_t syscall_getprocs(ProcInfo* procs, uint32_t max_count, uint32_t flags)
{
    if (!check_user_access(procs))
    {
        return -EFAULT;
    }

    //interrupts are disabled

    memset((uint8_t*)procs, 0, sizeof(ProcInfo) * max_count);

    ProcInfo* info = procs;

    Thread* t = thread_get_first();
    uint32_t i = 0;
    uint32_t process_count = 0;
    while (t && i < max_count)
    {
        Process* process = t->owner;

        if (!exists_in_info_list(process->pid, procs, process_count))
        {
            info->process_id = process->pid;
            info->parent_process_id = -1;
            if (process->parent)
            {
                info->parent_process_id = process->parent->pid;
            }

            for (uint32_t k = 0; k < SOSO_MAX_OPENED_FILES; ++k)
            {
                File* file = process->fd[k];
                if (file && file->node)
                {
                    info->fd[k] = file->node->node_type;
                }
            }

            memcpy((uint8_t*)info->name, process->name, SOSO_PROCESS_NAME_MAX);

            //TODO: tty, working_directory

            info++;
            process_count++;
        }
        

        t = t->next;
        i++;
    }

    return process_count;
}
