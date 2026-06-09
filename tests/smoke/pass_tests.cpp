#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

TEST_GROUP(Pass)
{
};

TEST(Pass, one)
{
    CHECK(1 == 1);
}

CPPUTEST_DEFAULT_MAIN
