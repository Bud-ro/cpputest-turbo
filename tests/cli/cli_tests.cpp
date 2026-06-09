#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

/* Execution order is reverse declaration order:
 * GroupB.ig, GroupB.b1, GroupA.a2, GroupA.a1 */

TEST_GROUP(GroupA)
{
};

TEST(GroupA, a1)
{
    CHECK(1 == 1);
}

TEST(GroupA, a2)
{
    CHECK(2 == 2);
}

TEST_GROUP(GroupB)
{
};

TEST(GroupB, b1)
{
    CHECK(3 == 3);
}

IGNORE_TEST(GroupB, ig)
{
    FAIL("only fails when run with -ri");
}

CPPUTEST_DEFAULT_MAIN
