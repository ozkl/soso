#ifndef TIME_H
#define TIME_H

#include "common.h"

typedef uint64 time_t;

struct timespec
{
    time_t tv_sec;        /* seconds */
    uint32 tv_nsec;       /* nanoseconds */
};

struct timeval
{
    time_t tv_sec;         /* seconds */
    uint32 tv_usec;        /* microseconds */
};

#endif //TIME_H