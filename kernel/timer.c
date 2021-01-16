#include "timer.h"
#include "isr.h"
#include "process.h"
#include "common.h"

#define TIMER_FREQ 1000

uint64 g_system_tick_count = 0;

uint64 g_system_date_ms = 0;

BOOL g_scheduler_enabled = FALSE;

//called from assembly
void handleTimerIRQ(TimerInt_Registers registers)
{
    g_system_tick_count++;

    g_system_date_ms++;

    if (g_scheduler_enabled == TRUE)
    {
        schedule(&registers);
    }
}

uint32 get_system_tick_count()
{
    return (uint32)g_system_tick_count;
}

uint64 get_system_tick_count64()
{
    return g_system_tick_count;
}

uint32 get_uptime_seconds()
{
    return ((uint32)g_system_tick_count) / TIMER_FREQ;
}

uint64 get_uptime_seconds64()
{
    //TODO: make this real 64 bit
    return get_uptime_seconds();
}

uint32 get_uptime_milliseconds()
{
    return (uint32)g_system_tick_count;
}

uint64 get_uptime_milliseconds64()
{
    return g_system_tick_count;
}

void scheduler_enable()
{
    g_scheduler_enabled = TRUE;
}

void scheduler_disable()
{
    g_scheduler_enabled = FALSE;
}

static void timer_init(uint32 frequency)
{
    uint32 divisor = 1193180 / frequency;

    outb(0x43, 0x36);

    uint8 l = (uint8)(divisor & 0xFF);
    uint8 h = (uint8)( (divisor>>8) & 0xFF );

    outb(0x40, l);
    outb(0x40, h);
}

void timer_initialize()
{
    timer_init(TIMER_FREQ);
}

int32 clock_getres64(int32 clockid, struct timespec *res)
{
    res->tv_sec = 0;
    res->tv_nsec = 1000000;

    return 0;
}

int32 clock_gettime64(int32 clockid, struct timespec *tp)
{
    //TODO: clockid

    //TODO: make proper use of 64 bit fields

    uint32 uptime_milli = g_system_date_ms;

    tp->tv_sec = uptime_milli / TIMER_FREQ;

    uint32 extra_milli = uptime_milli - tp->tv_sec * 1000;

    tp->tv_nsec = extra_milli * 1000000;

    return 0;
}

int32 clock_settime64(int32 clockid, const struct timespec *tp)
{
    //TODO: clockid

    //TODO: make use of tv_nsec

    uint32 uptime_milli = g_system_date_ms;

    g_system_date_ms = tp->tv_sec * TIMER_FREQ;

    return 0;
}