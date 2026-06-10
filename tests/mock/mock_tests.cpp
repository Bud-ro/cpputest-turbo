#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

/* Reverse declaration order: passing tests run first, then each failure
 * case pins its upstream-exact message in the golden file. */

TEST_GROUP(MockFail)
{
    TEST_TEARDOWN()
    {
        mock().clear();
    }
};

TEST(MockFail, outOfOrder)
{
    mock().strictOrder();
    mock().expectOneCall("first");
    mock().expectOneCall("second");
    mock().actualCall("second");
    mock().actualCall("first");
    mock().checkExpectations();
}

TEST(MockFail, expectNoCallViolated)
{
    mock().expectNoCall("never");
    mock().actualCall("never");
}

TEST(MockFail, expectedNotFulfilled)
{
    mock().expectOneCall("baz");
    mock().expectOneCall("baz");
    mock().actualCall("baz");
    mock().checkExpectations();
}

TEST(MockFail, unexpectedAdditionalCall)
{
    mock().expectOneCall("bar");
    mock().actualCall("bar");
    mock().actualCall("bar");
}

TEST(MockFail, unexpectedCall)
{
    mock().actualCall("foo");
}

TEST_GROUP(MockPass)
{
    TEST_TEARDOWN()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(MockPass, scopedCalls)
{
    mock("xmlparser").expectOneCall("open");
    mock("xmlparser").actualCall("open");
}

TEST(MockPass, disabledMockIgnoresEverything)
{
    mock().disable();
    mock().expectOneCall("notRecorded");
    mock().actualCall("alsoNotRecorded");
    mock().enable();
}

TEST(MockPass, ignoreOtherCalls)
{
    mock().expectOneCall("wanted");
    mock().ignoreOtherCalls();
    mock().actualCall("wanted");
    mock().actualCall("unwanted");
    mock().actualCall("alsoUnwanted");
}

TEST(MockPass, strictOrderRightOrder)
{
    mock().strictOrder();
    mock().expectOneCall("first");
    mock().expectOneCall("second");
    mock().actualCall("first");
    mock().actualCall("second");
}

TEST(MockPass, expectedCallsLeft)
{
    mock().expectOneCall("pending");
    CHECK(mock().expectedCallsLeft());
    mock().actualCall("pending");
    CHECK_FALSE(mock().expectedCallsLeft());
}

TEST(MockPass, nCalls)
{
    mock().expectNCalls(2, "twice");
    mock().actualCall("twice");
    mock().actualCall("twice");
}

TEST(MockPass, oneCall)
{
    mock().expectOneCall("foo");
    mock().actualCall("foo");
}

CPPUTEST_DEFAULT_MAIN
