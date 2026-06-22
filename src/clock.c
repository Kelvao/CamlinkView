#define _XOPEN_SOURCE 700
#include "common.h"
#include <time.h>

uint64_t
mono_ns (void)
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
