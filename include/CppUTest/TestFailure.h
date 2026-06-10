#ifndef D_TestFailure_h
#define D_TestFailure_h

/* cpputest-revibed: minimal TestFailure shim — enough for plugins and user
 * code that constructs failures and hands them to TestResult::addFailure.
 * Message formatting for the builtin assertion types lives in the C core. */

#include "CppUTest/SimpleString.h"
#include "CppUTest/Utest.h"

class TestFailure
{
  public:
    TestFailure(UtestShell *shell, const char *fileName, size_t lineNumber,
                const SimpleString &theMessage)
        : testName_(shell ? shell->getName() : ""), fileName_(fileName),
          lineNumber_(lineNumber), message_(theMessage)
    {
    }

    TestFailure(UtestShell *shell, const SimpleString &theMessage)
        : testName_(shell ? shell->getName() : ""),
          fileName_(shell ? shell->getFile() : "unknown file"),
          lineNumber_(shell ? shell->getLineNumber() : 0), message_(theMessage)
    {
    }

    virtual ~TestFailure() {}

    virtual SimpleString getFileName() const { return fileName_; }
    virtual SimpleString getTestName() const { return testName_; }
    virtual SimpleString getTestNameOnly() const { return testName_; }
    virtual size_t getFailureLineNumber() const { return lineNumber_; }
    virtual SimpleString getMessage() const { return message_; }

  private:
    SimpleString testName_;
    SimpleString fileName_;
    size_t lineNumber_;
    SimpleString message_;
};

#endif
