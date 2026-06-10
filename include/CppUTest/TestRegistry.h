#ifndef D_TestRegistry_h
#define D_TestRegistry_h

/* cpputest-turbo: TestRegistry shim. Test storage lives in the C core;
 * this class carries the plugin chain and the public registry API. */

#include "CppUTest/TestPlugin.h"
#include "CppUTest/TestOutput.h"
#include "CppUTest/Utest.h"
#include <cpputest_core/core.h>

class TestRegistry;

extern "C" {

inline void CppUTestPluginPreHook(cu_test *t);
inline void CppUTestPluginPostHook(cu_test *t);
inline int CppUTestPluginParseHook(int ac, const char *const *av, int index);

} /* extern "C" */

class TestRegistry
{
  public:
    /* instances are constructible like upstream; the default registry wraps
     * the process-wide C core list */
    TestRegistry() : firstPlugin_(NullTestPlugin::instance()), head_(NULLPTR) {}
    virtual ~TestRegistry() {}

    static TestRegistry *getCurrentRegistry()
    {
        return currentSlot() ? currentSlot() : defaultRegistry();
    }

    /* swap which registry's tests the core list holds; NULL restores the
     * default registry */
    virtual void setCurrentRegistry(TestRegistry *registry)
    {
        getCurrentRegistry()->head_ = cu_registry_tests();
        currentSlot() = registry;
        cu_registry_set_tests(getCurrentRegistry()->head_);
    }

    virtual void runAllTests(TestResult &result)
    {
        TestRegistry *saved = currentSlot();
        setCurrentRegistry(this);
        cu_set_output_sink(resultCaptureSink, &result);
        cu_run_stats stats;
        cu_run_registered_tests_ex(&stats, 0, 1);
        cu_set_output_sink(NULLPTR, NULLPTR);
        result.setStats(stats);
        setCurrentRegistry(saved);
    }

    virtual int countPlugins()
    {
        int count = 0;
        for (TestPlugin *p = firstPlugin_; p != NullTestPlugin::instance();
             p = p->getNext())
            count++;
        return count;
    }

    void addTest(UtestShell *test)
    {
        if (this == getCurrentRegistry()) {
            cu_registry_add(&test->node_);
        } else {
            test->node_.next = head_;
            head_ = &test->node_;
        }
    }

    void unDoLastAddTest() { cu_registry_undo_last_add(); }

    UtestShell *getFirstTest()
    {
        cu_test *t = cu_registry_tests();
        return t ? static_cast<UtestShell *>(t->user) : NULLPTR;
    }

    /* the test whose next pointer is `test` (NULL if none) */
    UtestShell *getTestWithNext(UtestShell *test)
    {
        for (cu_test *t = cu_registry_tests(); t; t = t->next)
            if (t->next == &test->node_)
                return static_cast<UtestShell *>(t->user);
        return NULLPTR;
    }

    void installPlugin(TestPlugin *plugin)
    {
        firstPlugin_ = plugin->addPlugin(firstPlugin_);
        cu_set_plugin_hooks(CppUTestPluginPreHook, CppUTestPluginPostHook,
                            CppUTestPluginParseHook);
    }

    TestPlugin *getFirstPlugin() { return firstPlugin_; }

    TestPlugin *getPluginByName(const SimpleString &name)
    {
        return firstPlugin_->getPluginByName(name);
    }

    TestPlugin *removePluginByName(const SimpleString &name)
    {
        if (firstPlugin_ != NullTestPlugin::instance() &&
            firstPlugin_->getName() == name) {
            TestPlugin *removed = firstPlugin_;
            firstPlugin_ = firstPlugin_->getNext();
            return removed;
        }
        return firstPlugin_->removePluginByName(name);
    }

  private:
    static TestRegistry *&currentSlot()
    {
        static TestRegistry *current = NULLPTR;
        return current;
    }

    static TestRegistry *defaultRegistry()
    {
        static TestRegistry registry;
        return &registry;
    }

    static void resultCaptureSink(const char *text, void *arg)
    {
        TestResult *result = static_cast<TestResult *>(arg);
        TestOutput *output = result->getOutputForCapture();
        if (output)
            output->print(text);
    }

    TestPlugin *firstPlugin_;
    cu_test *head_;
};

extern "C" inline void CppUTestPluginPreHook(cu_test *t)
{
    TestResult result;
    TestRegistry::getCurrentRegistry()->getFirstPlugin()->runAllPreTestAction(
        *static_cast<UtestShell *>(t->user), result);
}

extern "C" inline void CppUTestPluginPostHook(cu_test *t)
{
    TestResult result;
    TestRegistry::getCurrentRegistry()->getFirstPlugin()->runAllPostTestAction(
        *static_cast<UtestShell *>(t->user), result);
}

extern "C" inline int CppUTestPluginParseHook(int ac, const char *const *av,
                                              int index)
{
    return TestRegistry::getCurrentRegistry()
                   ->getFirstPlugin()
                   ->parseAllArguments(ac, av, index)
               ? 1
               : 0;
}

#endif
