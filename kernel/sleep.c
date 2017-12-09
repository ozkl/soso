#include "sleep.h"
#include "timer.h"

void sleep(uint32 ticks)
{
    uint32 systemTicks = getSystemTickCount();

    uint32 target = systemTicks + ticks;

    while (target >= getSystemTickCount())
    {
        halt();
    }
}
