/* clock_gettime needs POSIX visibility under strict -std=c11 */
#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void *checked(void *p)
{
    if (!p) {
        fputs("\ncpputest: out of memory in test framework internals\n",
              stderr);
        abort();
    }
    return p;
}

void *cu_xmalloc(size_t size)
{
    return checked(malloc(size));
}

void *cu_xcalloc(size_t count, size_t size)
{
    return checked(calloc(count, size));
}

void *cu_xrealloc(void *ptr, size_t size)
{
    return checked(realloc(ptr, size));
}

char *cu_xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    return memcpy(cu_xmalloc(n), s, n);
}

size_t cu_time_in_millis(void)
{
    struct timespec ts;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
        return 0;
    return (size_t)ts.tv_sec * 1000u + (size_t)(ts.tv_nsec / 1000000L);
}
