#ifndef TIMER_H
#define TIMER_H

#include "common.h"
#include "time.h"

extern uint64_t g_system_tick_count;

void timer_initialize();
uint32_t get_system_tick_count();
uint64_t get_system_tick_count64();
uint32_t get_uptime_seconds();
uint64_t get_uptime_seconds64();
uint32_t get_uptime_milliseconds();
uint64_t get_uptime_milliseconds64();
void scheduler_enable();
void scheduler_disable();

int32_t clock_getres64(int32_t clockid, struct timespec *res);
int32_t clock_gettime64(int32_t clockid, struct timespec *tp);
int32_t clock_settime64(int32_t clockid, const struct timespec *tp);

#endif
