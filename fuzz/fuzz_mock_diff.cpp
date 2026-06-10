/* Differential mock fuzzer.
 *
 * Uses ONLY the public CppUTest/CppUMock API, so the same source compiles
 * against upstream CppUTest and cpputest-turbo. A seed drives a
 * deterministic random sequence of mock operations inside each test; both
 * binaries are run with the same FUZZ_SEED and their outputs diffed
 * (timing-normalized). Any divergence — different failure messages,
 * different counts, different pass/fail — is a behavioral bug in one of
 * the two implementations.
 *
 * 50 seeds per process: test N runs sequence FUZZ_SEED*50+N. Failures
 * abort that one test (longjmp/exception), which is itself part of the
 * compared behavior. */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include <stdio.h>
#include <stdlib.h>

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

static const char *const kNames[] = {"alpha", "beta", "gamma"};
static const char *const kParams[] = {"a", "b", "c"};
static const char *const kStrings[] = {"x", "yy", ""};
static const char *const kScopes[] = {"", "s1"};

static unsigned char out_src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
static unsigned char out_dst[8];
static int fz_objs[3] = {10, 20, 30};
static int fz_copy_dst;
static const unsigned char fz_membuf[3] = {0xAA, 0xBB, 0xCC};

/* deterministic custom type "FZT": compares/prints the pointed-to int */
class FzComparator : public MockNamedValueComparator
{
  public:
    bool isEqual(const void *object1, const void *object2) CPPUTEST_OVERRIDE
    {
        return *(const int *)object1 == *(const int *)object2;
    }
    SimpleString valueToString(const void *object) CPPUTEST_OVERRIDE
    {
        return StringFrom(*(const int *)object);
    }
};

class FzCopier : public MockNamedValueCopier
{
  public:
    void copy(void *out, const void *in) CPPUTEST_OVERRIDE
    {
        *(int *)out = *(const int *)in;
    }
};

static FzComparator fz_comparator;
static FzCopier fz_copier;

static const char *last_scope;

static MockSupport &pick_mock(void)
{
    last_scope = kScopes[fz_r() % 2];
    return mock(last_scope);
}

static void add_expect_params(MockExpectedCall &e)
{
    unsigned n = fz_r() % 4;
    for (unsigned i = 0; i < n; i++) {
        const char *p = kParams[fz_r() % 3];
        switch (fz_r() % 9) {
        case 6: {
            int idx = (int)(fz_r() % 3);
            TR(" ep-oftype(%s=obj%d)", p, idx);
            e.withParameterOfType("FZT", p, &fz_objs[idx]);
            break;
        }
        case 7: {
            unsigned sz = 1 + fz_r() % 3;
            TR(" ep-membuf(%s,%u)", p, sz);
            e.withParameter(p, fz_membuf, sz);
            break;
        }
        case 8: {
            int idx = (int)(fz_r() % 3);
            TR(" ep-outoftype(%s=obj%d)", p, idx);
            e.withOutputParameterOfTypeReturning("FZT", p, &fz_objs[idx]);
            break;
        }
        case 0: {
            int v = (int)(fz_r() % 3);
            TR(" ep-int(%s=%d)", p, v);
            e.withParameter(p, v);
            break;
        }
        case 1: {
            unsigned long v = fz_r() % 3;
            TR(" ep-ul(%s=%lu)", p, v);
            e.withParameter(p, v);
            break;
        }
        case 2: {
            const char *v = kStrings[fz_r() % 3];
            TR(" ep-str(%s=%s)", p, v);
            e.withParameter(p, v);
            break;
        }
        case 3: {
            double v = (double)(fz_r() % 3);
            TR(" ep-dbl(%s=%g)", p, v);
            e.withParameter(p, v, 0.25);
            break;
        }
        case 4: {
            void *v = (void *)(size_t)(0x10 + (fz_r() % 3) * 0x10);
            TR(" ep-ptr(%s=%p)", p, v);
            e.withParameter(p, v);
            break;
        }
        default: {
            unsigned sz = fz_r() % 2 ? 4u : 8u;
            TR(" ep-out(%s,%u)", p, sz);
            e.withOutputParameterReturning(p, out_src, sz);
            break;
        }
        }
    }
    if (fz_r() % 4 == 0)
        e.ignoreOtherParameters();
    if (fz_r() % 5 == 0) {
        unsigned idx = fz_r() % 2;
        TR(" ep-onObject(0x%x)", 0x1000u + idx * 0x1000u);
        e.onObject((void *)(size_t)(0x1000 + idx * 0x1000));
    }
    if (fz_r() % 3 == 0) {
        switch (fz_r() % 3) {
        case 0:
            e.andReturnValue((int)(fz_r() % 100));
            break;
        case 1:
            e.andReturnValue(kStrings[fz_r() % 3]);
            break;
        default:
            e.andReturnValue(1.5);
            break;
        }
    }
}

