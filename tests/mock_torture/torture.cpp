/* Mock torture suite — deliberately nasty usage patterns that target the
 * matching state machine's corners. Like the differential fuzzer, this file
 * uses ONLY public API and is compiled against BOTH upstream CppUTest and
 * cpputest-revibed; outputs must be byte-identical (ms-normalized).
 *
 * Every pointer printed in a failure message is a fixed fake constant so
 * outputs are deterministic across builds. */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupport_c.h"

#include <string.h>

TEST_GROUP(Torture)
{
    TEST_TEARDOWN()
    {
        mock().clear();
        mock().removeAllComparatorsAndCopiers();
    }
};

/* ---- expectation preference: exact match beats ignoreOtherParameters ---- */
TEST(Torture, exactMatchPreferredOverIgnoreOthers)
{
    mock().expectOneCall("f").withParameter("a", 1).ignoreOtherParameters()
        .andReturnValue(111);
    mock().expectOneCall("f").withParameter("a", 1).withParameter("b", 2)
        .andReturnValue(222);
    /* both match; the non-ignoring expectation should be claimed first */
    int r = mock().actualCall("f").withParameter("a", 1).withParameter("b", 2)
                .returnIntValueOrDefault(0);
    LONGS_EQUAL(222, r);
    /* remaining call matches the ignoring one */
    int r2 = mock().actualCall("f").withParameter("a", 1).withParameter("b", 9)
                 .returnIntValueOrDefault(0);
    LONGS_EQUAL(111, r2);
    mock().checkExpectations();
}

/* ---- match reopening: a claimed expectation is released by a new param --- */
TEST(Torture, matchReopensWhenMoreParametersArrive)
{
    mock().expectOneCall("f").withParameter("a", 1);
    mock().expectOneCall("f").withParameter("a", 1).withParameter("b", 2);
    /* after withParameter(a) the first expectation matches; withParameter(b)
     * must reopen and land on the second */
    mock().actualCall("f").withParameter("a", 1).withParameter("b", 2);
    /* the single-param expectation is still open */
    mock().actualCall("f").withParameter("a", 1);
    mock().checkExpectations();
}

/* ---- same value disambiguation across N expectations ------------------- */
TEST(Torture, nCallsInterleavedWithOneCalls)
{
    mock().expectNCalls(2, "f").withParameter("k", 1).andReturnValue(10);
    mock().expectOneCall("f").withParameter("k", 2).andReturnValue(20);
    LONGS_EQUAL(10, mock().actualCall("f").withParameter("k", 1).returnIntValueOrDefault(0));
    LONGS_EQUAL(20, mock().actualCall("f").withParameter("k", 2).returnIntValueOrDefault(0));
    LONGS_EQUAL(10, mock().actualCall("f").withParameter("k", 1).returnIntValueOrDefault(0));
    mock().checkExpectations();
}

/* ---- strict order violated across two scopes (per-scope counters) ------- */
TEST(Torture, strictOrderPerScope)
{
    mock().strictOrder();
    mock("dev").strictOrder();
    mock().expectOneCall("a");
    mock().expectOneCall("b");
    mock("dev").expectOneCall("x");
    mock("dev").expectOneCall("y");
    /* in-order within each scope, interleaved between scopes: fine */
    mock().actualCall("a");
    mock("dev").actualCall("x");
    mock().actualCall("b");
    mock("dev").actualCall("y");
    mock().checkExpectations();
}

/* ---- strict order violation message ------------------------------------ */
TEST(Torture, strictOrderViolation)
{
    mock().strictOrder();
    mock().expectOneCall("first");
    mock().expectOneCall("second");
    mock().actualCall("second");
    mock().actualCall("first");
    mock().checkExpectations(); /* out-of-order failure */
}

/* ---- output parameters mixed sizes + unmodified -------------------------- */
TEST(Torture, outputParameterSizeVariants)
{
    static const unsigned char big[8] = { 9, 9, 9, 9, 9, 9, 9, 9 };
    unsigned char dst[8];
    memset(dst, 0xEE, sizeof dst);

    mock().expectOneCall("read").withOutputParameterReturning("buf", big, 3);
    mock().actualCall("read").withOutputParameter("buf", dst);
    LONGS_EQUAL(9, dst[2]);
    LONGS_EQUAL(0xEE, dst[3]); /* only 3 bytes copied */

    mock().expectOneCall("touch").withUnmodifiedOutputParameter("buf");
    memset(dst, 0x55, sizeof dst);
    mock().actualCall("touch").withOutputParameter("buf", dst);
    LONGS_EQUAL(0x55, dst[0]);
    mock().checkExpectations();
}

/* ---- NULL and empty-string parameters ----------------------------------- */
TEST(Torture, nullAndEmptyStringParameters)
{
    const char *null_string = 0;
    mock().expectOneCall("s").withParameter("p", null_string);
    mock().actualCall("s").withParameter("p", null_string);
    mock().expectOneCall("s").withParameter("p", "");
    mock().actualCall("s").withParameter("p", "");
    mock().checkExpectations();
}

/* ---- zero-size memory buffer parameters --------------------------------- */
TEST(Torture, zeroSizeMemoryBuffer)
{
    static const unsigned char b[1] = { 7 };
    mock().expectOneCall("m").withParameter("buf", b, 0u);
    mock().actualCall("m").withParameter("buf", b, 0u);
    mock().checkExpectations();
}

