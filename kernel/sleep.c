#include "sleep.h"
#include "timer.h"
#include "process.h"

void sleep_ms(Thread* thread, uint32 ms)
{
    uint32 uptime = get_uptime_milliseconds();

    //target uptime to wakeup
    uint32 target = uptime + ms;

    thread_change_state(thread, TS_SLEEP, (void*)target);

    enable_interrupts();

    halt();
}
