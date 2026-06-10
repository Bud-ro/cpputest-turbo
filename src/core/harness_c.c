#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <stdio.h>

/* C implementations of the TestHarness_c.h *_LOCATION functions, mirroring
 * upstream TestHarness_c.cpp dispatch (which assertion each type maps to). */

void CHECK_EQUAL_C_BOOL_LOCATION(int expected, int actual, const char *text,
                                 const char *fileName, size_t lineNumber)
{
    cu_assert_equals(!!expected != !!actual, expected ? "true" : "false",
                     actual ? "true" : "false", text, fileName, lineNumber);
}

void CHECK_EQUAL_C_INT_LOCATION(int expected, int actual, const char *text,
                                const char *fileName, size_t lineNumber)
{
    cu_assert_longs_equal((long)expected, (long)actual, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_UINT_LOCATION(unsigned int expected, unsigned int actual,
                                 const char *text, const char *fileName,
                                 size_t lineNumber)
{
    cu_assert_unsigned_longs_equal((unsigned long)expected, (unsigned long)actual,
                                   text, fileName, lineNumber);
}

void CHECK_EQUAL_C_LONG_LOCATION(long expected, long actual, const char *text,
                                 const char *fileName, size_t lineNumber)
{
    cu_assert_longs_equal(expected, actual, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_ULONG_LOCATION(unsigned long expected, unsigned long actual,
                                  const char *text, const char *fileName,
                                  size_t lineNumber)
{
    cu_assert_unsigned_longs_equal(expected, actual, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_LONGLONG_LOCATION(long long expected, long long actual,
                                     const char *text, const char *fileName,
                                     size_t lineNumber)
{
    cu_assert_longlongs_equal(expected, actual, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_ULONGLONG_LOCATION(unsigned long long expected,
                                      unsigned long long actual,
                                      const char *text, const char *fileName,
                                      size_t lineNumber)
{
    cu_assert_unsigned_longlongs_equal(expected, actual, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_REAL_LOCATION(double expected, double actual, double threshold,
                                 const char *text, const char *fileName,
                                 size_t lineNumber)
{
    cu_assert_doubles_equal(expected, actual, threshold, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_CHAR_LOCATION(char expected, char actual, const char *text,
                                 const char *fileName, size_t lineNumber)
{
    char e[2] = { expected, '\0' };
    char a[2] = { actual, '\0' };
    cu_assert_equals(expected != actual, e, a, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_UBYTE_LOCATION(unsigned char expected, unsigned char actual,
                                  const char *text, const char *fileName,
                                  size_t lineNumber)
{
    char e[16], a[16];
    snprintf(e, sizeof e, "%d", (int)expected);
    snprintf(a, sizeof a, "%d", (int)actual);
    cu_assert_equals(expected != actual, e, a, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_SBYTE_LOCATION(signed char expected, signed char actual,
                                  const char *text, const char *fileName,
                                  size_t lineNumber)
{
    char e[16], a[16];
    snprintf(e, sizeof e, "%d", (int)expected);
    snprintf(a, sizeof a, "%d", (int)actual);
    cu_assert_equals(expected != actual, e, a, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_STRING_LOCATION(const char *expected, const char *actual,
                                   const char *text, const char *fileName,
                                   size_t lineNumber)
{
    cu_assert_cstr_equal(expected, actual, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_POINTER_LOCATION(const void *expected, const void *actual,
                                    const char *text, const char *fileName,
                                    size_t lineNumber)
{
    cu_assert_pointers_equal(expected, actual, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_MEMCMP_LOCATION(const void *expected, const void *actual,
                                   size_t size, const char *text,
                                   const char *fileName, size_t lineNumber)
{
    cu_assert_binary_equal(expected, actual, size, text, fileName, lineNumber);
}

void CHECK_EQUAL_C_BITS_LOCATION(unsigned int expected, unsigned int actual,
                                 unsigned int mask, size_t size,
                                 const char *text, const char *fileName,
                                 size_t lineNumber)
{
    cu_assert_bits_equal(expected, actual, mask, size, text, fileName, lineNumber);
}

void FAIL_TEXT_C_LOCATION(const char *text, const char *fileName, size_t lineNumber)
{
    cu_fail(text, fileName, lineNumber);
}

void FAIL_C_LOCATION(const char *fileName, size_t lineNumber)
{
    cu_fail("", fileName, lineNumber);
}

void CHECK_C_LOCATION(int condition, const char *conditionString,
                      const char *text, const char *fileName, size_t lineNumber)
{
    cu_assert_true(condition != 0, "CHECK_C", conditionString, text,
                   fileName, lineNumber);
}
