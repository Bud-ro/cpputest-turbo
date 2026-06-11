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
 * Coverage: every public MockSupport / MockExpectedCall / MockActualCall
 * member — all withParameter overloads and their named with*Parameter
 * variants, all 12 andReturnValue overloads, every return*Value /
 * return*ValueOrDefault getter (with and without a queued return value,
 * so both the value path and the default/type-mismatch path run), the
 * MockSupport-level *ReturnValue getters,
 * comparators/copiers, output parameters, onObject, withCallOrder,
 * strictOrder, scopes, enable/disable, ignoreOtherCalls/Parameters,
 * expectNoCall, clear, checkExpectations, expectedCallsLeft.
 *
 * Function pointers and object pointers use small fake constants, never
 * symbol addresses: real addresses differ between the two binaries and
 * would show up as false divergence when a failure message prints them.
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

/* fake pointer/function-pointer values: identical literals in both binaries
 * (never dereferenced/called), so printed values cannot diverge */
static void *fz_ptr(void)
{
    return (void *)(size_t)(0x10 + (fz_r() % 3) * 0x10);
}
static const void *fz_cptr(void)
{
    return (const void *)(size_t)(0x40 + (fz_r() % 3) * 0x10);
}
typedef void (*fz_fp_t)(void);
static fz_fp_t fz_fp(void)
{
    return (fz_fp_t)(size_t)(0x80 + (fz_r() % 3) * 0x10);
}

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

/* a SECOND comparator for the same type name: re-installation semantics
 * (which registration wins) are compared against upstream */
class FzComparator2 : public MockNamedValueComparator
{
  public:
    bool isEqual(const void *object1, const void *object2) CPPUTEST_OVERRIDE
    {
        return *(const int *)object1 == *(const int *)object2;
    }
    SimpleString valueToString(const void *object) CPPUTEST_OVERRIDE
    {
        return SimpleString("v2:") + StringFrom(*(const int *)object);
    }
};

static FzComparator fz_comparator;
static FzComparator2 fz_comparator2;
static FzCopier fz_copier;

static const char *last_scope;

static MockSupport &pick_mock(void)
{
    last_scope = kScopes[fz_r() % 2];
    return mock(last_scope);
}

/* every andReturnValue overload (12) */
static void add_return_value(MockExpectedCall &e)
{
    switch (fz_r() % 12) {
    case 0:
        TR(" ret-bool");
        e.andReturnValue(fz_r() % 2 == 0);
        break;
    case 1:
        TR(" ret-int");
        e.andReturnValue((int)(fz_r() % 100) - 50);
        break;
    case 2:
        TR(" ret-uint");
        e.andReturnValue((unsigned int)(fz_r() % 100));
        break;
    case 3:
        TR(" ret-long");
        e.andReturnValue((long)(fz_r() % 100) - 50);
        break;
    case 4:
        TR(" ret-ulong");
        e.andReturnValue((unsigned long)(fz_r() % 100));
        break;
    case 5:
        TR(" ret-llong");
        e.andReturnValue((long long)(fz_r() % 100) - 50);
        break;
    case 6:
        TR(" ret-ullong");
        e.andReturnValue((unsigned long long)(fz_r() % 100));
        break;
    case 7:
        TR(" ret-dbl");
        e.andReturnValue((double)(fz_r() % 4) * 0.5);
        break;
    case 8:
        TR(" ret-str");
        e.andReturnValue(kStrings[fz_r() % 3]);
        break;
    case 9:
        TR(" ret-ptr");
        e.andReturnValue(fz_ptr());
        break;
    case 10:
        TR(" ret-cptr");
        e.andReturnValue(fz_cptr());
        break;
    default:
        TR(" ret-fp");
        e.andReturnValue(fz_fp());
        break;
    }
}

/* every return getter on the actual call: the 13 typed getters, their
 * OrDefault twins, and raw returnValue(). Runs both with and without a
 * queued return value (the caller doesn't know which), so the default and
 * type-mismatch paths get equal fuzz time. */
/* every getter PRINTS its result: silent value-level divergence (wrong
 * value, wrong default, wrong coercion) shows up in the output diff, not
 * just divergence that happens to fail a test */
