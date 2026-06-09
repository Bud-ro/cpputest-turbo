#ifndef D_CommandLineTestRunner_h
#define D_CommandLineTestRunner_h

/* cpputest-revibed: runner entry points. All logic lives in the C core. */

#include "CppUTest/CppUTestConfig.h"
#include <cpputest_core/core.h>

class CommandLineTestRunner
{
public:
    static int RunAllTests(int ac, const char *const *av)
    {
        return cu_run_all(ac, av);
    }

    static int RunAllTests(int ac, char **av)
    {
        return cu_run_all(ac, (const char *const *)av);
    }
};

#endif
