#ifndef D_UTest_h
#define D_UTest_h

/* cpputest-revibed: thin C++ shim over the C core. The classes here keep
 * upstream's names and shapes (user fixtures derive from Utest, the TEST
 * macros generate UtestShell subclasses) but all execution, registration,
 * counting and output happens in libCppUTest's C core. */

#include "CppUTest/CppUTestConfig.h"
#include <cpputest_core/core.h>

class Utest
{
public:
    Utest() {}
    virtual ~Utest() {}
    virtual void setup() {}
    virtual void teardown() {}
    virtual void testBody() {}
};

class UtestShell;

extern "C" {

inline void *CppUTestShimCreate(cu_test *t);
inline void CppUTestShimDestroy(cu_test *t, void *fixture);

inline void CppUTestShimSetup(cu_test *t, void *fixture)
{
    (void)t;
    static_cast<Utest *>(fixture)->setup();
}

inline void CppUTestShimBody(cu_test *t, void *fixture)
{
    (void)t;
    static_cast<Utest *>(fixture)->testBody();
}

inline void CppUTestShimTeardown(cu_test *t, void *fixture)
{
    (void)t;
    static_cast<Utest *>(fixture)->teardown();
}

} /* extern "C" */

inline const cu_test_ops *CppUTestShimOps()
{
    static const cu_test_ops ops = {
        CppUTestShimCreate,
        CppUTestShimDestroy,
        CppUTestShimSetup,
        CppUTestShimBody,
        CppUTestShimTeardown
    };
    return &ops;
}

class UtestShell
{
public:
    UtestShell()
    {
        node_.group = "";
        node_.name = "";
        node_.file = "";
        node_.line = 0;
        node_.is_ignored = 0;
        node_.run_ignored = 0;
        node_.has_failed = 0;
        node_.user = this;
        node_.ops = CppUTestShimOps();
        node_.next = NULLPTR;
    }
    virtual ~UtestShell() {}

    virtual Utest *createTest() { return new Utest(); }
    virtual void destroyTest(Utest *test) { delete test; }

    const char *getName() const { return node_.name; }
    const char *getGroup() const { return node_.group; }
    const char *getFile() const { return node_.file; }
    size_t getLineNumber() const { return node_.line; }

    void setGroupName(const char *groupName) { node_.group = groupName; }
    void setTestName(const char *testName) { node_.name = testName; }
    void setFileName(const char *fileName) { node_.file = fileName; }
    void setLineNumber(size_t lineNumber) { node_.line = lineNumber; }

    static UtestShell *getCurrent()
    {
        cu_test *t = cu_current_test();
        return t ? static_cast<UtestShell *>(t->user) : NULLPTR;
    }

    void exitTest() { cu_exit_current_test(); }

    cu_test node_; /* registered with the C core; shim-internal */
};

class IgnoredUtestShell : public UtestShell
{
public:
    IgnoredUtestShell() { node_.is_ignored = 1; }
};

extern "C" inline void *CppUTestShimCreate(cu_test *t)
{
    return static_cast<void *>(static_cast<UtestShell *>(t->user)->createTest());
}

extern "C" inline void CppUTestShimDestroy(cu_test *t, void *fixture)
{
    static_cast<UtestShell *>(t->user)->destroyTest(static_cast<Utest *>(fixture));
}

class TestInstaller
{
public:
    TestInstaller(UtestShell &shell, const char *groupName, const char *testName,
                  const char *fileName, size_t lineNumber)
    {
        shell.setGroupName(groupName);
        shell.setTestName(testName);
        shell.setFileName(fileName);
        shell.setLineNumber(lineNumber);
        cu_registry_add(&shell.node_);
    }
    void unDo() { cu_registry_undo_last_add(); }
};

#endif
