#ifndef D_UTestMacros_h
#define D_UTestMacros_h

/* cpputest-revibed: test declaration and assertion macros. Generated names
 * are identical to upstream UtestMacros.h (including its quirks — see
 * docs/INTERFACE.md section 1) so that IMPORT_TEST_GROUP and friends stay
 * source compatible. Assertions lower to C core calls.
 *
 * Phase 1 subset: declaration macros + CHECK/CHECK_TEXT/CHECK_TRUE/
 * CHECK_FALSE/FAIL/FAIL_TEST/TEST_EXIT. The full set lands in Phase 2. */

#include "CppUTest/Utest.h"

#define TEST_GROUP_BASE(testGroup, baseclass) \
  extern int externTestGroup##testGroup; \
  int externTestGroup##testGroup = 0; \
  struct TEST_GROUP_##CppUTestGroup##testGroup : public baseclass

#define TEST_BASE(testBaseClass) \
  struct testBaseClass : public Utest

#define TEST_GROUP(testGroup) \
  TEST_GROUP_BASE(testGroup, Utest)

#define TEST_SETUP() \
  virtual void setup() CPPUTEST_OVERRIDE

#define TEST_TEARDOWN() \
  virtual void teardown() CPPUTEST_OVERRIDE

#define TEST(testGroup, testName) \
  /* External declarations for strict compilers */ \
  class TEST_##testGroup##_##testName##_TestShell; \
  extern TEST_##testGroup##_##testName##_TestShell TEST_##testGroup##_##testName##_TestShell_instance; \
  \
  class TEST_##testGroup##_##testName##_Test : public TEST_GROUP_##CppUTestGroup##testGroup \
{ public: \
    TEST_##testGroup##_##testName##_Test () : TEST_GROUP_##CppUTestGroup##testGroup () {} \
       void testBody() CPPUTEST_OVERRIDE; }; \
  class TEST_##testGroup##_##testName##_TestShell : public UtestShell { \
      virtual Utest* createTest() CPPUTEST_OVERRIDE { return new TEST_##testGroup##_##testName##_Test; } \
  } TEST_##testGroup##_##testName##_TestShell_instance; \
  static TestInstaller TEST_##testGroup##_##testName##_Installer(TEST_##testGroup##_##testName##_TestShell_instance, #testGroup, #testName, __FILE__,__LINE__); \
    void TEST_##testGroup##_##testName##_Test::testBody()

/* Upstream quirk preserved: the installer name has no underscore between
 * group and name (TEST_##testGroup##testName##_Installer). */
#define IGNORE_TEST(testGroup, testName) \
  class IGNORE##testGroup##_##testName##_TestShell; \
  extern IGNORE##testGroup##_##testName##_TestShell IGNORE##testGroup##_##testName##_TestShell_instance; \
  \
  class IGNORE##testGroup##_##testName##_Test : public TEST_GROUP_##CppUTestGroup##testGroup \
{ public: \
    IGNORE##testGroup##_##testName##_Test () : TEST_GROUP_##CppUTestGroup##testGroup () {} \
    virtual void testBody() CPPUTEST_OVERRIDE; }; \
  class IGNORE##testGroup##_##testName##_TestShell : public IgnoredUtestShell { \
      virtual Utest* createTest() CPPUTEST_OVERRIDE { return new IGNORE##testGroup##_##testName##_Test; } \
  } IGNORE##testGroup##_##testName##_TestShell_instance; \
  static TestInstaller TEST_##testGroup##testName##_Installer(IGNORE##testGroup##_##testName##_TestShell_instance, #testGroup, #testName, __FILE__,__LINE__); \
    void IGNORE##testGroup##_##testName##_Test::testBody()

#define IMPORT_TEST_GROUP(testGroup) \
  extern int externTestGroup##testGroup;\
  extern int* p##testGroup; \
  int* p##testGroup = &externTestGroup##testGroup

#define CPPUTEST_DEFAULT_MAIN \
  /*#*/ int main(int argc, char** argv) \
  { \
      return CommandLineTestRunner::RunAllTests(argc, argv); \
  }

/* -------------------- assertions (Phase 1 subset) -------------------- */

#define CHECK(condition) \
  CHECK_TRUE_LOCATION(condition, "CHECK", #condition, NULLPTR, __FILE__, __LINE__)

#define CHECK_TEXT(condition, text) \
  CHECK_TRUE_LOCATION(condition, "CHECK", #condition, text, __FILE__, __LINE__)

#define CHECK_TRUE(condition) \
  CHECK_TRUE_LOCATION(condition, "CHECK_TRUE", #condition, NULLPTR, __FILE__, __LINE__)

#define CHECK_TRUE_TEXT(condition, text) \
  CHECK_TRUE_LOCATION(condition, "CHECK_TRUE", #condition, text, __FILE__, __LINE__)

#define CHECK_FALSE(condition) \
  CHECK_FALSE_LOCATION(condition, "CHECK_FALSE", #condition, NULLPTR, __FILE__, __LINE__)

#define CHECK_FALSE_TEXT(condition, text) \
  CHECK_FALSE_LOCATION(condition, "CHECK_FALSE", #condition, text, __FILE__, __LINE__)

#define CHECK_TRUE_LOCATION(condition, checkString, conditionString, text, file, line) \
  do { cu_count_check(); \
       if (!(condition)) \
           cu_fail_check(file, line, checkString, conditionString, text); \
  } while (0)

#define CHECK_FALSE_LOCATION(condition, checkString, conditionString, text, file, line) \
  do { cu_count_check(); \
       if (condition) \
           cu_fail_check(file, line, checkString, conditionString, text); \
  } while (0)

#define FAIL(text) \
  FAIL_LOCATION(text, __FILE__, __LINE__)

#define FAIL_TEST(text) \
  FAIL_LOCATION(text, __FILE__, __LINE__)

#define FAIL_LOCATION(text, file, line) \
  do { cu_fail_text(file, line, text); } while (0)

#define TEST_EXIT \
  do { cu_exit_current_test(); } while (0)

#endif
