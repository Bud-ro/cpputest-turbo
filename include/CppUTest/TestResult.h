#ifndef D_TestResult_h
#define D_TestResult_h

/* cpputest-turbo: TestResult shim. The real counters live in the C core's
 * per-run result; this class is a facade so plugin signatures and user code
 * stay source compatible. */

#include "CppUTest/TestFailure.h"
#include <cpputest_core/core.h>

class TestOutput;

class TestResult
{
  public:
    TestResult() : output_(NULLPTR), bound_(false) { zeroStats(); }
    /* upstream form: a result bound to an output object; used with
     * TestRegistry::runAllTests(result) which fills the stats */
    explicit TestResult(TestOutput &output) : output_(&output), bound_(false)
    {
        zeroStats();
    }
    virtual ~TestResult() {}

    virtual void addFailure(const TestFailure &failure)
    {
        cu_add_failure(failure.getFileName().asCharString(),
                       failure.getFailureLineNumber(),
                       failure.getMessage().asCharString());
    }

    virtual size_t getFailureCount() const
    {
        return bound_ ? stats_.failure_count : cu_failure_count();
    }
    virtual size_t getCheckCount() const { return stats_.check_count; }
    virtual size_t getTestCount() const { return stats_.test_count; }
    virtual size_t getRunCount() const { return stats_.run_count; }
    virtual size_t getIgnoredCount() const { return stats_.ignored_count; }
    virtual size_t getFilteredOutCount() const
    {
        return stats_.filtered_out_count;
    }

    virtual void countCheck() { cu_count_check(); }

    virtual void print(const char *text) { cu_print_text(text); }

    /* internal: TestRegistry::runAllTests plumbing */
    void setStats(const cu_run_stats &stats)
    {
        stats_ = stats;
        bound_ = true;
    }

    TestOutput *getOutputForCapture() { return output_; }

  private:
    void zeroStats()
    {
        stats_.test_count = stats_.run_count = stats_.check_count = 0;
        stats_.failure_count = stats_.ignored_count =
            stats_.filtered_out_count = 0;
    }

    TestOutput *output_;
    bool bound_;
    cu_run_stats stats_;
};

#endif
