#ifndef D_TestOutput_h
#define D_TestOutput_h

/* cpputest-revibed: minimal TestOutput shim. The real output engine lives in
 * the C core; these classes exist for source compatibility with code that
 * constructs outputs directly (upstream tests, custom runners). Text routed
 * into them is forwarded to the active core output sink only for the
 * console flavor; StringBufferTestOutput captures into a string. */

#include "CppUTest/SimpleString.h"

#include <stdio.h>

class UtestShell;
class TestResult;

class TestOutput
{
  public:
    enum WorkingEnvironment { visualStudio, eclipse, detectEnvironment };

    enum VerbosityLevel { level_quiet, level_verbose, level_veryVerbose };

    TestOutput() : verbose_(level_quiet), color_(false) {}
    virtual ~TestOutput() {}

    virtual void verbose(VerbosityLevel level) { verbose_ = level; }
    virtual void color() { color_ = true; }

    virtual void print(const char *str) { printBuffer(str); }
    virtual void print(long n) { printLong(n); }
    virtual void print(size_t n) { printSize(n); }
    virtual void printDouble(double d)
    {
        char buf[64];
        snprintf(buf, sizeof buf, "%f", d);
        printBuffer(buf);
    }

    virtual void printBuffer(const char *) = 0;
    virtual void flush() {}

  protected:
    void printLong(long n)
    {
        char buf[32];
        snprintf(buf, sizeof buf, "%ld", n);
        printBuffer(buf);
    }

    void printSize(size_t n)
    {
        char buf[32];
        snprintf(buf, sizeof buf, "%lu", (unsigned long)n);
        printBuffer(buf);
    }

    VerbosityLevel verbose_;
    bool color_;
};

class ConsoleTestOutput : public TestOutput
{
  public:
    virtual void printBuffer(const char *s) CPPUTEST_OVERRIDE
    {
        fputs(s, stdout);
        fflush(stdout);
    }
};

class StringBufferTestOutput : public TestOutput
{
  public:
    virtual ~StringBufferTestOutput() {}

    virtual void printBuffer(const char *s) CPPUTEST_OVERRIDE { output_ += s; }
    virtual void flush() CPPUTEST_OVERRIDE { output_ = ""; }
    const SimpleString &getOutput() { return output_; }

  protected:
    SimpleString output_;
};

#endif
