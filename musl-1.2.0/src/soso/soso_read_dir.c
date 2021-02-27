#include <stdint.h>
#include "syscall.h"
#include "soso.h"

int32_t soso_read_dir(int32_t fd, void *dirent, int32_t index)
{
    return __syscall(SYS_soso_read_dir, fd, dirent, index);
}