/* Differential COMPOSITION fuzzer: assertion macros + memory allocation
 * (plain new/delete in lite — no tracking) + mock
 * expectations + SimpleString + UT_PTR_SET, randomly interleaved inside
 * each test — the cross-API interactions a single-feature fuzzer misses
 * (e.g. the leak detector seeing user allocations around the mock core's
 * implicit internal allocations).
 *
 * Same public-API source compiles against upstream CppUTest and
 * cpputest-turbo; outputs diffed after normalization (timings, heap
 * addresses, allocation ordinals — see norm() in scripts/check-fuzz.sh).
 * Passing asserts use generated-equal operands; a random op can issue a
 * deterministic FAILING assert, which longjmps out of the test — also
 * compared behavior. Leaked blocks are fully memset so the leak report's
 * content dump is deterministic.
 *
 * 50 seeds per process: test N runs sequence FUZZ_SEED*50+N. */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fz_trace;
#define TR(...)                                                                \
    do {                                                                       \
        if (fz_trace)                                                          \
            fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

static unsigned long long fz_rng;

static unsigned fz_r(void)
{
    fz_rng ^= fz_rng << 13;
    fz_rng ^= fz_rng >> 7;
    fz_rng ^= fz_rng << 17;
    return (unsigned)(fz_rng >> 32);
}

static const char *const kStrings[] = {"alpha", "Beta", "alphabet soup", ""};

enum FzEnum { FZ_A = 1, FZ_B = 2 };

typedef void (*fz_fp_t)(void);

/* small stash so allocations survive across ops within a test and get
 * freed (or deliberately leaked) later */
static char *stash[4];
static size_t stash_sz[4];

static void stash_free_all(void)
{
    for (int i = 0; i < 4; i++) {
        delete[] stash[i];
        stash[i] = 0;
    }
}

static void passing_int_asserts(void)
{
    long v = (long)(fz_r() % 1000) - 500;
    TR(" ok-ints(%ld)", v);
    LONGS_EQUAL(v, v);
    UNSIGNED_LONGS_EQUAL((unsigned long)(v + 500), (unsigned long)(v + 500));
    LONGLONGS_EQUAL((long long)v * 7, (long long)v * 7);
    BYTES_EQUAL((unsigned char)(v & 0xFF), (unsigned char)(v & 0xFF));
    SIGNED_BYTES_EQUAL((signed char)(v & 0x7F), (signed char)(v & 0x7F));
    ENUMS_EQUAL_INT(FZ_A, FZ_A);
    BITS_EQUAL(0xA5, 0xA5, 0xFF);
}

static void passing_string_asserts(void)
{
    const char *s = kStrings[fz_r() % 4];
    TR(" ok-strs(%s)", s);
    STRCMP_EQUAL(s, s);
    STRNCMP_EQUAL("alphabet", "alphabet soup", 8);
    STRCMP_NOCASE_EQUAL("Beta", "bEtA");
    STRCMP_CONTAINS("pha", "alphabet");
}

static void passing_misc_asserts(void)
{
    unsigned v = fz_r() % 100;
    TR(" ok-misc(%u)", v);
    CHECK(v < 100);
    CHECK_TRUE(v + 1 > 0);
    CHECK_FALSE(v >= 100);
    CHECK_EQUAL(v, v);
    CHECK_COMPARE(v, <=, v);
    CHECK_TEXT(1 == 1, "text never shown");
    CHECK_EQUAL_TEXT((int)v, (int)v, "also never shown");
    DOUBLES_EQUAL((double)v, (double)v + 0.001, 0.01);
    POINTERS_EQUAL((void *)(size_t)0x30, (void *)(size_t)0x30);
    FUNCTIONPOINTERS_EQUAL((fz_fp_t)(size_t)0x50, (fz_fp_t)(size_t)0x50);
    const unsigned char m[4] = {1, 2, 3, 4};
    MEMCMP_EQUAL(m, m, 4);
}

static void failing_assert(void)
{
    switch (fz_r() % 8) {
    case 0:
        TR(" FAIL-longs");
        LONGS_EQUAL(12, 13);
        break;
    case 1:
        TR(" FAIL-str");
        STRCMP_EQUAL("expected words", "actual words");
        break;
    case 2:
        TR(" FAIL-check");
        CHECK_TEXT(0 != 0, "deliberate composed failure");
        break;
    case 3:
        TR(" FAIL-dbl");
        DOUBLES_EQUAL(1.0, 2.5, 0.25);
        break;
    case 4: {
        TR(" FAIL-mem");
        const unsigned char a[3] = {1, 2, 3};
        const unsigned char b[3] = {1, 9, 3};
        MEMCMP_EQUAL(a, b, 3);
        break;
    }
    case 5:
        TR(" FAIL-ptr");
        POINTERS_EQUAL((void *)(size_t)0x10, (void *)(size_t)0x20);
        break;
    case 6:
        TR(" FAIL-fail");
        FAIL("deliberate FAIL in composition fuzz");
        break;
    default:
        TR(" FAIL-bits");
        BITS_EQUAL(0x01, 0x02, 0xFF);
        break;
    }
}

static void heap_churn(void)
{
    int slot = (int)(fz_r() % 4);
    unsigned sz = 1 + fz_r() % 64;
    TR(" heap(slot%d,%u)", slot, sz);
    delete[] stash[slot];
    stash[slot] = new char[sz];
    stash_sz[slot] = sz;
    memset(stash[slot], (int)(fz_r() % 256), sz);
    if (fz_r() % 3 == 0) {
        int *one = new int((int)(fz_r() % 50));
        printf("V heap=%d\n", *one);
        delete one;
    }
}

static void sstring_ops(void)
{
    switch (fz_r() % 4) {
    case 0: {
        int v = (int)(fz_r() % 1000) - 500;
        SimpleString s = StringFromFormat("fmt:%d:%s", v, kStrings[fz_r() % 4]);
        printf("V ss=%s\n", s.asCharString());
        break;
    }
    case 1: {
        unsigned long v = fz_r() % 100000;
        printf("V hex=%s\n", HexStringFrom(v).asCharString());
        break;
    }
    case 2: {
        SimpleString a(kStrings[fz_r() % 4]);
        SimpleString b = a + SimpleString("-tail");
        printf("V cat=%s len=%d\n", b.asCharString(), (int)b.size());
        break;
    }
    default: {
        SimpleString a(kStrings[fz_r() % 4]);
        printf("V cmp=%d sub=%s\n", (int)(a == SimpleString("Beta")),
               a.subString(0, 3).asCharString());
        break;
    }
    }
}

static void mock_interleave(void)
{
    switch (fz_r() % 3) {
    case 0: {
        int v = (int)(fz_r() % 10);
        TR(" mock-pair(%d)", v);
        mock().expectOneCall("compose").withParameter("x", v).andReturnValue(v +
                                                                             1);
        printf("V mock=%d\n", mock()
                                  .actualCall("compose")
                                  .withParameter("x", v)
                                  .returnIntValue());
        break;
    }
    case 1:
        TR(" mock-expect(open)");
        /* expectation left open on purpose: checkExpectations at test end
         * reports it (compared), and the implicit expectation-store
         * allocations interleave with the user heap ops around it */
        mock().expectOneCall("dangling").withParameter("y", 3);
        break;
    default:
        TR(" mock-check");
        mock().checkExpectations();
        break;
    }
}

static void throws_op(void)
{
    TR(" throws");
    CHECK_THROWS(int, throw 42);
}

static void fz_sequence(unsigned long long seed)
{
    fz_rng = seed * 2654435761ull + 12345;
    unsigned ops = 4 + fz_r() % 12;
    TR("--- compose seed %llu (%u ops)\n", seed, ops);

    for (unsigned i = 0; i < ops; i++) {
        switch (fz_r() % 16) {
        case 0:
        case 1:
            passing_int_asserts();
            break;
        case 2:
        case 3:
            passing_string_asserts();
            break;
        case 4:
        case 5:
            passing_misc_asserts();
            break;
        case 6:
        case 7:
            heap_churn();
            break;
        case 8:
            heap_churn();
            break;
        case 9:
            sstring_ops();
            break;
        case 10:
        case 11:
            mock_interleave();
            break;
        case 12:
            passing_int_asserts();
            break;
        case 13:
            throws_op();
            break;
        case 14:
            if (fz_r() % 2)
                failing_assert(); /* longjmps out; teardown cleans up */
            break;
        default:
            passing_string_asserts();
            break;
        }
        TR("\n");
    }
    mock().checkExpectations();
}

TEST_GROUP(ComposeFuzz){TEST_TEARDOWN(){mock().clear();
stash_free_all();
}
}
;

static unsigned long long base_seed;

#define FZ_TEST(N)                                                             \
    TEST(ComposeFuzz, seq##N)                                                  \
    {                                                                          \
        fz_sequence(base_seed * 50 + N);                                       \
    }

// clang-format off
FZ_TEST(0)  FZ_TEST(1)  FZ_TEST(2)  FZ_TEST(3)  FZ_TEST(4)
FZ_TEST(5)  FZ_TEST(6)  FZ_TEST(7)  FZ_TEST(8)  FZ_TEST(9)
FZ_TEST(10) FZ_TEST(11) FZ_TEST(12) FZ_TEST(13) FZ_TEST(14)
FZ_TEST(15) FZ_TEST(16) FZ_TEST(17) FZ_TEST(18) FZ_TEST(19)
FZ_TEST(20) FZ_TEST(21) FZ_TEST(22) FZ_TEST(23) FZ_TEST(24)
FZ_TEST(25) FZ_TEST(26) FZ_TEST(27) FZ_TEST(28) FZ_TEST(29)
FZ_TEST(30) FZ_TEST(31) FZ_TEST(32) FZ_TEST(33) FZ_TEST(34)
FZ_TEST(35) FZ_TEST(36) FZ_TEST(37) FZ_TEST(38) FZ_TEST(39)
FZ_TEST(40) FZ_TEST(41) FZ_TEST(42) FZ_TEST(43) FZ_TEST(44)
FZ_TEST(45) FZ_TEST(46) FZ_TEST(47) FZ_TEST(48) FZ_TEST(49)
// clang-format on

int main(int argc, char **argv)
{
    const char *seed_env = getenv("FUZZ_SEED");
    base_seed = seed_env ? strtoull(seed_env, NULL, 10) : 0;
    fz_trace = getenv("FUZZ_TRACE") != NULL;
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
