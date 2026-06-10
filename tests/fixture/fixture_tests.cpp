#include "CppUTest/TestHarness.h"
#include "CppUTest/TestTestingFixture.h"
#include "CppUTest/CommandLineTestRunner.h"

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

CPPUTEST_DEFAULT_MAIN