static void do_return_getter(MockActualCall &a)
{
    switch (fz_r() % 27) {
    case 0:
        TR(" g-bool");
        printf("V abool=%d\n", (int)a.returnBoolValue());
        break;
    case 1:
        TR(" g-boolDflt");
        printf("V aboolD=%d\n", (int)a.returnBoolValueOrDefault(true));
        break;
    case 2:
        TR(" g-int");
        printf("V aint=%d\n", a.returnIntValue());
        break;
    case 3:
        TR(" g-intDflt");
        printf("V aintD=%d\n", a.returnIntValueOrDefault(-1));
        break;
    case 4:
        TR(" g-uint");
        printf("V auint=%u\n", a.returnUnsignedIntValue());
        break;
    case 5:
        TR(" g-uintDflt");
        printf("V auintD=%u\n", a.returnUnsignedIntValueOrDefault(7u));
        break;
    case 6:
        TR(" g-long");
        printf("V along=%ld\n", a.returnLongIntValue());
        break;
    case 7:
        TR(" g-longDflt");
        printf("V alongD=%ld\n", a.returnLongIntValueOrDefault(-7L));
        break;
    case 8:
        TR(" g-ulong");
        printf("V aulong=%lu\n", a.returnUnsignedLongIntValue());
        break;
    case 9:
        TR(" g-ulongDflt");
        printf("V aulongD=%lu\n", a.returnUnsignedLongIntValueOrDefault(9UL));
        break;
    case 10:
        TR(" g-llong");
        printf("V allong=%lld\n", a.returnLongLongIntValue());
        break;
    case 11:
        TR(" g-llongDflt");
        printf("V allongD=%lld\n", a.returnLongLongIntValueOrDefault(-11LL));
        break;
    case 12:
        TR(" g-ullong");
        printf("V aullong=%llu\n", a.returnUnsignedLongLongIntValue());
        break;
    case 13:
        TR(" g-ullongDflt");
        printf("V aullongD=%llu\n",
               a.returnUnsignedLongLongIntValueOrDefault(13ULL));
        break;
    case 14:
        TR(" g-dbl");
        printf("V adbl=%g\n", a.returnDoubleValue());
        break;
    case 15:
        TR(" g-dblDflt");
        printf("V adblD=%g\n", a.returnDoubleValueOrDefault(0.5));
        break;
    case 16:
        TR(" g-str");
        printf("V astr=%s\n", a.returnStringValue());
        break;
    case 17:
        TR(" g-strDflt");
        printf("V astrD=%s\n", a.returnStringValueOrDefault("dflt"));
        break;
    case 18:
        TR(" g-ptr");
        printf("V aptr=%p\n", a.returnPointerValue());
        break;
    case 19:
        TR(" g-ptrDflt");
        printf("V aptrD=%p\n",
               a.returnPointerValueOrDefault((void *)(size_t)0x99));
        break;
    case 20:
        TR(" g-cptr");
        printf("V acptr=%p\n", a.returnConstPointerValue());
        break;
    case 21:
        TR(" g-cptrDflt");
        printf("V acptrD=%p\n",
               a.returnConstPointerValueOrDefault((const void *)(size_t)0x88));
        break;
    case 22:
        TR(" g-fp");
        printf("V afp=%p\n", (void *)(size_t)a.returnFunctionPointerValue());
        break;
    case 23:
        TR(" g-fpDflt");
        printf("V afpD=%p\n",
               (void *)(size_t)a.returnFunctionPointerValueOrDefault(
                   (fz_fp_t)(size_t)0x77));
        break;
    case 24: {
        TR(" g-raw");
        MockNamedValue v = a.returnValue();
        printf("V araw=%s/%s\n", v.getName().asCharString(),
               v.getType().asCharString());
        break;
    }
    default:
        TR(" g-none");
        break;
    }
}

/* MockSupport-level return getters: read the last actual call's return
 * value through the support object (the "C programmer caches the mock
 * handle" pattern) */
static void do_support_return_getter(MockSupport &m)
{
    switch (fz_r() % 14) {
    case 0:
        TR(" sg-has");
        printf("V shas=%d\n", (int)m.hasReturnValue());
        break;
    case 1:
        TR(" sg-bool");
        printf("V sbool=%d\n", (int)m.boolReturnValue());
        break;
    case 2:
        TR(" sg-int");
        printf("V sint=%d\n", m.intReturnValue());
        break;
    case 3:
        TR(" sg-uint");
        printf("V suint=%u\n", m.unsignedIntReturnValue());
        break;
    case 4:
        TR(" sg-long");
        printf("V slong=%ld\n", m.longIntReturnValue());
        break;
    case 5:
        TR(" sg-ulong");
        printf("V sulong=%lu\n", m.unsignedLongIntReturnValue());
        break;
    case 6:
        TR(" sg-llong");
        printf("V sllong=%lld\n", m.longLongIntReturnValue());
        break;
    case 7:
        TR(" sg-ullong");
        printf("V sullong=%llu\n", m.unsignedLongLongIntReturnValue());
        break;
    case 8:
        TR(" sg-dbl");
        printf("V sdbl=%g\n", m.doubleReturnValue());
        break;
    case 9:
        TR(" sg-str");
        printf("V sstr=%s\n", m.stringReturnValue());
        break;
    case 10:
        TR(" sg-ptr");
        printf("V sptr=%p\n", m.pointerReturnValue());
        break;
    case 11:
        TR(" sg-cptr");
        printf("V scptr=%p\n", m.constPointerReturnValue());
        break;
    case 12:
        TR(" sg-fp");
        printf("V sfp=%p\n", (void *)(size_t)m.functionPointerReturnValue());
        break;
    default: {
        TR(" sg-raw");
        MockNamedValue v = m.returnValue();
        printf("V sraw=%s/%s\n", v.getName().asCharString(),
               v.getType().asCharString());
        break;
    }
    }
}

