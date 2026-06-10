#ifndef D_CommandLineTestRunner_h
#define D_CommandLineTestRunner_h

/* cpputest-revibed: runner entry points. Run logic lives in the C core;
 * this shim installs the standard plugins around the run like upstream
 * (SetPointerPlugin now; MemoryLeakWarningPlugin arrives in Phase 4). */

#include "CppUTest/CppUTestConfig.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTest/MemoryLeakWarningPlugin.h"
#include <cpputest_core/core.h>

#include <stdio.h>

#define DEF_PLUGIN_MEM_LEAK "MemoryLeakPlugin"
#define DEF_PLUGIN_SET_POINTER "SetPointerPlugin"

class CommandLineTestRunner
{
public:
    /* upstream flow: leak plugin around everything, SetPointerPlugin around
     * the run, final leak report printed only on a fully green result */
    static int RunAllTests(int ac, const char *const *av)
    {
        MemoryLeakWarningPlugin memLeakWarn(DEF_PLUGIN_MEM_LEAK);
        memLeakWarn.destroyGlobalDetectorAndTurnOffMemoryLeakDetectionInDestructor(true);
        TestRegistry::getCurrentRegistry()->installPlugin(&memLeakWarn);

        SetPointerPlugin pPlugin(DEF_PLUGIN_SET_POINTER);
        TestRegistry::getCurrentRegistry()->installPlugin(&pPlugin);
        int testResult = cu_run_all(ac, av);
        TestRegistry::getCurrentRegistry()->removePluginByName(DEF_PLUGIN_SET_POINTER);

        if (testResult == 0)
            fputs(memLeakWarn.FinalReport(0), stdout);
        TestRegistry::getCurrentRegistry()->removePluginByName(DEF_PLUGIN_MEM_LEAK);
        return testResult;
    }

    static int RunAllTests(int ac, char **av)
    {
        return RunAllTests(ac, (const char *const *)av);
    }
};

#endif
