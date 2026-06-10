#ifndef D_MemoryLeakWarningPlugin_h
#define D_MemoryLeakWarningPlugin_h

/* cpputest-revibed: MemoryLeakWarningPlugin shim over the C core's leak
 * detector. Behavior mirrors upstream MemoryLeakWarningPlugin.cpp. */

#include "CppUTest/TestPlugin.h"
#include "CppUTest/TestResult.h"
#include <cpputest_core/core.h>

#define IGNORE_ALL_LEAKS_IN_TEST()                                             \
    if (MemoryLeakWarningPlugin::getFirstPlugin())                             \
    MemoryLeakWarningPlugin::getFirstPlugin()->ignoreAllLeaksInTest()

#define EXPECT_N_LEAKS(n)                                                      \
    if (MemoryLeakWarningPlugin::getFirstPlugin())                             \
    MemoryLeakWarningPlugin::getFirstPlugin()->expectLeaksInTest(n)

class MemoryLeakWarningPlugin : public TestPlugin
{
  public:
    MemoryLeakWarningPlugin(const SimpleString &name)
        : TestPlugin(name), ignoreAllWarnings_(false),
          destroyGlobalDetectorInDestructor_(false), expectedLeaks_(0),
          failureCount_(0)
    {
        if (firstPluginSlot() == NULLPTR)
            firstPluginSlot() = this;
    }

    virtual ~MemoryLeakWarningPlugin()
    {
        if (firstPluginSlot() == this)
            firstPluginSlot() = NULLPTR;
        if (destroyGlobalDetectorInDestructor_) {
            cu_mem_tracking_set(0);
            cu_mem_destroy_all();
        }
    }

    virtual void preTestAction(UtestShell &,
                               TestResult &result) CPPUTEST_OVERRIDE
    {
        cu_mem_start_checking();
        failureCount_ = result.getFailureCount();
    }

    virtual void postTestAction(UtestShell &test,
                                TestResult &result) CPPUTEST_OVERRIDE
    {
        cu_mem_stop_checking();
        size_t leaks = cu_mem_leak_count(1);

        if (!ignoreAllWarnings_ && expectedLeaks_ != leaks &&
            failureCount_ == result.getFailureCount()) {
            if (areNewDeleteOverloaded()) {
                TestFailure f(&test, cu_mem_leak_report(1));
                result.addFailure(f);
            } else if (expectedLeaks_ > 0) {
                result.print(StringFromFormat("Warning: Expected %d leak(s), "
                                              "but leak detection was disabled",
                                              (int)expectedLeaks_)
                                 .asCharString());
            }
        }
        cu_mem_mark_checking_as_global();
        ignoreAllWarnings_ = false;
        expectedLeaks_ = 0;
    }

    const char *FinalReport(size_t toBeDeletedLeaks = 0)
    {
        size_t leaks = cu_mem_leak_count(0);
        if (leaks != toBeDeletedLeaks)
            return cu_mem_leak_report(0);
        return "";
    }

    void ignoreAllLeaksInTest() { ignoreAllWarnings_ = true; }
    void expectLeaksInTest(size_t n) { expectedLeaks_ = n; }

    void
    destroyGlobalDetectorAndTurnOffMemoryLeakDetectionInDestructor(bool des)
    {
        destroyGlobalDetectorInDestructor_ = des;
    }

    static MemoryLeakWarningPlugin *getFirstPlugin()
    {
        return firstPluginSlot();
    }

    static void turnOffNewDeleteOverloads() { cu_mem_tracking_set(0); }
    static void turnOnDefaultNotThreadSafeNewDeleteOverloads()
    {
        cu_mem_tracking_set(1);
    }
    static void turnOnThreadSafeNewDeleteOverloads() { cu_mem_tracking_set(1); }
    static bool areNewDeleteOverloaded() { return cu_mem_tracking() != 0; }
    static void saveAndDisableNewDeleteOverloads()
    {
        cu_mem_save_and_disable_tracking();
    }
    static void restoreNewDeleteOverloads() { cu_mem_restore_tracking(); }
    static void destroyGlobalDetector() { cu_mem_destroy_all(); }

  private:
    static MemoryLeakWarningPlugin *&firstPluginSlot()
    {
        static MemoryLeakWarningPlugin *first = NULLPTR;
        return first;
    }

    bool ignoreAllWarnings_;
    bool destroyGlobalDetectorInDestructor_;
    size_t expectedLeaks_;
    size_t failureCount_;
};

#endif

/* Upstream pulls the new-macro machinery in through this header */
#include "CppUTest/MemoryLeakDetectorNewMacros.h"
