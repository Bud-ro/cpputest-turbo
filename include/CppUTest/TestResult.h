#ifndef D_TestResult_h
#define D_TestResult_h

/* cpputest-revibed: TestResult shim. The real counters live in the C core's
 * per-run result; this class is a facade so plugin signatures and user code
 * stay source compatible. */

#include "CppUTest/TestFailure.h"
#include <cpputest_core/core.h>

class TestResult
{
public:
    TestResult() {}
    virtual ~TestResult() {}

    virtual void addFailure(const TestFailure &failure)
    {
        cu_add_failure(failure.getFileName().asCharString(),
                       failure.getFailureLineNumber(),
                       failure.getMessage().asCharString());
    }

    virtual size_t getFailureCount() const { return cu_failure_count(); }
    virtual void countCheck() { cu_count_check(); }

    virtual void print(const char *text) { cu_print_text(text); }
};

#endif
