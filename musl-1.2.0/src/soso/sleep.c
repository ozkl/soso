#include <stdint.h>
#include "syscall.h"
#include "soso.h"

void sleep_ms(uint32_t ms)
{
    __syscall(SYS_sleep_ms, ms);
}