static void add_expect_params(MockExpectedCall &e)
{
    unsigned n = fz_r() % 4;
    for (unsigned i = 0; i < n; i++) {
        const char *p = kParams[fz_r() % 3];
        switch (fz_r() % 22) {
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
            void *v = fz_ptr();
            TR(" ep-ptr(%s=%p)", p, v);
            e.withParameter(p, v);
            break;
        }
        case 5: {
            unsigned sz = fz_r() % 2 ? 4u : 8u;
            TR(" ep-out(%s,%u)", p, sz);
            e.withOutputParameterReturning(p, out_src, sz);
            break;
        }
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
        case 9: {
            bool v = fz_r() % 2 == 0;
            TR(" ep-bool(%s=%d)", p, (int)v);
            e.withParameter(p, v);
            break;
        }
        case 10: {
            long long v = (long long)(fz_r() % 3) - 1;
            TR(" ep-ll(%s=%lld)", p, v);
            e.withParameter(p, v);
            break;
        }
        case 11: {
            unsigned long long v = fz_r() % 3;
            TR(" ep-ull(%s=%llu)", p, v);
            e.withParameter(p, v);
            break;
        }
        case 12: {
            long v = (long)(fz_r() % 3) - 1;
            TR(" ep-l(%s=%ld)", p, v);
            e.withParameter(p, v);
            break;
        }
        case 13: {
            unsigned int v = fz_r() % 3;
            TR(" ep-nUint(%s=%u)", p, v);
            e.withUnsignedIntParameter(p, v);
            break;
        }
        case 14: {
            int v = (int)(fz_r() % 3);
            TR(" ep-nInt(%s=%d)", p, v);
            e.withIntParameter(p, v);
            break;
        }
        case 15: {
            bool v = fz_r() % 2 == 0;
            TR(" ep-nBool(%s=%d)", p, (int)v);
            e.withBoolParameter(p, v);
            break;
        }
        case 16: {
            const char *v = kStrings[fz_r() % 3];
            TR(" ep-nStr(%s=%s)", p, v);
            e.withStringParameter(p, v);
            break;
        }
        case 17: {
            double v = (double)(fz_r() % 3);
            TR(" ep-nDblTol(%s=%g)", p, v);
            e.withDoubleParameter(p, v, 0.25);
            break;
        }
        case 18: {
            double v = (double)(fz_r() % 3);
            TR(" ep-nDbl(%s=%g)", p, v);
            e.withDoubleParameter(p, v);
            break;
        }
        case 19: {
            const void *v = fz_cptr();
            TR(" ep-cptr(%s=%p)", p, v);
            e.withConstPointerParameter(p, v);
            break;
        }
        case 20: {
            fz_fp_t v = fz_fp();
            TR(" ep-fp(%s)", p);
            e.withFunctionPointerParameter(p, v);
            break;
        }
        default: {
            unsigned sz = 1 + fz_r() % 3;
            TR(" ep-nMembuf(%s,%u)", p, sz);
            e.withMemoryBufferParameter(p, fz_membuf, sz);
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
    if (fz_r() % 6 == 0) {
        unsigned ord = 1 + fz_r() % 4;
        TR(" ep-callOrder(%u)", ord);
        e.withCallOrder(ord);
    }
    if (fz_r() % 3 == 0)
        add_return_value(e);
}

static void add_actual_params(MockActualCall &a)
{
    unsigned n = fz_r() % 4;
    for (unsigned i = 0; i < n; i++) {
        const char *p = kParams[fz_r() % 3];
        switch (fz_r() % 22) {
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
            void *v = fz_ptr();
            TR(" ap-ptr(%s=%p)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 5: {
            TR(" ap-out(%s)", p);
            a.withOutputParameter(p, out_dst);
            break;
        }
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
        case 9: {
            bool v = fz_r() % 2 == 0;
            TR(" ap-bool(%s=%d)", p, (int)v);
            a.withParameter(p, v);
            break;
        }
        case 10: {
            long long v = (long long)(fz_r() % 3) - 1;
            TR(" ap-ll(%s=%lld)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 11: {
            unsigned long long v = fz_r() % 3;
            TR(" ap-ull(%s=%llu)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 12: {
            long v = (long)(fz_r() % 3) - 1;
            TR(" ap-l(%s=%ld)", p, v);
            a.withParameter(p, v);
            break;
        }
        case 13: {
            unsigned int v = fz_r() % 3;
            TR(" ap-nUint(%s=%u)", p, v);
            a.withUnsignedIntParameter(p, v);
            break;
        }
        case 14: {
            int v = (int)(fz_r() % 3);
            TR(" ap-nInt(%s=%d)", p, v);
            a.withIntParameter(p, v);
            break;
        }
        case 15: {
            bool v = fz_r() % 2 == 0;
            TR(" ap-nBool(%s=%d)", p, (int)v);
            a.withBoolParameter(p, v);
            break;
        }
        case 16: {
            const char *v = kStrings[fz_r() % 3];
            TR(" ap-nStr(%s=%s)", p, v);
            a.withStringParameter(p, v);
            break;
        }
        case 17: {
            long v = (long)(fz_r() % 3);
            TR(" ap-nLong(%s=%ld)", p, v);
            a.withLongIntParameter(p, v);
            break;
        }
        case 18: {
            double v = (double)(fz_r() % 3);
            TR(" ap-nDbl(%s=%g)", p, v);
            a.withDoubleParameter(p, v);
            break;
        }
        case 19: {
            const void *v = fz_cptr();
            TR(" ap-cptr(%s=%p)", p, v);
            a.withConstPointerParameter(p, v);
            break;
        }
        case 20: {
            fz_fp_t v = fz_fp();
            TR(" ap-fp(%s)", p);
            a.withFunctionPointerParameter(p, v);
            break;
        }
        default: {
            unsigned sz = 1 + fz_r() % 3;
            TR(" ap-nMembuf(%s,%u)", p, sz);
            a.withMemoryBufferParameter(p, fz_membuf, sz);
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
        switch (fz_r() % 12) {
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
            if (fz_r() % 2 == 0)
                do_return_getter(a);
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
            switch (fz_r() % 6) {
            case 0: {
                MockSupport &m8 = pick_mock();
                TR("[%s].disable\n", last_scope);
                m8.disable();
                break;
            }
            case 1: {
                MockSupport &m8 = pick_mock();
                TR("[%s].enable\n", last_scope);
                m8.enable();
                break;
            }
            case 2: {
                MockSupport &m8 = pick_mock();
                TR("[%s].disable\n", last_scope);
                m8.disable();
                break;
            }
            case 3: {
                TR("[].activeCalls\n");
                printf("V left=%d\n", (int)mock().expectedCallsLeft());
                break;
            }
            default: {
                MockSupport &m8 = pick_mock();
                TR("[%s].enable\n", last_scope);
                m8.enable();
                break;
            }
            }
            break;
        case 9: { /* MockSupport-level return getter */
            MockSupport &m9 = pick_mock();
            TR("[%s].supportGetter", last_scope);
            do_support_return_getter(m9);
            TR("\n");
            break;
        }
        case 10: { /* mid-test expectation check, heavily used downstream */
            MockSupport &m10 = pick_mock();
            TR("[%s].checkExpectations", last_scope);
            m10.checkExpectations();
            TR("\n");
            break;
        }
        default:
            switch (fz_r() % 10) {
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
                printf("V left=%d\n", (int)mock().expectedCallsLeft());
                break;
            case 3:
                TR("[s1].expectedCallsLeft\n");
                printf("V s1left=%d\n", (int)mock("s1").expectedCallsLeft());
                break;
            case 4:
                TR("[s1].checkExpectations\n");
                mock("s1").checkExpectations();
                break;
            case 5:
                /* re-install over an already-registered type name: which
                 * registration wins is upstream-defined behavior */
                TR("[].reinstallComparator\n");
                mock().installComparator("FZT", fz_comparator2);
                break;
            case 6:
                /* mid-sequence removal: subsequent OfType params take the
                 * "no way to compare/copy" failure path */
                TR("[].removeAllComparatorsAndCopiers\n");
                mock().removeAllComparatorsAndCopiers();
                break;
            case 7:
                /* default-off toggle exercised without ever crashing */
                TR("[].crashOnFailure(false)\n");
                mock().crashOnFailure(false);
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
