#ifndef TIMER_H
#define TIMER_H

#include "common.h"

struct timespec
{
    uint64 tv_sec;        /* seconds */
    uint32 tv_nsec;       /* nanoseconds */
};

void initializeTimer();
uint32 getSystemTickCount();
uint64 getSystemTickCount64();
uint32 getUptimeSeconds();
uint64 getUptimeSeconds64();
uint32 getUptimeMilliseconds();
uint64 getUptimeMilliseconds64();
void enableScheduler();
void disableScheduler();

int32 clock_getres64(int32 clockid, struct timespec *res);
int32 clock_gettime64(int32 clockid, struct timespec *tp);
int32 clock_settime64(int32 clockid, const struct timespec *tp);

#endif
