#ifndef D_UTestMacros_h
#define D_UTestMacros_h

/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Portions copyright (c) 2007 Michael Feathers, James Grenning and Bas
 * Vodde: several macro bodies in this file are reproduced from CppUTest's
 * UtestMacros.h to guarantee source compatibility and byte-identical
 * behavior. Remainder copyright (c) 2026 cpputest-turbo contributors.
 * See LICENSE at the repository root.
 *
 * cpputest-turbo: test declaration and assertion macros. Generated names
 * are identical to upstream UtestMacros.h (including its quirks — see
 * docs/INTERFACE.md section 1) so that IMPORT_TEST_GROUP and friends stay
 * source compatible. Assertions lower to C core calls. */

#include "CppUTest/Utest.h"

#define TEST_GROUP_BASE(testGroup, baseclass)                                  \
    extern int externTestGroup##testGroup;                                     \
    int externTestGroup##testGroup = 0;                                        \
    struct TEST_GROUP_##CppUTestGroup##testGroup : public baseclass

#define TEST_BASE(testBaseClass) struct testBaseClass : public Utest

#define TEST_GROUP(testGroup) TEST_GROUP_BASE(testGroup, Utest)

#define TEST_SETUP() virtual void setup() CPPUTEST_OVERRIDE

#define TEST_TEARDOWN() virtual void teardown() CPPUTEST_OVERRIDE

#define TEST(testGroup, testName)                                              \
    /* External declarations for strict compilers */                           \
    class TEST_##testGroup##_##testName##_TestShell;                           \
    extern TEST_##testGroup##_##testName##_TestShell                           \
        TEST_##testGroup##_##testName##_TestShell_instance;                    \
                                                                               \
    class TEST_##testGroup##_##testName##_Test                                 \
        : public TEST_GROUP_##CppUTestGroup##testGroup                         \
    {                                                                          \
      public:                                                                  \
        TEST_##testGroup##_##testName##_Test()                                 \
            : TEST_GROUP_##CppUTestGroup##testGroup()                          \
        {                                                                      \
        }                                                                      \
        void testBody() CPPUTEST_OVERRIDE;                                     \
    };                                                                         \
    class TEST_##testGroup##_##testName##_TestShell : public UtestShell        \
    {                                                                          \
        virtual Utest *createTest() CPPUTEST_OVERRIDE                          \
        {                                                                      \
            return new TEST_##testGroup##_##testName##_Test;                   \
        }                                                                      \
    } TEST_##testGroup##_##testName##_TestShell_instance;                      \
    static TestInstaller TEST_##testGroup##_##testName##_Installer(            \
        TEST_##testGroup##_##testName##_TestShell_instance, #testGroup,        \
        #testName, __FILE__, __LINE__);                                        \
    void TEST_##testGroup##_##testName##_Test::testBody()

/* Upstream quirk preserved: the installer name has no underscore between
 * group and name (TEST_##testGroup##testName##_Installer). */
#define IGNORE_TEST(testGroup, testName)                                       \
    class IGNORE##testGroup##_##testName##_TestShell;                          \
    extern IGNORE##testGroup##_##testName##_TestShell                          \
        IGNORE##testGroup##_##testName##_TestShell_instance;                   \
                                                                               \
    class IGNORE##testGroup##_##testName##_Test                                \
        : public TEST_GROUP_##CppUTestGroup##testGroup                         \
    {                                                                          \
      public:                                                                  \
        IGNORE##testGroup##_##testName##_Test()                                \
            : TEST_GROUP_##CppUTestGroup##testGroup()                          \
        {                                                                      \
        }                                                                      \
        virtual void testBody() CPPUTEST_OVERRIDE;                             \
    };                                                                         \
    class IGNORE##testGroup##_##testName##_TestShell                           \
        : public IgnoredUtestShell                                             \
    {                                                                          \
        virtual Utest *createTest() CPPUTEST_OVERRIDE                          \
        {                                                                      \
            return new IGNORE##testGroup##_##testName##_Test;                  \
        }                                                                      \
    } IGNORE##testGroup##_##testName##_TestShell_instance;                     \
    static TestInstaller TEST_##testGroup##testName##_Installer(               \
        IGNORE##testGroup##_##testName##_TestShell_instance, #testGroup,       \
        #testName, __FILE__, __LINE__);                                        \
    void IGNORE##testGroup##_##testName##_Test::testBody()

