#ifndef TIMER_H
#define TIMER_H

#include "common.h"

void initializeTimer();
uint32 getSystemTickCount();
uint32 getUptimeSeconds();
void enableScheduler();
void disableScheduler();

#endif
