#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTest/CommandLineTestRunner.h"

TEST_GROUP_C_WRAPPER(MockC)
{
    TEST_GROUP_C_SETUP_WRAPPER(MockC)
    TEST_GROUP_C_TEARDOWN_WRAPPER(MockC)
};

TEST_C_WRAPPER(MockC, expectAndActual)
TEST_C_WRAPPER(MockC, returnDefault)
TEST_C_WRAPPER(MockC, outputParameter)
TEST_C_WRAPPER(MockC, dataStore)
TEST_C_WRAPPER(MockC, scopes)
TEST_C_WRAPPER(MockC, supportLevelReturn)

CPPUTEST_DEFAULT_MAIN