#define IMPORT_TEST_GROUP(testGroup)                                           \
    extern int externTestGroup##testGroup;                                     \
    extern int *p##testGroup;                                                  \
    int *p##testGroup = &externTestGroup##testGroup

#define CPPUTEST_DEFAULT_MAIN                                                  \
    /*#*/ int main(int argc, char **argv)                                      \
    {                                                                          \
        return CommandLineTestRunner::RunAllTests(argc, argv);                 \
    }

/* ------------------------------ assertions ------------------------------ */
/* Macro bodies mirror upstream UtestMacros.h, including quirks like
 * CHECK_COMPARE_LOCATION ignoring its file/line parameters and CHECK_EQUAL
 * double-evaluating its arguments (with the WARNING print on re-evaluation
 * mismatch). */

#include "CppUTest/SimpleString.h"

#define CHECK(condition)                                                       \
    CHECK_TRUE_LOCATION(condition, "CHECK", #condition, NULLPTR, __FILE__,     \
                        __LINE__)

#define CHECK_TEXT(condition, text)                                            \
    CHECK_TRUE_LOCATION((bool)(condition), "CHECK", #condition, text,          \
                        __FILE__, __LINE__)

#define CHECK_TRUE(condition)                                                  \
    CHECK_TRUE_LOCATION((bool)(condition), "CHECK_TRUE", #condition, NULLPTR,  \
                        __FILE__, __LINE__)

#define CHECK_TRUE_TEXT(condition, text)                                       \
    CHECK_TRUE_LOCATION(condition, "CHECK_TRUE", #condition, text, __FILE__,   \
                        __LINE__)

#define CHECK_FALSE(condition)                                                 \
    CHECK_FALSE_LOCATION(condition, "CHECK_FALSE", #condition, NULLPTR,        \
                         __FILE__, __LINE__)

#define CHECK_FALSE_TEXT(condition, text)                                      \
    CHECK_FALSE_LOCATION(condition, "CHECK_FALSE", #condition, text, __FILE__, \
                         __LINE__)

#define CHECK_TRUE_LOCATION(condition, checkString, conditionString, text,     \
                            file, line)                                        \
    do {                                                                       \
        cu_fast_count_check();                                                 \
        if (condition) {                                                       \
        } else                                                                 \
            cu_fail_check(checkString, conditionString, text, file, line);     \
    } while (0)

#define CHECK_FALSE_LOCATION(condition, checkString, conditionString, text,    \
                             file, line)                                       \
    do {                                                                       \
        cu_fast_count_check();                                                 \
        if (condition)                                                         \
            cu_fail_check(checkString, conditionString, text, file, line);     \
    } while (0)

#define CHECK_EQUAL(expected, actual)                                          \
    CHECK_EQUAL_LOCATION(expected, actual, NULLPTR, __FILE__, __LINE__)

#define CHECK_EQUAL_TEXT(expected, actual, text)                               \
    CHECK_EQUAL_LOCATION(expected, actual, text, __FILE__, __LINE__)

#define CHECK_EQUAL_LOCATION(expected, actual, text, file, line)               \
    do {                                                                       \
        if ((expected) != (actual)) {                                          \
            if ((actual) != (actual))                                          \
                UtestShell::getCurrent()->print(                               \
                    "WARNING:\n\tThe \"Actual Parameter\" parameter is "       \
                    "evaluated multiple times resulting in different "         \
                    "values.\n\tThus the value in the error message is "       \
                    "probably incorrect.",                                     \
                    file, line);                                               \
            if ((expected) != (expected))                                      \
                UtestShell::getCurrent()->print(                               \
                    "WARNING:\n\tThe \"Expected Parameter\" parameter is "     \
                    "evaluated multiple times resulting in different "         \
                    "values.\n\tThus the value in the error message is "       \
                    "probably incorrect.",                                     \
                    file, line);                                               \
            cu_fast_count_check();                                             \
            cu_fail_equals_strings(StringFrom(expected).asCharString(),        \
                                   StringFrom(actual).asCharString(), text,    \
                                   file, line);                                \
        } else {                                                               \
            cu_fast_count_check();                                             \
        }                                                                      \
    } while (0)

#define CHECK_EQUAL_ZERO(actual) CHECK_EQUAL(0, (actual))

#define CHECK_EQUAL_ZERO_TEXT(actual, text)                                    \
    CHECK_EQUAL_TEXT(0, (actual), (text))

