/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "process.h"
#include "timer.h"
#include "common.h"
#include "errno.h"
#include "log.h"
#include "isr.h"
#include "syscall_select.h"

#define POLLIN   0x001
#define POLLOUT  0x004
#define POLLNVAL 0x020

void select_update(Thread* thread)
{
    Process* process = thread->owner;

    int total_ready = 0;
    uint32_t count = (uint32_t)thread->select.nfds;
    count = MIN(count, SOSO_MAX_OPENED_FILES);

    for (uint32_t fd = 0; fd < count; ++fd)
    {
        File* file = process_get_file(process, fd);

        if (file)
        {
            if (FD_ISSET(fd, &thread->select.read_set))
            {
                if (file->node->read_test_ready && file->node->read_test_ready(file))
                {
                    FD_SET(fd, &thread->select.read_set_result);

                    ++total_ready;
                }
            }

            if (FD_ISSET(fd, &thread->select.write_set))
            {
                if (file->node->write_test_ready && file->node->write_test_ready(file))
                {
                    FD_SET(fd, &thread->select.write_set_result);

                    ++total_ready;
                }
            }
        }
    }

    if (total_ready > 0)
    {
        thread->select.result = total_ready;
        thread->select.select_state = SS_FINISHED;
    }
    else if (thread->select.target_time > 0)
    {
        time_t now = get_uptime_milliseconds64();

        if (now > thread->select.target_time)
        {
            thread->select.result = 0;
            thread->select.select_state = SS_FINISHED;
        }
    }
}

static int select_finish(Thread* thread, fd_set* rfds, fd_set* wfds)
{
    if (rfds)
    {
        *rfds = thread->select.read_set_result;
    }

    if (wfds)
    {
        *wfds = thread->select.write_set_result;
    }

    int result = thread->select.result;
    memset((uint8_t*)&thread->select, 0, sizeof(thread->select));

    return result;
}

int syscall_select(int n, fd_set* rfds, fd_set* wfds, fd_set* efds, struct timeval* tv)
{
    if (!check_user_access(rfds))
    {
        return -EFAULT;
    }

    if (!check_user_access(wfds))
    {
        return -EFAULT;
    }

    if (!check_user_access(efds))
    {
        return -EFAULT;
    }

    if (!check_user_access(tv))
    {
        return -EFAULT;
    }

    Thread* thread = thread_get_current();
    Process* process = thread->owner;

    if (process)
    {
        thread->select.select_state = SS_STARTED;
        thread->select.nfds = n;
        FD_ZERO(&thread->select.read_set_result);
        FD_ZERO(&thread->select.write_set_result);

        FD_ZERO(&thread->select.read_set);
        if (rfds)
        {
            thread->select.read_set = *rfds;
            FD_ZERO(rfds);
        }

        FD_ZERO(&thread->select.write_set);
        if (wfds)
        {
            thread->select.write_set = *wfds;
            FD_ZERO(wfds);
        }

        thread->select.result = -1;

        thread->select.target_time = 0;
        if (tv)
        {
            thread->select.target_time = get_uptime_milliseconds64() + tv->tv_sec * 1000 + tv->tv_usec / 1000;
        }

        while (TRUE)
        {
            disable_interrupts();

            select_update(thread);

            if (thread->select.select_state == SS_FINISHED)
            {
                int result = select_finish(thread, rfds, wfds);

                thread_resume(thread);

                return result;
            }

            thread_change_state(thread, TS_SELECT, NULL);
            enable_interrupts();
            halt();
        }
    }

    return -1;
}

int32_t syscall_poll(struct pollfd* fds, uint32_t nfds, int32_t timeout)
{
    if (nfds > SOSO_MAX_OPENED_FILES)
    {
        return -EINVAL;
    }

    if (nfds > 0)
    {
        if (!check_user_access(fds) || !check_user_access(fds + nfds - 1))
        {
            return -EFAULT;
        }
    }

    Process* process = thread_get_current()->owner;
    time_t target_time = 0;
    if (timeout > 0)
    {
        target_time = get_uptime_milliseconds64() + timeout;
    }

    while (TRUE)
    {
        disable_interrupts();

        int32_t ready_count = 0;
        for (uint32_t i = 0; i < nfds; ++i)
        {
            struct pollfd* poll_fd = &fds[i];
            poll_fd->revents = 0;

            if (poll_fd->fd < 0)
            {
                continue;
            }

            if (poll_fd->fd >= SOSO_MAX_OPENED_FILES ||
                process_get_file(process, poll_fd->fd) == NULL)
            {
                poll_fd->revents = POLLNVAL;
                ++ready_count;
                continue;
            }

            File* file = process_get_file(process, poll_fd->fd);
            if (poll_fd->events & POLLIN)
            {
                if (file->node->read_test_ready == NULL ||
                    file->node->read_test_ready(file))
                {
                    poll_fd->revents |= POLLIN;
                }
            }

            if (poll_fd->events & POLLOUT)
            {
                if (file->node->write_test_ready == NULL ||
                    file->node->write_test_ready(file))
                {
                    poll_fd->revents |= POLLOUT;
                }
            }

            if (poll_fd->revents != 0)
            {
                ++ready_count;
            }
        }

        if (ready_count > 0 || timeout == 0)
        {
            return ready_count;
        }

        if (timeout > 0 && get_uptime_milliseconds64() >= target_time)
        {
            return 0;
        }

        if (thread_get_current()->pending_signal_count > 0)
        {
            return -EINTR;
        }

        enable_interrupts();
        halt();
    }

    return -1;
}
