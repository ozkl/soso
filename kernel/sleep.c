#include "sleep.h"
#include "timer.h"
#include "process.h"

void sleep_ms(Thread* thread, uint32_t ms)
{
    uint32_t uptime = get_uptime_milliseconds();

    //target uptime to wakeup
    uint32_t target = uptime + ms;

    thread_change_state(thread, TS_SLEEP, (void*)target);

    while (thread->state == TS_SLEEP)
    {
        enable_interrupts();

        halt();
    }
}