#define CHECK_COMPARE(first, relop, second)                                    \
    CHECK_COMPARE_TEXT(first, relop, second, NULLPTR)

#define CHECK_COMPARE_TEXT(first, relop, second, text)                         \
    CHECK_COMPARE_LOCATION(first, relop, second, text, __FILE__, __LINE__)

#define CHECK_COMPARE_LOCATION(first, relop, second, text, file, line)         \
    do {                                                                       \
        bool success = (first)relop(second);                                   \
        if (!success) {                                                        \
            SimpleString conditionString;                                      \
            conditionString += StringFrom(first);                              \
            conditionString += " ";                                            \
            conditionString += #relop;                                         \
            conditionString += " ";                                            \
            conditionString += StringFrom(second);                             \
            UtestShell::getCurrent()->assertCompare(                           \
                false, "CHECK_COMPARE", conditionString.asCharString(), text,  \
                __FILE__, __LINE__);                                           \
        }                                                                      \
    } while (0)

#define STRCMP_EQUAL(expected, actual)                                         \
    STRCMP_EQUAL_LOCATION(expected, actual, NULLPTR, __FILE__, __LINE__)

#define STRCMP_EQUAL_TEXT(expected, actual, text)                              \
    STRCMP_EQUAL_LOCATION(expected, actual, text, __FILE__, __LINE__)

#define STRCMP_EQUAL_LOCATION(expected, actual, text, file, line)              \
    do {                                                                       \
        cu_fast_count_check();                                                 \
        cu_strcmp_check((expected), (actual), text, file, line);               \
    } while (0)

#define STRNCMP_EQUAL(expected, actual, length)                                \
    STRNCMP_EQUAL_LOCATION(expected, actual, length, NULLPTR, __FILE__,        \
                           __LINE__)

#define STRNCMP_EQUAL_TEXT(expected, actual, length, text)                     \
    STRNCMP_EQUAL_LOCATION(expected, actual, length, text, __FILE__, __LINE__)

#define STRNCMP_EQUAL_LOCATION(expected, actual, length, text, file, line)     \
    do {                                                                       \
        cu_fast_count_check();                                                 \
        cu_strncmp_check((expected), (actual), (length), text, file, line);    \
    } while (0)

#define STRCMP_NOCASE_EQUAL(expected, actual)                                  \
    STRCMP_NOCASE_EQUAL_LOCATION(expected, actual, NULLPTR, __FILE__, __LINE__)

#define STRCMP_NOCASE_EQUAL_TEXT(expected, actual, text)                       \
    STRCMP_NOCASE_EQUAL_LOCATION(expected, actual, text, __FILE__, __LINE__)

#define STRCMP_NOCASE_EQUAL_LOCATION(expected, actual, text, file, line)       \
    do {                                                                       \
        UtestShell::getCurrent()->assertCstrNoCaseEqual(expected, actual,      \
                                                        text, file, line);     \
    } while (0)

#define STRCMP_CONTAINS(expected, actual)                                      \
    STRCMP_CONTAINS_LOCATION(expected, actual, NULLPTR, __FILE__, __LINE__)

#define STRCMP_CONTAINS_TEXT(expected, actual, text)                           \
    STRCMP_CONTAINS_LOCATION(expected, actual, text, __FILE__, __LINE__)

#define STRCMP_CONTAINS_LOCATION(expected, actual, text, file, line)           \
    do {                                                                       \
        UtestShell::getCurrent()->assertCstrContains(expected, actual, text,   \
                                                     file, line);              \
    } while (0)

#define STRCMP_NOCASE_CONTAINS(expected, actual)                               \
    STRCMP_NOCASE_CONTAINS_LOCATION(expected, actual, NULLPTR, __FILE__,       \
                                    __LINE__)

#define STRCMP_NOCASE_CONTAINS_TEXT(expected, actual, text)                    \
    STRCMP_NOCASE_CONTAINS_LOCATION(expected, actual, text, __FILE__, __LINE__)

#define STRCMP_NOCASE_CONTAINS_LOCATION(expected, actual, text, file, line)    \
    do {                                                                       \
        UtestShell::getCurrent()->assertCstrNoCaseContains(expected, actual,   \
                                                           text, file, line);  \
    } while (0)

#define LONGS_EQUAL(expected, actual)                                          \
    LONGS_EQUAL_LOCATION((expected), (actual),                                 \
                         "LONGS_EQUAL(" #expected ", " #actual ") failed",     \
                         __FILE__, __LINE__)

