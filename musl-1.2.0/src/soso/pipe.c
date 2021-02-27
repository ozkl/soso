#include <stdint.h>
#include "syscall.h"
#include "soso.h"

int32_t manage_pipe(const char *pipe_name, int32_t operation, int32_t data)
{
    return __syscall(SYS_manage_pipe, pipe_name, operation, data);
}