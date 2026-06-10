#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/OrderedTest.h"

TEST_GROUP(Ordered)
{
};

/* declared out of level order on purpose */
TEST_ORDERED(Ordered, third, 3)
{
    CHECK(true);
}

TEST_ORDERED(Ordered, first, 1)
{
    CHECK(true);
}

TEST_ORDERED(Ordered, fifth, 5)
{
    CHECK(true);
}

TEST_ORDERED(Ordered, second, 2)
{
    CHECK(true);
}

TEST_ORDERED(Ordered, fourth, 4)
{
    CHECK(true);
}

CPPUTEST_DEFAULT_MAIN