#define LONGS_EQUAL_TEXT(expected, actual, text)                               \
    LONGS_EQUAL_LOCATION((expected), (actual), text, __FILE__, __LINE__)

#define UNSIGNED_LONGS_EQUAL(expected, actual)                                 \
    UNSIGNED_LONGS_EQUAL_LOCATION((expected), (actual), NULLPTR, __FILE__,     \
                                  __LINE__)

#define UNSIGNED_LONGS_EQUAL_TEXT(expected, actual, text)                      \
    UNSIGNED_LONGS_EQUAL_LOCATION((expected), (actual), text, __FILE__,        \
                                  __LINE__)

#define LONGS_EQUAL_LOCATION(expected, actual, text, file, line)               \
    do {                                                                       \
        long cu_fp_e = (long)(expected);                                       \
        long cu_fp_a = (long)(actual);                                         \
        cu_fast_count_check();                                                 \
        if (cu_fp_e == cu_fp_a) {                                              \
        } else                                                                 \
            cu_fail_longs(cu_fp_e, cu_fp_a, text, file, line);                 \
    } while (0)

#define UNSIGNED_LONGS_EQUAL_LOCATION(expected, actual, text, file, line)      \
    do {                                                                       \
        unsigned long cu_fp_e = (unsigned long)(expected);                     \
        unsigned long cu_fp_a = (unsigned long)(actual);                       \
        cu_fast_count_check();                                                 \
        if (cu_fp_e == cu_fp_a) {                                              \
        } else                                                                 \
            cu_fail_unsigned_longs(cu_fp_e, cu_fp_a, text, file, line);        \
    } while (0)

#if CPPUTEST_USE_LONG_LONG

#define LONGLONGS_EQUAL(expected, actual)                                      \
    LONGLONGS_EQUAL_LOCATION(expected, actual, NULLPTR, __FILE__, __LINE__)

#define LONGLONGS_EQUAL_TEXT(expected, actual, text)                           \
    LONGLONGS_EQUAL_LOCATION(expected, actual, text, __FILE__, __LINE__)

#define UNSIGNED_LONGLONGS_EQUAL(expected, actual)                             \
    UNSIGNED_LONGLONGS_EQUAL_LOCATION(expected, actual, NULLPTR, __FILE__,     \
                                      __LINE__)

#define UNSIGNED_LONGLONGS_EQUAL_TEXT(expected, actual, text)                  \
    UNSIGNED_LONGLONGS_EQUAL_LOCATION(expected, actual, text, __FILE__,        \
                                      __LINE__)

#define LONGLONGS_EQUAL_LOCATION(expected, actual, text, file, line)           \
    do {                                                                       \
        cpputest_longlong cu_fp_e = (cpputest_longlong)(expected);             \
        cpputest_longlong cu_fp_a = (cpputest_longlong)(actual);               \
        cu_fast_count_check();                                                 \
        if (cu_fp_e == cu_fp_a) {                                              \
        } else                                                                 \
            cu_fail_longlongs(cu_fp_e, cu_fp_a, text, file, line);             \
    } while (0)

#define UNSIGNED_LONGLONGS_EQUAL_LOCATION(expected, actual, text, file, line)  \
    do {                                                                       \
        cpputest_ulonglong cu_fp_e = (cpputest_ulonglong)(expected);           \
        cpputest_ulonglong cu_fp_a = (cpputest_ulonglong)(actual);             \
        cu_fast_count_check();                                                 \
        if (cu_fp_e == cu_fp_a) {                                              \
        } else                                                                 \
            cu_fail_unsigned_longlongs(cu_fp_e, cu_fp_a, text, file, line);    \
    } while (0)

#endif

#define BYTES_EQUAL(expected, actual)                                          \
    LONGS_EQUAL((expected) & 0xff, (actual) & 0xff)

#define BYTES_EQUAL_TEXT(expected, actual, text)                               \
    LONGS_EQUAL_TEXT((expected) & 0xff, (actual) & 0xff, text)

#define SIGNED_BYTES_EQUAL(expected, actual)                                   \
    SIGNED_BYTES_EQUAL_LOCATION(expected, actual, NULLPTR, __FILE__, __LINE__)

