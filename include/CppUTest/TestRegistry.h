#ifndef D_TestRegistry_h
#define D_TestRegistry_h

/* cpputest-revibed: TestRegistry shim. Test storage lives in the C core;
 * this class carries the plugin chain and the public registry API. */

#include "CppUTest/TestPlugin.h"
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
    static TestRegistry *getCurrentRegistry()
    {
        static TestRegistry registry;
        return &registry;
    }

    void addTest(UtestShell *test) { cu_registry_add(&test->node_); }
    void unDoLastAddTest() { cu_registry_undo_last_add(); }

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
    TestRegistry() : firstPlugin_(NullTestPlugin::instance()) {}

    TestPlugin *firstPlugin_;
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

extern "C" inline int CppUTestPluginParseHook(int ac, const char *const *av, int index)
{
    return TestRegistry::getCurrentRegistry()->getFirstPlugin()->parseAllArguments(
               ac, av, index)
               ? 1
               : 0;
}

#endif
