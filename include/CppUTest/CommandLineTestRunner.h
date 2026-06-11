#ifndef D_CommandLineTestRunner_h
#define D_CommandLineTestRunner_h

/* cpputest-turbo lite: runner entry points; run logic lives in the C
 * core. Install plugins (e.g. MockSupportPlugin) on the registry before
 * calling RunAllTests. */

#include "CppUTest/CppUTestConfig.h"
#include "CppUTest/TestRegistry.h"
#include "cpputest_core/core.h"

#include <stdio.h>

class CommandLineTestRunner
{
  public:
    static int RunAllTests(int ac, const char *const *av)
    {
        return cu_run_all(ac, av);
    }

    static int RunAllTests(int ac, char **av)
    {
        return RunAllTests(ac, (const char *const *)av);
    }
};

#endif
