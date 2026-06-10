/* differential probe: MockSupportPlugin comparator install + hasFailed gate */
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

class IntComparator : public MockNamedValueComparator
{
public:
    bool isEqual(const void *a, const void *b) CPPUTEST_OVERRIDE
    {
        return *(const int *)a == *(const int *)b;
    }
    SimpleString valueToString(const void *o) CPPUTEST_OVERRIDE
    {
        return StringFrom(*(const int *)o);
    }
};

static IntComparator comparator;
static int obj = 42;

TEST_GROUP(PluginProbe)
{
};

/* runs LAST (reverse registration order) */
TEST(PluginProbe, comparatorAvailableWithoutManualInstall)
{
    mock().expectOneCall("draw").withParameterOfType("I", "p", &obj);
    mock().actualCall("draw").withParameterOfType("I", "p", &obj);
    /* no manual checkExpectations: plugin's postTestAction covers it */
}

/* runs 2nd: a test that fails an ordinary assertion AND leaves an
 * unfulfilled expectation — the plugin must NOT pile on a mock failure */
TEST(PluginProbe, failedTestSkipsExpectationCheck)
{
    mock().expectOneCall("never");
    FAIL("deliberate");
}

/* runs FIRST: comparator available again (re-installed per test) */
TEST(PluginProbe, comparatorAvailableEveryTest)
{
    mock().expectOneCall("draw").withParameterOfType("I", "p", &obj);
    mock().actualCall("draw").withParameterOfType("I", "p", &obj);
}

int main(int argc, char **argv)
{
    MockSupportPlugin plugin;
    plugin.installComparator("I", comparator);
    TestRegistry::getCurrentRegistry()->installPlugin(&plugin);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
