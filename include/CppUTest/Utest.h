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
    static void crash() { __builtin_trap(); }

    /* assertion dispatch — same method surface as upstream UtestShell;
     * everything lowers to the C core */
    void assertTrue(bool condition, const char *checkString, const char *conditionString,
                    const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_true(condition, checkString, conditionString, text, fileName, lineNumber); }
    void fail(const char *text, const char *fileName, size_t lineNumber)
    { cu_fail(text, fileName, lineNumber); }
    void assertCstrEqual(const char *expected, const char *actual, const char *text,
                         const char *fileName, size_t lineNumber)
    { cu_assert_cstr_equal(expected, actual, text, fileName, lineNumber); }
    void assertCstrNEqual(const char *expected, const char *actual, size_t length,
                          const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_cstr_nequal(expected, actual, length, text, fileName, lineNumber); }
    void assertCstrNoCaseEqual(const char *expected, const char *actual, const char *text,
                               const char *fileName, size_t lineNumber)
    { cu_assert_cstr_nocase_equal(expected, actual, text, fileName, lineNumber); }
    void assertCstrContains(const char *expected, const char *actual, const char *text,
                            const char *fileName, size_t lineNumber)
    { cu_assert_cstr_contains(expected, actual, text, fileName, lineNumber); }
    void assertCstrNoCaseContains(const char *expected, const char *actual, const char *text,
                                  const char *fileName, size_t lineNumber)
    { cu_assert_cstr_nocase_contains(expected, actual, text, fileName, lineNumber); }
    void assertLongsEqual(long expected, long actual, const char *text,
                          const char *fileName, size_t lineNumber)
    { cu_assert_longs_equal(expected, actual, text, fileName, lineNumber); }
    void assertUnsignedLongsEqual(unsigned long expected, unsigned long actual,
                                  const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_unsigned_longs_equal(expected, actual, text, fileName, lineNumber); }
    void assertLongLongsEqual(cpputest_longlong expected, cpputest_longlong actual,
                              const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_longlongs_equal(expected, actual, text, fileName, lineNumber); }
    void assertUnsignedLongLongsEqual(cpputest_ulonglong expected, cpputest_ulonglong actual,
                                      const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_unsigned_longlongs_equal(expected, actual, text, fileName, lineNumber); }
    void assertSignedBytesEqual(signed char expected, signed char actual, const char *text,
                                const char *fileName, size_t lineNumber)
    { cu_assert_signed_bytes_equal(expected, actual, text, fileName, lineNumber); }
    void assertPointersEqual(const void *expected, const void *actual, const char *text,
                             const char *fileName, size_t lineNumber)
    { cu_assert_pointers_equal(expected, actual, text, fileName, lineNumber); }
    void assertFunctionPointersEqual(void (*expected)(), void (*actual)(), const char *text,
                                     const char *fileName, size_t lineNumber)
    { cu_assert_functionpointers_equal((void (*)(void))expected, (void (*)(void))actual,
                                       text, fileName, lineNumber); }
    void assertDoublesEqual(double expected, double actual, double threshold,
                            const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_doubles_equal(expected, actual, threshold, text, fileName, lineNumber); }
    void assertBinaryEqual(const void *expected, const void *actual, size_t length,
                           const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_binary_equal(expected, actual, length, text, fileName, lineNumber); }
    void assertBitsEqual(unsigned long expected, unsigned long actual, unsigned long mask,
                         size_t byteCount, const char *text,
                         const char *fileName, size_t lineNumber)
    { cu_assert_bits_equal(expected, actual, mask, byteCount, text, fileName, lineNumber); }
    void assertEquals(bool failed, const char *expected, const char *actual,
                      const char *text, const char *file, size_t line)
    { cu_assert_equals(failed, expected, actual, text, file, line); }
    void assertCompare(bool comparison, const char *checkString, const char *comparisonString,
                       const char *text, const char *fileName, size_t lineNumber)
    { cu_assert_compare(comparison, checkString, comparisonString, text, fileName, lineNumber); }
    void print(const char *text, const char *fileName, size_t lineNumber)
    { cu_print(text, fileName, lineNumber); }
    void countCheck() { cu_count_check(); }

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
