#ifndef D_TestTestingFixture_H
#define D_TestTestingFixture_H

/* cpputest-turbo: TestTestingFixture shim — runs a generated test in an
 * isolated registry context and captures its output, so the framework can
 * test itself (upstream's own suite leans on this heavily). */

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestRegistry.h"
#include "cpputest_core/core.h"

class ExecFunction
{
  public:
    ExecFunction() {}
    virtual ~ExecFunction() {}
    virtual void exec() = 0;
};

class ExecFunctionWithoutParameters : public ExecFunction
{
  public:
    void (*testFunction_)();

    ExecFunctionWithoutParameters(void (*testFunction)())
        : testFunction_(testFunction)
    {
    }
    virtual ~ExecFunctionWithoutParameters() {}
    virtual void exec() CPPUTEST_OVERRIDE
    {
        if (testFunction_)
            testFunction_();
    }
};

class ExecFunctionTest : public Utest
{
  public:
    void (*setup_)();
    void (*teardown_)();
    ExecFunction *testFunction_;

    /* parameter names must not shadow the setup()/teardown() members:
     * consumers compile this header under -Wshadow -Werror */
    ExecFunctionTest(void (*setupFn)(), void (*teardownFn)(),
                     ExecFunction *testFunction)
        : setup_(setupFn), teardown_(teardownFn), testFunction_(testFunction)
    {
    }
    virtual void setup() CPPUTEST_OVERRIDE
    {
        if (setup_)
            setup_();
    }
    virtual void teardown() CPPUTEST_OVERRIDE
    {
        if (teardown_)
            teardown_();
    }
    virtual void testBody() CPPUTEST_OVERRIDE
    {
        if (testFunction_)
            testFunction_->exec();
    }
};

class ExecFunctionTestShell : public UtestShell
{
  public:
    void (*setup_)();
    void (*teardown_)();
    ExecFunction *testFunction_;

    ExecFunctionTestShell()
        : setup_(NULLPTR), teardown_(NULLPTR), testFunction_(NULLPTR)
    {
        setGroupName("Generic");
        setTestName("Generic Test");
        setFileName("Generic File");
    }
    virtual Utest *createTest() CPPUTEST_OVERRIDE
    {
        return new ExecFunctionTest(setup_, teardown_, testFunction_);
    }
};

class TestTestingFixture
{
  public:
    TestTestingFixture() : verbose_(false), ownsExecFunction_(false)
    {
        registry_ = new TestRegistry;
        TestRegistry::getCurrentRegistry()->setCurrentRegistry(registry_);
        genTest_ = new ExecFunctionTestShell();
        registry_->addTest(genTest_);
        clearStats();
        currentFixture() = this;
        lineExecutedFlag() = false;
    }

    virtual ~TestTestingFixture()
    {
        clearExecFunction();
        TestRegistry::getCurrentRegistry()->setCurrentRegistry(NULLPTR);
        delete genTest_;
        delete registry_;
        currentFixture() = NULLPTR;
    }

    void flushOutputAndResetResult()
    {
        output_ = "";
        clearStats();
    }

    void addTest(UtestShell *test) { registry_->addTest(test); }

    void installPlugin(TestPlugin *plugin) { registry_->installPlugin(plugin); }

    void setTestFunction(void (*testFunction)())
    {
        clearExecFunction();
        genTest_->testFunction_ =
            new ExecFunctionWithoutParameters(testFunction);
        ownsExecFunction_ = true;
    }

    void setTestFunction(ExecFunction *testFunction)
    {
        clearExecFunction();
        genTest_->testFunction_ = testFunction;
        ownsExecFunction_ = false;
    }

    void setSetup(void (*setupFunction)()) { genTest_->setup_ = setupFunction; }
    void setTeardown(void (*teardownFunction)())
    {
        genTest_->teardown_ = teardownFunction;
    }
    void setOutputVerbose() { verbose_ = true; }

    void runTestWithMethod(void (*method)())
    {
        setTestFunction(method);
        runAllTests();
    }

    void runAllTests()
    {
        /* the fixture's own registry is current, so its (usually empty)
         * plugin chain runs — not the outer runner's plugins */
        cu_set_output_sink(captureSink, this);
        cu_run_registered_tests_ex(&stats_, verbose_ ? 1 : 0, 1);
        cu_set_output_sink(NULLPTR, NULLPTR);
    }

    size_t getFailureCount() { return stats_.failure_count; }
    size_t getCheckCount() { return stats_.check_count; }
    size_t getIgnoreCount() { return stats_.ignored_count; }
    size_t getRunCount() { return stats_.run_count; }
    size_t getTestCount() { return stats_.test_count; }

    const SimpleString &getOutput() { return output_; }
    TestRegistry *getRegistry() { return registry_; }

    bool hasTestFailed() { return genTest_->node_.has_failed != 0; }

    void assertPrintContains(const SimpleString &contains)
    {
        STRCMP_CONTAINS(contains.asCharString(), output_.asCharString());
    }

    void assertPrintContainsNot(const SimpleString &contains)
    {
        CHECK_FALSE(output_.contains(contains));
    }

    /* upstream-exact: one failure, output contains text, and the failing
     * macro must have jumped (the line after it must not have executed) */
    void checkTestFailsWithProperTestLocation(const char *text,
                                              const char *file, size_t line)
    {
        if (getFailureCount() != 1)
            FAIL_LOCATION(StringFromFormat("Expected one test failure, but got "
                                           "%d amount of test failures",
                                           (int)getFailureCount())
                              .asCharString(),
                          file, line);
        STRCMP_CONTAINS_LOCATION(text, output_.asCharString(), "", file, line);
        if (lineExecutedFlag())
            FAIL_LOCATION(
                "The test should jump/throw on failure and not execute the "
                "next line. However, the next line was executed.",
                file, line);
    }

    static void lineExecutedAfterCheck() { lineExecutedFlag() = true; }
    static bool &lineExecutedFlag()
    {
        static bool executed = false;
        return executed;
    }

  private:
    static TestTestingFixture *&currentFixture()
    {
        static TestTestingFixture *fixture = NULLPTR;
        return fixture;
    }

    static void captureSink(const char *text, void *arg)
    {
        static_cast<TestTestingFixture *>(arg)->output_ += text;
    }

    void clearExecFunction()
    {
        if (ownsExecFunction_ && genTest_->testFunction_) {
            delete genTest_->testFunction_;
        }
        genTest_->testFunction_ = NULLPTR;
        ownsExecFunction_ = false;
    }

    void clearStats()
    {
        stats_.test_count = stats_.run_count = stats_.check_count = 0;
        stats_.failure_count = stats_.ignored_count =
            stats_.filtered_out_count = 0;
    }

    TestRegistry *registry_;
    ExecFunctionTestShell *genTest_;
    bool verbose_;
    bool ownsExecFunction_;
    SimpleString output_;
    cu_run_stats stats_;
};

#endif
