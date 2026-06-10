#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <string.h>
#include <stdlib.h>

/* Reverse declaration order: execution is leaksNewArray, leaksMalloc,
 * ignored, expected, freeUntracked, mismatch, corruption. */

TEST_GROUP(Leaks)
{
};

TEST(Leaks, corruption)
{
    char *p = (char *)cpputest_malloc(4);
    p[4] = 'X'; /* stomp the guard bytes */
    cpputest_free(p); /* reports corruption and aborts the test */
}

TEST(Leaks, mismatch)
{
    char *p = (char *)cpputest_malloc(4);
    delete p; /* malloc'd but deleted: type mismatch, aborts the test */
}

TEST(Leaks, freeUntracked)
{
    void *p = malloc(4); /* raw libc malloc: never tracked */
    cpputest_free(p); /* reported as deallocating non-allocated memory */
    (void)p;
}

TEST(Leaks, expected)
{
    EXPECT_N_LEAKS(1);
    char *p = new char[3];
    memcpy(p, "ok", 3);
}

TEST(Leaks, ignored)
{
    IGNORE_ALL_LEAKS_IN_TEST();
    char *p = new char[3];
    memcpy(p, "ig", 3);
}

TEST(Leaks, leaksMalloc)
{
    char *p = (char *)cpputest_malloc(5);
    memcpy(p, "WXYZ", 5);
}

TEST(Leaks, leaksNewArray)
{
    char *p = new char[5];
    memcpy(p, "ABCD", 5);
}

CPPUTEST_DEFAULT_MAIN