static void add_actual_params(MockActualCall &a)
{
    unsigned n = fz_r() % 4;
    for (unsigned i = 0; i < n; i++) {
        const char *p = kParams[fz_r() % 3];
        switch (fz_r() % 9) {
        case 6: {
            int idx = (int)(fz_r() % 3);
            TR(" ap-oftype(%s=obj%d)", p, idx);
            a.withParameterOfType("FZT", p, &fz_objs[idx]);
            break;
        }
        case 7: {
            unsigned sz = 1 + fz_r() % 3;
            TR(" ap-membuf(%s,%u)", p, sz);
            a.withParameter(p, fz_membuf, sz);
            break;
        }
        case 8: {
            TR(" ap-outoftype(%s)", p);
            a.withOutputParameterOfType("FZT", p, &fz_copy_dst);
            break;
        }
        case 0: {
            int v = (int)(fz_r() % 3);
            TR(" ap-int(%s=%d)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 1: {
            unsigned long v = fz_r() % 3;
            TR(" ap-ul(%s=%lu)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 2: {
            const char *v = kStrings[fz_r() % 3];
            TR(" ap-str(%s=%s)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 3: {
            double v = (double)(fz_r() % 3);
            TR(" ap-dbl(%s=%g)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 4: {
            void *v = (void *)(size_t)(0x10 + (fz_r() % 3) * 0x10);
            TR(" ap-ptr(%s=%p)", p, v);
            a.withParameter(p, v);
            break;
        }
        default: {
            TR(" ap-out(%s)", p);
            a.withOutputParameter(p, out_dst);
            break;
        }
        }
    }
}

static void fz_sequence(unsigned long long seed)
{
    fz_rng = seed * 2654435761ull + 12345;
    unsigned ops = 6 + fz_r() % 18;
    TR("--- seq seed %llu (%u ops)\n", seed, ops);

    mock().installComparator("FZT", fz_comparator);
    mock().installCopier("FZT", fz_copier);

    for (unsigned i = 0; i < ops; i++) {
        switch (fz_r() % 10) {
        case 0:
        case 1:
        case 2: { /* expectation */
            MockSupport &m = pick_mock();
            const char *name = kNames[fz_r() % 3];
            unsigned kind = fz_r() % 4;
            if (kind == 0) {
                TR("[%s].expectOneCall(%s)", last_scope, name);
                add_expect_params(m.expectOneCall(name));
            } else if (kind == 1) {
                unsigned n = 1 + fz_r() % 3;
                TR("[%s].expectNCalls(%u,%s)", last_scope, n, name);
                add_expect_params(m.expectNCalls(n, name));
            } else if (kind == 2) {
                TR("[%s].expectNoCall(%s)", last_scope, name);
                m.expectNoCall(name);
            } else {
                TR("[%s].expectOneCall(%s)", last_scope, name);
                add_expect_params(m.expectOneCall(name));
            }
            TR("\n");
            break;
        }
        case 3:
        case 4:
        case 5:
        case 6: { /* actual call */
            MockSupport &m = pick_mock();
            const char *an = kNames[fz_r() % 3];
            TR("[%s].actualCall(%s)", last_scope, an);
            MockActualCall &a = m.actualCall(an);
            if (fz_r() % 5 == 0) {
                unsigned idx = fz_r() % 3; /* 0x3000 never expected */
                TR(" ap-onObject(0x%x)", 0x1000u + idx * 0x1000u);
                a.onObject((const void *)(size_t)(0x1000 + idx * 0x1000));
            }
            add_actual_params(a);
            if (fz_r() % 4 == 0) {
                switch (fz_r() % 4) {
                case 0:
                    TR(" retIntOrDefault");
                    a.returnIntValueOrDefault(-1);
                    break;
                case 1:
                    TR(" retStringOrDefault");
                    a.returnStringValueOrDefault("dflt");
                    break;
                case 2:
                    TR(" retDoubleOrDefault");
                    a.returnDoubleValueOrDefault(0.5);
                    break;
                default:
                    TR(" retULongOrDefault");
                    a.returnUnsignedLongIntValueOrDefault(9);
                    break;
                }
            }
            TR("\n");
            break;
        }
        case 7:
            if (fz_r() % 2) {
                MockSupport &m7 = pick_mock();
                TR("[%s].strictOrder\n", last_scope);
                m7.strictOrder();
            } else {
                MockSupport &m7 = pick_mock();
                TR("[%s].ignoreOtherCalls\n", last_scope);
                m7.ignoreOtherCalls();
            }
            break;
        case 8:
            if (fz_r() % 3 == 0) {
                MockSupport &m8 = pick_mock();
                TR("[%s].disable\n", last_scope);
                m8.disable();
            } else {
                MockSupport &m8 = pick_mock();
                TR("[%s].enable\n", last_scope);
                m8.enable();
            }
            break;
        default:
            switch (fz_r() % 5) {
            case 0:
                TR("[].clear\n");
                mock().clear();
                break;
            case 1:
                TR("[s1].clear\n");
                mock("s1").clear();
                break;
            case 2:
                TR("[].expectedCallsLeft\n");
                (void)mock().expectedCallsLeft();
                break;
            default:
                TR("[].checkExpectations\n");
                mock().checkExpectations();
                break;
            }
            break;
        }
    }
    mock().checkExpectations();
}

TEST_GROUP(MockFuzz){TEST_TEARDOWN(){mock().clear();
mock().removeAllComparatorsAndCopiers();
}
}
;

static unsigned long long base_seed;

#define FZ_TEST(N)                                                             \
    TEST(MockFuzz, seq##N)                                                     \
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
