#ifndef TIMER_H
#define TIMER_H

#include "common.h"

void initTimer(uint32 frequency);
uint32 getSystemTickCount();
void enableScheduler();
void disableScheduler();

#endif
