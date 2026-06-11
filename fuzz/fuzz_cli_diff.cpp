/* Differential CLI/runner fuzzer — the FIXTURE half.
 *
 * A small fixed suite (passing, failing, ignored tests across three groups,
 * plus a mock-using test) compiled against upstream CppUTest and against
 * cpputest-turbo. scripts/check-fuzz.sh generates seeded random command-line
 * flag combinations (-v, -ri, -g/-n filters, strict/exclude variants) and
 * runs both binaries with identical argv in separate scratch dirs, diffing
 * stdout and exit code.
 *
 * Everything here must be deterministic: failure messages use literals. */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

TEST_GROUP(GroupA){};

TEST(GroupA, pass)
{
    LONGS_EQUAL(1, 1);
    STRCMP_EQUAL("same", "same");
}

TEST(GroupA, fails)
{
    CHECK_TEXT(1 == 1, "fine");
    LONGS_EQUAL(7, 9);
}

TEST(GroupA, alsoPasses)
{
    DOUBLES_EQUAL(2.5, 2.5, 0.001);
}

TEST_GROUP(GroupB){};

IGNORE_TEST(GroupB, ignored)
{
    FAIL("never runs unless -ri");
}

TEST(GroupB, pass)
{
    CHECK(true);
    CHECK_COMPARE(3, >, 2);
}

TEST_GROUP(GroupC){TEST_TEARDOWN(){mock().clear();
}
}
;

TEST(GroupC, mockPass)
{
    mock().expectOneCall("fn").withParameter("x", 4).andReturnValue(8);
    LONGS_EQUAL(8,
                mock().actualCall("fn").withParameter("x", 4).returnIntValue());
    mock().checkExpectations();
}

TEST(GroupC, mockFails)
{
    mock().expectOneCall("wanted");
    mock().checkExpectations();
}

int main(int argc, char **argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
