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

/* --- parameters (these groups run FIRST: reverse declaration order) ------ */

TEST_GROUP(MockParamFail)
{
    TEST_TEARDOWN()
    {
        mock().clear();
    }
};

TEST(MockParamFail, missingParameter)
{
    mock().expectOneCall("f").withParameter("a", 1);
    mock().actualCall("f");
    mock().checkExpectations();
}

TEST(MockParamFail, unexpectedParameterValue)
{
    mock().expectOneCall("f").withParameter("a", 1);
    mock().actualCall("f").withParameter("a", 2);
}

TEST(MockParamFail, unexpectedParameterName)
{
    mock().expectOneCall("f").withParameter("a", 1);
    mock().actualCall("f").withParameter("b", 1);
}

TEST_GROUP(MockParamPass)
{
    TEST_TEARDOWN()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(MockParamPass, disambiguatesByValue)
{
    mock().expectOneCall("f").withParameter("a", 1);
    mock().expectOneCall("f").withParameter("a", 2);
    mock().actualCall("f").withParameter("a", 2);
    mock().actualCall("f").withParameter("a", 1);
}

TEST(MockParamPass, ignoreOtherParameters)
{
    mock().expectOneCall("f").withParameter("a", 1).ignoreOtherParameters();
    mock().actualCall("f").withParameter("a", 1).withParameter("b", 99)
        .withParameter("c", "extra");
}

TEST(MockParamPass, crossTypeIntegerMatch)
{
    mock().expectOneCall("f").withParameter("x", (unsigned long)5);
    mock().actualCall("f").withParameter("x", 5);
}

TEST(MockParamPass, doubleTolerance)
{
    mock().expectOneCall("f").withParameter("d", 1.0, 0.5);
    mock().actualCall("f").withParameter("d", 1.4);
}

TEST(MockParamPass, allTypes)
{
    const unsigned char buf[3] = { 1, 2, 3 };
    mock().expectOneCall("f")
        .withParameter("b", true)
        .withParameter("i", -3)
        .withParameter("u", 7u)
        .withParameter("l", -10L)
        .withParameter("ul", 11UL)
        .withParameter("ll", 1LL << 40)
        .withParameter("ull", 2ULL << 40)
        .withParameter("d", 2.5)
        .withParameter("s", "hello")
        .withParameter("p", (void *)0x12)
        .withParameter("cp", (const void *)0x34)
        .withParameter("fp", (void (*)())0x56)
        .withParameter("m", buf, sizeof buf);
    mock().actualCall("f")
        .withParameter("b", true)
        .withParameter("i", -3)
        .withParameter("u", 7u)
        .withParameter("l", -10L)
        .withParameter("ul", 11UL)
        .withParameter("ll", 1LL << 40)
        .withParameter("ull", 2ULL << 40)
        .withParameter("d", 2.5004)
        .withParameter("s", "hello")
        .withParameter("p", (void *)0x12)
        .withParameter("cp", (const void *)0x34)
        .withParameter("fp", (void (*)())0x56)
        .withParameter("m", buf, sizeof buf);
}

CPPUTEST_DEFAULT_MAIN
