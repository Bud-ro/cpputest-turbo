#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <math.h>

/* One failing assertion per test; the golden file pins every failure
 * message byte-for-byte. Runs in reverse declaration order. */

TEST_GROUP(Fails)
{
};

TEST(Fails, checkEqual)
{
    CHECK_EQUAL(12345, 12367);
}

TEST(Fails, checkEqualText)
{
    CHECK_EQUAL_TEXT(1, 2, "custom message");
}

TEST(Fails, checkEqualStringsPrintable)
{
    STRCMP_EQUAL("hello\nworld", "hello\tworld");
}

TEST(Fails, checkCompare)
{
    CHECK_COMPARE(1, >, 2);
}

TEST(Fails, strcmpEqual)
{
    STRCMP_EQUAL("hello", "world");
}

TEST(Fails, strncmpEqual)
{
    STRNCMP_EQUAL("hello", "hellp", 5);
}

TEST(Fails, strcmpNocaseEqual)
{
    STRCMP_NOCASE_EQUAL("HELLO", "world");
}

TEST(Fails, strcmpContains)
{
    STRCMP_CONTAINS("xyz", "hello world");
}

TEST(Fails, strcmpNocaseContains)
{
    STRCMP_NOCASE_CONTAINS("XYZ", "hello");
}

TEST(Fails, longsEqual)
{
    LONGS_EQUAL(1, 100);
}

TEST(Fails, longsEqualText)
{
    LONGS_EQUAL_TEXT(1, 100, "size mismatch");
}

TEST(Fails, unsignedLongsEqual)
{
    UNSIGNED_LONGS_EQUAL(7, 8);
}

TEST(Fails, longlongsEqual)
{
    LONGLONGS_EQUAL(0x100000000LL, 0x100000001LL);
}

TEST(Fails, unsignedLonglongsEqual)
{
    UNSIGNED_LONGLONGS_EQUAL(1ULL, 2ULL);
}

TEST(Fails, bytesEqual)
{
    BYTES_EQUAL(0x01, 0x02);
}

TEST(Fails, signedBytesEqual)
{
    SIGNED_BYTES_EQUAL(-1, 2);
}

TEST(Fails, pointersEqual)
{
    POINTERS_EQUAL((void *)0x12, (void *)0x34);
}

TEST(Fails, functionPointersEqual)
{
    FUNCTIONPOINTERS_EQUAL((void (*)())0x1, (void (*)())0x2);
}

TEST(Fails, doublesEqual)
{
    DOUBLES_EQUAL(1.0, 2.0, 0.5);
}

TEST(Fails, doublesEqualNan)
{
    DOUBLES_EQUAL(nan(""), 2.0, 0.5);
}

TEST(Fails, memcmpEqual)
{
    MEMCMP_EQUAL("ABC", "ABD", 3);
}

TEST(Fails, bitsEqual)
{
    unsigned char actual = 0x02;
    BITS_EQUAL(0x01, actual, 0xFF);
}

TEST(Fails, enumsEqualInt)
{
    ENUMS_EQUAL_INT(1, 2);
}

#if CPPUTEST_HAVE_EXCEPTIONS
struct ExpectedError {};
struct OtherError {};

static void throwNothing() {}
static void throwOther() { throw OtherError(); }

TEST(Fails, checkThrowsNothing)
{
    CHECK_THROWS(ExpectedError, throwNothing());
}

TEST(Fails, checkThrowsWrongType)
{
    CHECK_THROWS(ExpectedError, throwOther());
}
#endif

TEST_GROUP(Passes)
{
};

#if CPPUTEST_HAVE_EXCEPTIONS
static void throwExpected() { throw ExpectedError(); }
#endif

TEST(Passes, allAssertionsPass)
{
    CHECK(true);
    CHECK_TRUE(1 == 1);
    CHECK_FALSE(1 == 2);
    CHECK_EQUAL(3, 3);
    CHECK_EQUAL_ZERO(0);
    CHECK_COMPARE(2, >, 1);
    STRCMP_EQUAL("a", "a");
    STRNCMP_EQUAL("ab", "ac", 1);
    STRCMP_NOCASE_EQUAL("AbC", "aBc");
    STRCMP_CONTAINS("ell", "hello");
    STRCMP_NOCASE_CONTAINS("ELL", "hello");
    STRCMP_EQUAL(NULLPTR, NULLPTR);
    LONGS_EQUAL(5, 5);
    UNSIGNED_LONGS_EQUAL(5, 5);
    LONGLONGS_EQUAL(5, 5);
    UNSIGNED_LONGLONGS_EQUAL(5, 5);
    BYTES_EQUAL(0x102, 0x202); /* only low byte compared */
    SIGNED_BYTES_EQUAL(-3, -3);
    POINTERS_EQUAL(NULLPTR, NULLPTR);
    DOUBLES_EQUAL(1.0, 1.4, 0.5);
    DOUBLES_EQUAL(INFINITY, -INFINITY, 0.0); /* both inf == equal, quirk */
    MEMCMP_EQUAL("AB", "AB", 2);
    MEMCMP_EQUAL(NULLPTR, NULLPTR, 0);
    BITS_EQUAL(0x0F, 0x4F, 0x0F);
    ENUMS_EQUAL_INT(2, 2);
#if CPPUTEST_HAVE_EXCEPTIONS
    CHECK_THROWS(ExpectedError, throwExpected());
#endif
}

TEST(Passes, utPrint)
{
    UT_PRINT("a user message");
}

CPPUTEST_DEFAULT_MAIN