/* ---- stateful comparator ------------------------------------------------ */
class FlakyComparator : public MockNamedValueComparator
{
public:
    int calls;
    FlakyComparator() : calls(0) {}
    bool isEqual(const void *, const void *) CPPUTEST_OVERRIDE
    {
        return ++calls > 1; /* first comparison fails, later ones pass */
    }
    SimpleString valueToString(const void *) CPPUTEST_OVERRIDE
    {
        return "FLAKY";
    }
};

TEST(Torture, statefulComparator)
{
    static FlakyComparator comparator;
    comparator.calls = 0;
    static int obj = 1;
    mock().installComparator("T", comparator);
    mock().expectOneCall("c").withParameterOfType("T", "o", &obj);
    mock().expectOneCall("c").withParameterOfType("T", "o", &obj);
    /* first actual: comparator says no for exp1, yes for exp2 */
    mock().actualCall("c").withParameterOfType("T", "o", &obj);
    mock().actualCall("c").withParameterOfType("T", "o", &obj);
    mock().checkExpectations();
}

/* ---- C and C++ mock APIs hitting the same scope -------------------------- */
TEST(Torture, mixedCAndCppOnSameScope)
{
    mock().expectOneCall("mixed").withParameter("n", 5).andReturnValue(50);
    int r = mock_c()->actualCall("mixed")->withIntParameters("n", 5)->intReturnValue();
    LONGS_EQUAL(50, r);

    mock_c()->expectOneCall("mixed2")->withStringParameters("s", "q");
    mock().actualCall("mixed2").withParameter("s", "q");
    mock().checkExpectations();
}

/* ---- expectNCalls partially fulfilled: message shows counts -------------- */
TEST(Torture, partialNCallsMessage)
{
    mock().expectNCalls(3, "thrice").withParameter("v", 1);
    mock().actualCall("thrice").withParameter("v", 1);
    mock().actualCall("thrice").withParameter("v", 1);
    mock().checkExpectations(); /* failure: expected 3 calls, called 2 times */
}

/* ---- duplicate parameter names (upstream first-by-name quirks) ----------- */
TEST(Torture, duplicateParameterNames)
{
    mock().expectOneCall("dup").withParameter("a", 1).withParameter("a", 2);
    /* the first "a" wins lookups; an actual a=1 marks both entries */
    mock().actualCall("dup").withParameter("a", 1);
    mock().checkExpectations();
}

/* ---- disabled scope records nothing, enabled again works ------------------ */
TEST(Torture, disableEnableScopes)
{
    mock().disable();
    mock().expectOneCall("ghost");
    mock("sub").expectOneCall("ghost");
    mock().enable();
    mock().expectOneCall("real");
    mock().actualCall("real");
    mock().checkExpectations();
}

/* ---- expectNoCall coexists with fulfilled calls --------------------------- */
TEST(Torture, expectNoCallWithOtherTraffic)
{
    mock().expectNoCall("forbidden");
    mock().expectOneCall("allowed").andReturnValue("ok");
    STRCMP_EQUAL("ok", mock().actualCall("allowed").returnStringValueOrDefault(""));
    mock().checkExpectations();
}

/* ---- onObject: instance matching, both failure shapes --------------------- */
TEST(Torture, onObjectMatchesInstance)
{
    void *obj1 = (void *)0x1000;
    void *obj2 = (void *)0x2000;
    mock().expectOneCall("method").onObject(obj1).andReturnValue(1);
    mock().expectOneCall("method").onObject(obj2).andReturnValue(2);
    LONGS_EQUAL(2, mock().actualCall("method").onObject(obj2).returnIntValueOrDefault(0));
    LONGS_EQUAL(1, mock().actualCall("method").onObject(obj1).returnIntValueOrDefault(0));
    mock().checkExpectations();
}

TEST(Torture, onObjectUnexpectedObject)
{
    void *obj1 = (void *)0x1000;
    void *obj3 = (void *)0x3000;
    mock().expectOneCall("method").onObject(obj1);
    mock().actualCall("method").onObject(obj3); /* fails: unexpected object */
    mock().checkExpectations();
}

TEST(Torture, onObjectNeverPassed)
{
    void *obj1 = (void *)0x1000;
    mock().expectOneCall("method").onObject(obj1);
    mock().actualCall("method"); /* object never passed */
    mock().checkExpectations();  /* expected-object-didnt-happen */
}

TEST(Torture, onObjectMixedWithPlainExpectation)
{
    void *obj1 = (void *)0x1000;
    mock().expectOneCall("method").andReturnValue(7);
    /* plain expectation ignores the object */
    LONGS_EQUAL(7, mock().actualCall("method").onObject(obj1).returnIntValueOrDefault(0));
    mock().checkExpectations();
}

TEST(Torture, onObjectWithParameters)
{
    void *obj1 = (void *)0x1000;
    mock().expectOneCall("set").onObject(obj1).withParameter("v", 5);
    mock().actualCall("set").onObject(obj1).withParameter("v", 5);
    mock().checkExpectations();
}

/* ---- return value type punning between int widths ------------------------- */
TEST(Torture, crossWidthReturnDefaults)
{
    mock().expectOneCall("w").andReturnValue((unsigned long)7);
    mock().actualCall("w");
    UNSIGNED_LONGS_EQUAL(7, mock().unsignedLongIntReturnValue());
    LONGS_EQUAL(-3, mock().returnIntValueOrDefault(-3) == -3 ? -3 : 0); /* wrong type asked via OrDefault */
}

int main(int argc, char **argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
