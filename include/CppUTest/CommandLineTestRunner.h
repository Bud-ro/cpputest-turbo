#ifndef D_CommandLineTestRunner_h
#define D_CommandLineTestRunner_h

/* cpputest-revibed: runner entry points. Run logic lives in the C core;
 * this shim installs the standard plugins around the run like upstream
 * (SetPointerPlugin now; MemoryLeakWarningPlugin arrives in Phase 4). */

#include "CppUTest/CppUTestConfig.h"
#include "CppUTest/TestRegistry.h"
#include <cpputest_core/core.h>

#define DEF_PLUGIN_MEM_LEAK "MemoryLeakPlugin"
#define DEF_PLUGIN_SET_POINTER "SetPointerPlugin"

class CommandLineTestRunner
{
public:
    static int RunAllTests(int ac, const char *const *av)
    {
        SetPointerPlugin pPlugin(DEF_PLUGIN_SET_POINTER);
        TestRegistry::getCurrentRegistry()->installPlugin(&pPlugin);
        int testResult = cu_run_all(ac, av);
        TestRegistry::getCurrentRegistry()->removePluginByName(DEF_PLUGIN_SET_POINTER);
        return testResult;
    }

    static int RunAllTests(int ac, char **av)
    {
        return RunAllTests(ac, (const char *const *)av);
    }
};

#endif
