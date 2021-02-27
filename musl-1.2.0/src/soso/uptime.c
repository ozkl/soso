#include <stdint.h>
#include "syscall.h"
#include "soso.h"

uint32_t get_uptime_ms()
{
    return __syscall(SYS_get_uptime_ms);
}