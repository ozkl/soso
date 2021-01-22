#ifndef TIME_H
#define TIME_H

#include "common.h"

typedef uint64_t time_t;

typedef int64_t suseconds_t;

struct timespec
{
    time_t tv_sec;        /* seconds */
    uint32_t tv_nsec;       /* nanoseconds */
};

struct timeval
{
    uint32_t tv_sec;         /* seconds */
    uint32_t tv_usec;        /* microseconds */
};

#endif //TIME_H