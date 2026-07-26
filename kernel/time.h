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

struct timespec32
{
    int32_t tv_sec;
    int32_t tv_nsec;
};

struct timeval
{
    uint32_t tv_sec;         /* seconds */
    uint32_t tv_usec;        /* microseconds */
};

struct timeval32
{
    int32_t tv_sec;
    int32_t tv_usec;
};

struct timeval64
{
    int64_t tv_sec;
    int64_t tv_usec;
};

#endif //TIME_H
