#ifndef D_MockSupportPlugin_h
#define D_MockSupportPlugin_h

/* cpputest-revibed: MockSupportPlugin — checks and clears mock expectations
 * after every test, like upstream. Comparator/copier repository installation
 * arrives with the comparator slice. */

#include "CppUTest/TestPlugin.h"
#include "CppUTestExt/MockSupport.h"

class MockSupportPlugin : public TestPlugin
{
public:
    MockSupportPlugin(const SimpleString &name = "MockSupportPlugin")
        : TestPlugin(name)
    {
    }

    virtual ~MockSupportPlugin() {}

    virtual void preTestAction(UtestShell &, TestResult &) CPPUTEST_OVERRIDE
    {
        /* comparator/copier repository installation lands later */
    }

    virtual void postTestAction(UtestShell &, TestResult &) CPPUTEST_OVERRIDE
    {
        mock().checkExpectations();
        mock().clear();
    }
};

#endif
