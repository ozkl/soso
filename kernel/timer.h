#ifndef TIMER_H
#define TIMER_H

#include "common.h"
#include "time.h"

void timer_initialize();
uint32 get_system_tick_count();
uint64 get_system_tick_count64();
uint32 get_uptime_seconds();
uint64 get_uptime_seconds64();
uint32 get_uptime_milliseconds();
uint64 get_uptime_milliseconds64();
void scheduler_enable();
void scheduler_disable();

int32 clock_getres64(int32 clockid, struct timespec *res);
int32 clock_gettime64(int32 clockid, struct timespec *tp);
int32 clock_settime64(int32 clockid, const struct timespec *tp);

#endif