#define SIGNED_BYTES_EQUAL_LOCATION(expected, actual, text, file, line)        \
    do {                                                                       \
        signed char cu_fp_e = (signed char)(expected);                         \
        signed char cu_fp_a = (signed char)(actual);                           \
        cu_fast_count_check();                                                 \
        if (cu_fp_e == cu_fp_a) {                                              \
        } else                                                                 \
            cu_fail_signed_bytes(cu_fp_e, cu_fp_a, text, file, line);          \
    } while (0)

#define SIGNED_BYTES_EQUAL_TEXT(expected, actual, text)                        \
    SIGNED_BYTES_EQUAL_LOCATION(expected, actual, text, __FILE__, __LINE__)

#define POINTERS_EQUAL(expected, actual)                                       \
    POINTERS_EQUAL_LOCATION((expected), (actual), NULLPTR, __FILE__, __LINE__)

#define POINTERS_EQUAL_TEXT(expected, actual, text)                            \
    POINTERS_EQUAL_LOCATION((expected), (actual), text, __FILE__, __LINE__)

#define POINTERS_EQUAL_LOCATION(expected, actual, text, file, line)            \
    do {                                                                       \
        const void *cu_fp_e = (const void *)(expected);                        \
        const void *cu_fp_a = (const void *)(actual);                          \
        cu_fast_count_check();                                                 \
        if (cu_fp_e == cu_fp_a) {                                              \
        } else                                                                 \
            cu_fail_pointers(cu_fp_e, cu_fp_a, text, file, line);              \
    } while (0)

#define FUNCTIONPOINTERS_EQUAL(expected, actual)                               \
    FUNCTIONPOINTERS_EQUAL_LOCATION((expected), (actual), NULLPTR, __FILE__,   \
                                    __LINE__)

#define FUNCTIONPOINTERS_EQUAL_TEXT(expected, actual, text)                    \
    FUNCTIONPOINTERS_EQUAL_LOCATION((expected), (actual), text, __FILE__,      \
                                    __LINE__)

#define FUNCTIONPOINTERS_EQUAL_LOCATION(expected, actual, text, file, line)    \
    do {                                                                       \
        void (*cu_fp_e)() = (void (*)())(expected);                            \
        void (*cu_fp_a)() = (void (*)())(actual);                              \
        cu_fast_count_check();                                                 \
        if (cu_fp_e == cu_fp_a) {                                              \
        } else                                                                 \
            cu_fail_functionpointers((void (*)(void))cu_fp_e,                  \
                                     (void (*)(void))cu_fp_a, text, file,      \
                                     line);                                    \
    } while (0)

#define DOUBLES_EQUAL(expected, actual, threshold)                             \
    DOUBLES_EQUAL_LOCATION((expected), (actual), (threshold), NULLPTR,         \
                           __FILE__, __LINE__)

#define DOUBLES_EQUAL_TEXT(expected, actual, threshold, text)                  \
    DOUBLES_EQUAL_LOCATION((expected), (actual), (threshold), text, __FILE__,  \
                           __LINE__)

#define DOUBLES_EQUAL_LOCATION(expected, actual, threshold, text, file, line)  \
    do {                                                                       \
        double cu_fp_e = (double)(expected);                                   \
        double cu_fp_a = (double)(actual);                                     \
        double cu_fp_t = (double)(threshold);                                  \
        cu_fast_count_check();                                                 \
        if (cu_doubles_eq(cu_fp_e, cu_fp_a, cu_fp_t)) {                        \
        } else                                                                 \
            cu_fail_doubles(cu_fp_e, cu_fp_a, cu_fp_t, text, file, line);      \
    } while (0)

#define MEMCMP_EQUAL(expected, actual, size)                                   \
    MEMCMP_EQUAL_LOCATION(expected, actual, size, NULLPTR, __FILE__, __LINE__)

#define MEMCMP_EQUAL_TEXT(expected, actual, size, text)                        \
    MEMCMP_EQUAL_LOCATION(expected, actual, size, text, __FILE__, __LINE__)

#define MEMCMP_EQUAL_LOCATION(expected, actual, size, text, file, line)        \
    do {                                                                       \
        cu_fast_count_check();                                                 \
        cu_memcmp_check((expected), (actual), (size), text, file, line);       \
    } while (0)

#define BITS_EQUAL(expected, actual, mask)                                     \
    BITS_LOCATION(expected, actual, mask, NULLPTR, __FILE__, __LINE__)

#define BITS_EQUAL_TEXT(expected, actual, mask, text)                          \
    BITS_LOCATION(expected, actual, mask, text, __FILE__, __LINE__)

