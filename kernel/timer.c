#include "timer.h"
#include "isr.h"
#include "process.h"
#include "common.h"

#define TIMER_FREQ 1000

uint64 gSystemTickCount = 0;

uint64 gSystemDateMs = 0;

BOOL gSchedulerEnabled = FALSE;

//called from assembly
void handleTimerIRQ(TimerInt_Registers registers)
{
    gSystemTickCount++;

    gSystemDateMs++;

    if (gSchedulerEnabled == TRUE)
    {
        schedule(&registers);
    }
}

uint32 getSystemTickCount()
{
    return (uint32)gSystemTickCount;
}

uint64 getSystemTickCount64()
{
    return gSystemTickCount;
}

uint32 getUptimeSeconds()
{
    return ((uint32)gSystemTickCount) / TIMER_FREQ;
}

uint64 getUptimeSeconds64()
{
    //TODO: make this real 64 bit
    return getUptimeSeconds();
}

uint32 getUptimeMilliseconds()
{
    return (uint32)gSystemTickCount;
}

uint64 getUptimeMilliseconds64()
{
    return gSystemTickCount;
}

void enableScheduler()
{
    gSchedulerEnabled = TRUE;
}

void disableScheduler()
{
    gSchedulerEnabled = FALSE;
}

static void initTimer(uint32 frequency)
{
    uint32 divisor = 1193180 / frequency;

    outb(0x43, 0x36);

    uint8 l = (uint8)(divisor & 0xFF);
    uint8 h = (uint8)( (divisor>>8) & 0xFF );

    outb(0x40, l);
    outb(0x40, h);
}

void initializeTimer()
{
    initTimer(TIMER_FREQ);
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

    uint32 uptimeMilli = gSystemDateMs;

    tp->tv_sec = uptimeMilli / TIMER_FREQ;

    uint32 extraMilli = uptimeMilli - tp->tv_sec * 1000;

    tp->tv_nsec = extraMilli * 1000000;

    return 0;
}

int32 clock_settime64(int32 clockid, const struct timespec *tp)
{
    //TODO: clockid

    //TODO: make use of tv_nsec

    uint32 uptimeMilli = gSystemDateMs;

    gSystemDateMs = tp->tv_sec * TIMER_FREQ;

    return 0;
}