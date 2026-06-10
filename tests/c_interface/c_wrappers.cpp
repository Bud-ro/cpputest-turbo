#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTest/CommandLineTestRunner.h"

TEST_GROUP_C_WRAPPER(CGroup)
{
    TEST_GROUP_C_SETUP_WRAPPER(CGroup)
    TEST_GROUP_C_TEARDOWN_WRAPPER(CGroup)
};

/* reverse declaration order: ignored, passes, fails */
TEST_C_WRAPPER(CGroup, fails)
TEST_C_WRAPPER(CGroup, passes)
IGNORE_TEST_C_WRAPPER(CGroup, ignored)

CPPUTEST_DEFAULT_MAIN
