#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <string.h>

/* Used by tests/process/run.sh:
 * - default run skips the crashing/failing tests via filters
 * - -p runs include them to prove fork containment */

TEST_GROUP(StableA)
{
};

TEST(StableA, one)
{
    CHECK(true);
}

TEST(StableA, two)
{
    LONGS_EQUAL(4, 2 + 2);
}

TEST_GROUP(StableB)
{
};

TEST(StableB, one)
{
    STRCMP_EQUAL("x", "x");
}

TEST_GROUP(StableC)
{
};

TEST(StableC, one)
{
    CHECK(true);
}

TEST_GROUP(StableD)
{
};

TEST(StableD, one)
{
    CHECK(true);
}

TEST_GROUP(Hazard)
{
};

TEST(Hazard, crashes)
{
    /* deliberate crash: contained by -p, fatal without it */
    volatile int *bad = (volatile int *)0;
    *bad = 42; /* SIGSEGV */
}

TEST(Hazard, fails)
{
    FAIL("plain failure inside forked child");
}

CPPUTEST_DEFAULT_MAIN
