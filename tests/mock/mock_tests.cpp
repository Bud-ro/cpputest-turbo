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

/* --- return values & data store (run first) ------------------------------ */

TEST_GROUP(MockReturn)
{
    TEST_TEARDOWN()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(MockReturn, allReturnTypes)
{
    mock().expectOneCall("b").andReturnValue(true);
    CHECK_EQUAL(true, mock().actualCall("b").returnBoolValue());

    mock().expectOneCall("i").andReturnValue(-7);
    LONGS_EQUAL(-7, mock().actualCall("i").returnIntValue());

    mock().expectOneCall("u").andReturnValue(9u);
    UNSIGNED_LONGS_EQUAL(9u, mock().actualCall("u").returnUnsignedIntValue());

    mock().expectOneCall("l").andReturnValue(-10L);
    LONGS_EQUAL(-10L, mock().actualCall("l").returnLongIntValue());

    mock().expectOneCall("ul").andReturnValue(11UL);
    UNSIGNED_LONGS_EQUAL(11UL, mock().actualCall("ul").returnUnsignedLongIntValue());

    mock().expectOneCall("ll").andReturnValue(1LL << 40);
    LONGLONGS_EQUAL(1LL << 40, mock().actualCall("ll").returnLongLongIntValue());

    mock().expectOneCall("d").andReturnValue(2.5);
    DOUBLES_EQUAL(2.5, mock().actualCall("d").returnDoubleValue(), 0.001);

    mock().expectOneCall("s").andReturnValue("hello");
    STRCMP_EQUAL("hello", mock().actualCall("s").returnStringValue());

    mock().expectOneCall("p").andReturnValue((void *)0x99);
    POINTERS_EQUAL((void *)0x99, mock().actualCall("p").returnPointerValue());

    mock().expectOneCall("cp").andReturnValue((const void *)0x77);
    POINTERS_EQUAL((const void *)0x77, mock().actualCall("cp").returnConstPointerValue());

    mock().expectOneCall("fp").andReturnValue((void (*)())0x55);
    FUNCTIONPOINTERS_EQUAL((void (*)())0x55, mock().actualCall("fp").returnFunctionPointerValue());
}

TEST(MockReturn, orDefault)
{
    mock().expectOneCall("noReturn");
    LONGS_EQUAL(42, mock().actualCall("noReturn").returnIntValueOrDefault(42));

    mock().expectOneCall("withReturn").andReturnValue(7);
    LONGS_EQUAL(7, mock().actualCall("withReturn").returnIntValueOrDefault(42));
}

TEST(MockReturn, supportLevelReturnValue)
{
    mock().expectOneCall("f").andReturnValue(13);
    mock().actualCall("f");
    CHECK(mock().hasReturnValue());
    LONGS_EQUAL(13, mock().intReturnValue());
    LONGS_EQUAL(13, mock().returnValue().getIntValue());
}

TEST(MockReturn, hasReturnValueFalse)
{
    mock().expectOneCall("f");
    mock().actualCall("f");
    CHECK_FALSE(mock().hasReturnValue());
    LONGS_EQUAL(5, mock().returnIntValueOrDefault(5));
}

/* --- output parameters (run first) ---------------------------------------- */

TEST_GROUP(MockOutput)
{
    TEST_TEARDOWN()
    {
        mock().clear();
    }
};

TEST(MockOutput, unexpectedOutputParameterName)
{
    mock().expectOneCall("f").withOutputParameterReturning("a", "x", 1);
    char sink = 0;
    mock().actualCall("f").withOutputParameter("wrong", &sink);
}

TEST(MockOutput, copiesBytesOnMatch)
{
    int provided = 42;
    int received = 0;
    mock().expectOneCall("f").withOutputParameterReturning("out", &provided,
                                                           sizeof provided);
    mock().actualCall("f").withOutputParameter("out", &received);
    LONGS_EQUAL(42, received);
    mock().checkExpectations();
}

TEST(MockOutput, unmodifiedOutputParameter)
{
    int received = 7;
    mock().expectOneCall("f").withUnmodifiedOutputParameter("out");
    mock().actualCall("f").withOutputParameter("out", &received);
    LONGS_EQUAL(7, received);
    mock().checkExpectations();
}

TEST(MockOutput, mixedWithInputParameters)
{
    const char *text = "hi";
    char buf[3] = { 0, 0, 0 };
    mock().expectOneCall("f")
        .withParameter("len", 3)
        .withOutputParameterReturning("buf", text, 3);
    mock().actualCall("f").withParameter("len", 3).withOutputParameter("buf", buf);
    STRCMP_EQUAL("hi", buf);
    mock().checkExpectations();
}

/* --- comparators & copiers (run first) ------------------------------------ */

struct Point
{
    int x;
    int y;
};

class PointComparator : public MockNamedValueComparator
{
public:
    bool isEqual(const void *o1, const void *o2) CPPUTEST_OVERRIDE
    {
        const Point *p1 = (const Point *)o1;
        const Point *p2 = (const Point *)o2;
        return p1->x == p2->x && p1->y == p2->y;
    }
    SimpleString valueToString(const void *o) CPPUTEST_OVERRIDE
    {
        const Point *p = (const Point *)o;
        return StringFromFormat("(%d, %d)", p->x, p->y);
    }
};

class PointCopier : public MockNamedValueCopier
{
public:
    void copy(void *out, const void *in) CPPUTEST_OVERRIDE
    {
        *(Point *)out = *(const Point *)in;
    }
};

TEST_GROUP(MockComparators)
{
    PointComparator comparator;
    PointCopier copier;

    TEST_TEARDOWN()
    {
        mock().clear();
        mock().removeAllComparatorsAndCopiers();
    }
};

TEST(MockComparators, customValueInFailureMessage)
{
    mock().installComparator("Point", comparator);
    Point expected = { 1, 2 };
    Point actual = { 3, 4 };
    mock().expectOneCall("draw").withParameterOfType("Point", "p", &expected);
    mock().actualCall("draw").withParameterOfType("Point", "p", &actual);
}

TEST(MockComparators, noWayToCompare)
{
    Point p = { 1, 2 };
    mock().expectOneCall("draw").withParameterOfType("Point", "p", &p);
    mock().actualCall("draw").withParameterOfType("Point", "p", &p);
}

TEST(MockComparators, comparatorMatches)
{
    mock().installComparator("Point", comparator);
    Point expected = { 1, 2 };
    Point actual = { 1, 2 };
    mock().expectOneCall("draw").withParameterOfType("Point", "p", &expected);
    mock().actualCall("draw").withParameterOfType("Point", "p", &actual);
    mock().checkExpectations();
}

TEST(MockComparators, copierFillsOutput)
{
    mock().installCopier("Point", copier);
    Point provided = { 9, 8 };
    Point received = { 0, 0 };
    mock().expectOneCall("get").withOutputParameterOfTypeReturning("Point", "p",
                                                                   &provided);
    mock().actualCall("get").withOutputParameterOfType("Point", "p", &received);
    LONGS_EQUAL(9, received.x);
    LONGS_EQUAL(8, received.y);
    mock().checkExpectations();
}

TEST(MockComparators, noWayToCopy)
{
    Point received;
    mock().expectOneCall("get").withOutputParameterOfTypeReturning("Point", "p",
                                                                   &received);
    mock().actualCall("get").withOutputParameterOfType("Point", "p", &received);
}

CPPUTEST_DEFAULT_MAIN
