#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestPlugin.h"
#include "CppUTest/TestRegistry.h"

#include <string.h>

static int preCount = 0;
static int postCount = 0;
static int disabledCount = 0;
static int parsedCustomArg = 0;

class CountingPlugin : public TestPlugin
{
public:
    CountingPlugin(const SimpleString &name) : TestPlugin(name) {}

    void preTestAction(UtestShell &, TestResult &) CPPUTEST_OVERRIDE { preCount++; }
    void postTestAction(UtestShell &, TestResult &) CPPUTEST_OVERRIDE { postCount++; }

    bool parseArguments(int, const char *const *av, int index) CPPUTEST_OVERRIDE
    {
        if (0 == strcmp(av[index], "-pcustom")) {
            parsedCustomArg++;
            return true;
        }
        return false;
    }
};

class DisabledPlugin : public TestPlugin
{
public:
    DisabledPlugin(const SimpleString &name) : TestPlugin(name) {}
    void preTestAction(UtestShell &, TestResult &) CPPUTEST_OVERRIDE { disabledCount++; }
};

static const char *pointee = "original";

TEST_GROUP(Ptr)
{
};

/* runs SECOND (reverse declaration order): pointer must have been restored
 * by SetPointerPlugin's postTestAction after the previous test */
TEST(Ptr, restoredAfterPreviousTest)
{
    STRCMP_EQUAL("original", pointee);
}

/* runs FIRST */
TEST(Ptr, utPtrSet)
{
    UT_PTR_SET(pointee, "changed");
    STRCMP_EQUAL("changed", pointee);
}

int main(int ac, char **av)
{
    CountingPlugin counting("CountingPlugin");
    DisabledPlugin disabled("DisabledPlugin");
    disabled.disable();
    TestRegistry::getCurrentRegistry()->installPlugin(&counting);
    TestRegistry::getCurrentRegistry()->installPlugin(&disabled);

    if (TestRegistry::getCurrentRegistry()->getPluginByName("CountingPlugin") != &counting)
        return 90;

    int result = CommandLineTestRunner::RunAllTests(ac, av);

    TestRegistry::getCurrentRegistry()->removePluginByName("DisabledPlugin");
    TestRegistry::getCurrentRegistry()->removePluginByName("CountingPlugin");

    if (result != 0) /* parse errors / failures: no invariants to check */
        return result;
    if (preCount != 2 || postCount != 2)
        return 91;
    if (disabledCount != 0)
        return 92;
    /* exactly one -pcustom expected when passed on the command line */
    for (int i = 1; i < ac; i++)
        if (0 == strcmp(av[i], "-pcustom") && parsedCustomArg != 1)
            return 93;

    return result;
}
