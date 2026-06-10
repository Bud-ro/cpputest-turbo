#include "CppUTest/TestHarness_c.h"

#include <stddef.h>

static int setup_called;

TEST_GROUP_C_SETUP(CGroup)
{
    setup_called++;
}

TEST_GROUP_C_TEARDOWN(CGroup)
{
}

TEST_C(CGroup, fails)
{
    CHECK_EQUAL_C_INT(1, 2);
}

TEST_C(CGroup, passes)
{
    CHECK_C(setup_called > 0);
    CHECK_C_TEXT(setup_called < 100, "sanity");
    CHECK_EQUAL_C_BOOL(1, 2); /* both truthy */
    CHECK_EQUAL_C_INT(1, 1);
    CHECK_EQUAL_C_UINT(2u, 2u);
    CHECK_EQUAL_C_LONG(3L, 3L);
    CHECK_EQUAL_C_ULONG(4UL, 4UL);
    CHECK_EQUAL_C_LONGLONG(5LL, 5LL);
    CHECK_EQUAL_C_ULONGLONG(6ULL, 6ULL);
    CHECK_EQUAL_C_REAL(1.0, 1.1, 0.2);
    CHECK_EQUAL_C_CHAR('a', 'a');
    CHECK_EQUAL_C_UBYTE(200, 200);
    CHECK_EQUAL_C_SBYTE(-5, -5);
    CHECK_EQUAL_C_STRING("hi", "hi");
    CHECK_EQUAL_C_POINTER(NULL, NULL);
    CHECK_EQUAL_C_MEMCMP("ab", "ab", 2);
    {
        unsigned char actual = 0x0F;
        CHECK_EQUAL_C_BITS(0x0F, actual, 0xFF);
    }
    {
        char *p = (char *)cpputest_malloc(10);
        p = (char *)cpputest_realloc(p, 20);
        cpputest_free(p);
        p = cpputest_strdup("hello");
        cpputest_free(p);
    }
}

IGNORE_TEST_C(CGroup, ignored)
{
    FAIL_C();
}
