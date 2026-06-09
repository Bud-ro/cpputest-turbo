#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

/* Registration prepends, so execution order is the REVERSE of declaration
 * order in this file — the golden files depend on that upstream behavior. */

TEST_GROUP(Smoke)
{
    int x;

    TEST_SETUP()
    {
        x = 41;
    }

    TEST_TEARDOWN()
    {
    }
};

TEST(Smoke, setupRan)
{
    x++;
    CHECK(x == 42);
}

TEST(Smoke, checkText)
{
    CHECK_TEXT(x == 41, "setup should have run");
    CHECK_TRUE(x > 0);
    CHECK_FALSE(x < 0);
}

IGNORE_TEST(Smoke, notRunYet)
{
    FAIL("must never execute");
}

TEST_GROUP(Failing)
{
};

TEST(Failing, deliberateFailure)
{
    FAIL("deliberate failure"); /* golden output pins this block's format */
}

TEST(Failing, deliberateCheckFailure)
{
    CHECK_TEXT(1 == 2, "one is not two");
}

CPPUTEST_DEFAULT_MAIN
