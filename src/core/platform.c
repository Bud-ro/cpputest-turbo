/* clock_gettime needs POSIX visibility under strict -std=c11 */
#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <time.h>

size_t cu_time_in_millis(void)
{
    struct timespec ts;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
        return 0;
    return (size_t)ts.tv_sec * 1000u + (size_t)(ts.tv_nsec / 1000000L);
}
