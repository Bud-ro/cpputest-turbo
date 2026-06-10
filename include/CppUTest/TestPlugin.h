#ifndef D_TestPlugin_h
#define D_TestPlugin_h

/* cpputest-revibed: TestPlugin chain, upstream-identical semantics. The
 * chain lives entirely in the C++ shim; the C core invokes it through
 * trampolines installed by TestRegistry. */

#include "CppUTest/SimpleString.h"
#include "CppUTest/TestResult.h"
#include "CppUTest/UtestMacros.h"

class UtestShell;
class NullTestPlugin;

class TestPlugin
{
  public:
    TestPlugin(const SimpleString &name);
    TestPlugin(TestPlugin *next) : next_(next), name_("null"), enabled_(true) {}
    virtual ~TestPlugin() {}

    virtual void preTestAction(UtestShell &, TestResult &) {}
    virtual void postTestAction(UtestShell &, TestResult &) {}
    virtual bool parseArguments(int, const char *const *, int) { return false; }

    virtual void runAllPreTestAction(UtestShell &test, TestResult &result)
    {
        if (enabled_)
            preTestAction(test, result);
        next_->runAllPreTestAction(test, result);
    }

    virtual void runAllPostTestAction(UtestShell &test, TestResult &result)
    {
        next_->runAllPostTestAction(test, result);
        if (enabled_)
            postTestAction(test, result);
    }

    virtual bool parseAllArguments(int ac, char **av, int index)
    {
        return parseAllArguments(ac, (const char *const *)av, index);
    }

    virtual bool parseAllArguments(int ac, const char *const *av, int index)
    {
        if (parseArguments(ac, av, index))
            return true;
        if (next_)
            return next_->parseAllArguments(ac, av, index);
        return false;
    }

    virtual TestPlugin *addPlugin(TestPlugin *plugin)
    {
        next_ = plugin;
        return this;
    }

    virtual TestPlugin *removePluginByName(const SimpleString &name)
    {
        TestPlugin *removed = NULLPTR;
        if (next_ && next_->getName() == name) {
            removed = next_;
            next_ = next_->next_;
        }
        return removed;
    }

    const SimpleString &getName() { return name_; }

    TestPlugin *getPluginByName(const SimpleString &name)
    {
        if (name == name_)
            return this;
        if (next_)
            return next_->getPluginByName(name);
        return next_;
    }

    virtual TestPlugin *getNext() { return next_; }
    virtual void disable() { enabled_ = false; }
    virtual void enable() { enabled_ = true; }
    virtual bool isEnabled() { return enabled_; }

  protected:
    TestPlugin *next_;

  private:
    SimpleString name_;
    bool enabled_;
};

class NullTestPlugin : public TestPlugin
{
  public:
    NullTestPlugin() : TestPlugin(NULLPTR) {}
    virtual ~NullTestPlugin() {}

    static NullTestPlugin *instance()
    {
        static NullTestPlugin _instance;
        return &_instance;
    }

    virtual void runAllPreTestAction(UtestShell &,
                                     TestResult &) CPPUTEST_OVERRIDE
    {
    }
    virtual void runAllPostTestAction(UtestShell &,
                                      TestResult &) CPPUTEST_OVERRIDE
    {
    }
};

inline TestPlugin::TestPlugin(const SimpleString &name)
    : next_(NullTestPlugin::instance()), name_(name), enabled_(true)
{
}

/* ------------------------- SetPointerPlugin ----------------------------- */

struct cpputest_pair {
    void **orig;
    void *orig_value;
};

class SetPointerPlugin : public TestPlugin
{
  public:
    enum { MAX_SET = 32 };

    SetPointerPlugin(const SimpleString &name);

    virtual void postTestAction(UtestShell &, TestResult &) CPPUTEST_OVERRIDE;
};

/* single instances across all translation units */
inline cpputest_pair *CppUTestStoreList()
{
    static cpputest_pair setlist[SetPointerPlugin::MAX_SET];
    return setlist;
}

inline int &CppUTestStoreIndex()
{
    static int pointerTableIndex = 0;
    return pointerTableIndex;
}

inline void CppUTestStore(void **function)
{
    if (CppUTestStoreIndex() >= SetPointerPlugin::MAX_SET) {
        FAIL("Maximum number of function pointers installed!");
    }
    CppUTestStoreList()[CppUTestStoreIndex()].orig_value = *function;
    CppUTestStoreList()[CppUTestStoreIndex()].orig = function;
    CppUTestStoreIndex()++;
}

inline SetPointerPlugin::SetPointerPlugin(const SimpleString &name)
    : TestPlugin(name)
{
    CppUTestStoreIndex() = 0;
}

inline void SetPointerPlugin::postTestAction(UtestShell &, TestResult &)
{
    for (int i = CppUTestStoreIndex() - 1; i >= 0; i--)
        *(CppUTestStoreList()[i].orig) = CppUTestStoreList()[i].orig_value;
    CppUTestStoreIndex() = 0;
}

#define UT_PTR_SET(a, b)                                                       \
    do {                                                                       \
        CppUTestStore((void **)&(a));                                          \
        (a) = b;                                                               \
    } while (0)

#endif
