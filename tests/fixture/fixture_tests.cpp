#include "CppUTest/TestHarness.h"
#include "CppUTest/TestTestingFixture.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <string.h>

static bool setupCalled = false;

static void doSetup()
{
    setupCalled = true;
}

static void failMethod()
{
    FAIL("deliberate inner failure");
}

static void passMethod()
{
    CHECK(true);
}

TEST_GROUP(Fixture)
{
};

TEST(Fixture, passingRun)
{
    TestTestingFixture fixture;
    fixture.runTestWithMethod(passMethod);
    LONGS_EQUAL(0, fixture.getFailureCount());
    LONGS_EQUAL(1, fixture.getRunCount());
    LONGS_EQUAL(1, fixture.getCheckCount());
    CHECK_FALSE(fixture.hasTestFailed());
    fixture.assertPrintContains("OK (1 tests");
}

TEST(Fixture, capturesInnerFailure)
{
    setupCalled = false;
    TestTestingFixture fixture;
    fixture.setSetup(doSetup);
    fixture.runTestWithMethod(failMethod);
    LONGS_EQUAL(1, fixture.getFailureCount());
    CHECK(setupCalled);
    CHECK(fixture.hasTestFailed());
    fixture.assertPrintContains("deliberate inner failure");
    fixture.assertPrintContains("Failure in TEST(Generic, Generic Test)");
    fixture.assertPrintContainsNot("no such text in the output");
    fixture.assertPrintContains("Errors (1 failures, 1 tests");
}

/* regression: failure messages longer than the output path's stack buffer
 * (1 KiB) must not be truncated — found by the differential mock fuzzer */
static char long_message[3000];

static void failWithLongMessage()
{
    memset(long_message, 'A', sizeof long_message);
    memcpy(long_message + sizeof long_message - 8, "MARKER", 7);
    long_message[sizeof long_message - 1] = '\0';
    FAIL(long_message);
}

TEST(Fixture, longFailureMessageNotTruncated)
{
    TestTestingFixture fixture;
    fixture.runTestWithMethod(failWithLongMessage);
    LONGS_EQUAL(1, fixture.getFailureCount());
    fixture.assertPrintContains("MARKER");
}

CPPUTEST_DEFAULT_MAIN