#define BITS_LOCATION(expected, actual, mask, text, file, line)                \
    do {                                                                       \
        unsigned long cu_fp_m = (unsigned long)(mask);                         \
        size_t cu_fp_n = sizeof(actual);                                       \
        unsigned long cu_fp_e = (unsigned long)(expected);                     \
        unsigned long cu_fp_a = (unsigned long)(actual);                       \
        cu_fast_count_check();                                                 \
        if ((cu_fp_e & cu_fp_m) == (cu_fp_a & cu_fp_m)) {                      \
        } else                                                                 \
            cu_fail_bits(cu_fp_e, cu_fp_a, cu_fp_m, cu_fp_n, text, file,       \
                         line);                                                \
    } while (0)

#define ENUMS_EQUAL_INT(expected, actual)                                      \
    ENUMS_EQUAL_TYPE(int, expected, actual)

#define ENUMS_EQUAL_INT_TEXT(expected, actual, text)                           \
    ENUMS_EQUAL_TYPE_TEXT(int, expected, actual, text)

#define ENUMS_EQUAL_TYPE(underlying_type, expected, actual)                    \
    ENUMS_EQUAL_TYPE_LOCATION(underlying_type, expected, actual, NULLPTR,      \
                              __FILE__, __LINE__)

#define ENUMS_EQUAL_TYPE_TEXT(underlying_type, expected, actual, text)         \
    ENUMS_EQUAL_TYPE_LOCATION(underlying_type, expected, actual, text,         \
                              __FILE__, __LINE__)

#define ENUMS_EQUAL_TYPE_LOCATION(underlying_type, expected, actual, text,     \
                                  file, line)                                  \
    do {                                                                       \
        underlying_type expected_underlying_value =                            \
            (underlying_type)(expected);                                       \
        underlying_type actual_underlying_value = (underlying_type)(actual);   \
        if (expected_underlying_value != actual_underlying_value) {            \
            UtestShell::getCurrent()->assertEquals(                            \
                true, StringFrom(expected_underlying_value).asCharString(),    \
                StringFrom(actual_underlying_value).asCharString(), text,      \
                file, line);                                                   \
        } else {                                                               \
            UtestShell::getCurrent()->assertLongsEqual((long)0, (long)0,       \
                                                       NULLPTR, file, line);   \
        }                                                                      \
    } while (0)

#define FAIL(text) FAIL_LOCATION(text, __FILE__, __LINE__)

#define FAIL_LOCATION(text, file, line)                                        \
    do {                                                                       \
        UtestShell::getCurrent()->fail(text, file, line);                      \
    } while (0)

#define FAIL_TEST(text) FAIL_TEST_LOCATION(text, __FILE__, __LINE__)

#define FAIL_TEST_LOCATION(text, file, line)                                   \
    do {                                                                       \
        UtestShell::getCurrent()->fail(text, file, line);                      \
    } while (0)

#define TEST_EXIT                                                              \
    do {                                                                       \
        UtestShell::getCurrent()->exitTest();                                  \
    } while (0)

#define UT_PRINT_LOCATION(text, file, line)                                    \
    do {                                                                       \
        UtestShell::getCurrent()->print(text, file, line);                     \
    } while (0)

#define UT_PRINT(text) UT_PRINT_LOCATION(text, __FILE__, __LINE__)

#if CPPUTEST_HAVE_EXCEPTIONS
#define CHECK_THROWS(expected, expression)                                     \
    do {                                                                       \
        SimpleString failure_msg("expected to throw " #expected                \
                                 "\nbut threw nothing");                       \
        bool caught_expected = false;                                          \
        try {                                                                  \
            (expression);                                                      \
        } catch (const expected &) {                                           \
            caught_expected = true;                                            \
        } catch (...) {                                                        \
            failure_msg =                                                      \
                "expected to throw " #expected "\nbut threw a different type"; \
        }                                                                      \
        if (!caught_expected) {                                                \
            UtestShell::getCurrent()->fail(failure_msg.asCharString(),         \
                                           __FILE__, __LINE__);                \
        } else {                                                               \
            UtestShell::getCurrent()->countCheck();                            \
        }                                                                      \
    } while (0)
#endif

#define UT_CRASH()                                                             \
    do {                                                                       \
        UtestShell::crash();                                                   \
    } while (0)

#define RUN_ALL_TESTS(ac, av) CommandLineTestRunner::RunAllTests(ac, av)

#endif
