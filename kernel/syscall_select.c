#include "process.h"
#include "timer.h"
#include "syscall_select.h"

int syscall_select(int n, fd_set* rfds, fd_set* wfds, fd_set* efds, struct timeval* tv)
{
    enableInterrupts();

    fd_set rfdsOriginal;
    FD_ZERO(&rfdsOriginal);
    if (rfds)
    {
        rfdsOriginal = *rfds;
        FD_ZERO(rfds);
    }

    fd_set wfdsOriginal;
    FD_ZERO(&wfdsOriginal);
    if (wfds)
    {
        wfdsOriginal = *wfds;
        FD_ZERO(wfds);
    }

    Process* process = getCurrentThread()->owner;
    if (process)
    {
        time_t targetTime = 0;
        if (tv)
        {
            targetTime = getUptimeMilliseconds64() + tv->tv_sec * 1000 + tv->tv_usec / 1000;
        }

        while (TRUE)
        {
            int totalReady = 0;
            uint32 count = (uint32)n;
            count = MIN(count, MAX_OPENED_FILES);
            for (uint32 fd = 0; fd < count; ++fd)
            {
                File* file = process->fd[fd];

                if (file)
                {
                    if (rfds)
                    {
                        if (FD_ISSET(fd, &rfdsOriginal))
                        {
                            if (file->node->readTestReady)
                            {
                                if (file->node->readTestReady(file))
                                {
                                    FD_SET(fd, rfds);

                                    ++totalReady;
                                }
                            }
                        }
                    }

                    if (wfds)
                    {
                        if (FD_ISSET(fd, &wfdsOriginal))
                        {
                            if (file->node->writeTestReady)
                            {
                                if (file->node->writeTestReady(file))
                                {
                                    FD_SET(fd, wfds);

                                    ++totalReady;
                                }
                            }
                        }
                    }
                }
            }

            if (totalReady > 0)
            {
                return totalReady;
            }

            if (tv)
            {
                time_t now = getUptimeMilliseconds64();

                if (now > targetTime)
                {
                    return 0;
                }
            }

            //TODO: make better to prevent wasting cpu cycles by cheking these in scheduler even before switching to this thread!
            halt();
        }
    }

    return -1;
}