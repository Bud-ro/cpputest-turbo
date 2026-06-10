/* C-side driver for the differential mock_c fuzzer (fuzz_mock_c_diff.cpp).
 *
 * This is a real C11 translation unit using ONLY CppUTestExt/MockSupport_c.h
 * — the embedded-style split the harness exists for: production/test-double
 * code written in C calling mock_c(), while the tests binding into CppUTest
 * are C++. Compiles unchanged against upstream CppUTest and cpputest-turbo;
 * outputs are diffed.
 *
 * Coverage: the entire MockSupport_c surface — every with*Parameters
 * variant (expected + actual), every andReturn*Value setter, every
 * *ReturnValue / return*ValueOrDefault getter (printed, so silent value
 * divergence diffs), returnValue/getData MockValue_c mapping, the full
 * set*Data family, comparators/copiers via C function pointers, output
 * parameters (plain, of-type, unmodified), scopes via mock_scope_c,
 * strictOrder/enable/disable/ignoreOtherCalls, expectNoCall/expectNCalls,
 * checkExpectations/expectedCallsLeft/clear.
 *
 * Getter calls are gated on a freshness flag: upstream's static actualCall
 * pointer dangles after clear (a real upstream hazard), so reading it then
 * would be UB in the oracle, not a comparison. checkExpectations keeps the
 * call alive on both sides and is NOT gated. */

#include "CppUTestExt/MockSupport_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fz_trace;
#define TR(...)                                                                \
    do {                                                                       \
        if (fz_trace)                                                          \
            fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

void fz_mock_c_set_trace(int on);
void fz_mock_c_sequence(unsigned long long seed);

void fz_mock_c_set_trace(int on)
{
    fz_trace = on;
}

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
static const char *const kDataNames[] = {"d1", "d2"};

static unsigned char out_src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
static unsigned char out_dst[8];
static int fz_objs[3] = {10, 20, 30};
static int fz_copy_dst;
static const unsigned char fz_membuf[3] = {0xAA, 0xBB, 0xCC};

/* fake pointers: identical literals in both binaries (never dereferenced) */
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

/* C comparator/copier for type "FZT": deterministic, value-derived text */
static int fzt_equal(const void *o1, const void *o2)
{
    return *(const int *)o1 == *(const int *)o2;
}
static const char *fzt_to_string(const void *o)
{
    static char buf[32];
    snprintf(buf, sizeof buf, "%d", *(const int *)o);
    return buf;
}
static void fzt_copy(void *dst, const void *src)
{
    *(int *)dst = *(const int *)src;
}

/* upstream's static actualCall pointer dangles after clear/checkExpectations;
 * typed getters may only run while fresh */
static int act_fresh;

static MockSupport_c *pick_mock(void)
{
    if (fz_r() % 2) {
        TR("[s1]");
        return mock_scope_c("s1");
    }
    TR("[]");
    return mock_c();
}

static void add_expect_params(MockExpectedCall_c *e)
{
    unsigned n = fz_r() % 4;
    for (unsigned i = 0; i < n; i++) {
        const char *p = kParams[fz_r() % 3];
        switch (fz_r() % 18) {
        case 0:
            TR(" ep-bool(%s)", p);
            e = e->withBoolParameters(p, (int)(fz_r() % 2));
            break;
        case 1:
            TR(" ep-int(%s)", p);
            e = e->withIntParameters(p, (int)(fz_r() % 3));
            break;
        case 2:
            TR(" ep-uint(%s)", p);
            e = e->withUnsignedIntParameters(p, fz_r() % 3);
            break;
        case 3:
            TR(" ep-long(%s)", p);
            e = e->withLongIntParameters(p, (long)(fz_r() % 3) - 1);
            break;
        case 4:
            TR(" ep-ulong(%s)", p);
            e = e->withUnsignedLongIntParameters(p, fz_r() % 3);
            break;
        case 5:
            TR(" ep-ll(%s)", p);
            e = e->withLongLongIntParameters(p, (long long)(fz_r() % 3) - 1);
            break;
        case 6:
            TR(" ep-ull(%s)", p);
            e = e->withUnsignedLongLongIntParameters(p, fz_r() % 3);
            break;
        case 7:
            TR(" ep-dbl(%s)", p);
            e = e->withDoubleParameters(p, (double)(fz_r() % 3));
            break;
        case 8:
            TR(" ep-dblTol(%s)", p);
            e = e->withDoubleParametersAndTolerance(p, (double)(fz_r() % 3),
                                                    0.25);
            break;
        case 9:
            TR(" ep-str(%s)", p);
            e = e->withStringParameters(p, kStrings[fz_r() % 3]);
            break;
        case 10:
            TR(" ep-ptr(%s)", p);
            e = e->withPointerParameters(p, fz_ptr());
            break;
        case 11:
            TR(" ep-cptr(%s)", p);
            e = e->withConstPointerParameters(p, fz_cptr());
            break;
        case 12:
            TR(" ep-fp(%s)", p);
            e = e->withFunctionPointerParameters(p, fz_fp());
            break;
        case 13:
            TR(" ep-membuf(%s)", p);
            e = e->withMemoryBufferParameter(p, fz_membuf, 1 + fz_r() % 3);
            break;
        case 14:
            TR(" ep-oftype(%s)", p);
            e = e->withParameterOfType("FZT", p, &fz_objs[fz_r() % 3]);
            break;
        case 15:
            TR(" ep-out(%s)", p);
            e = e->withOutputParameterReturning(p, out_src,
                                                fz_r() % 2 ? 4u : 8u);
            break;
        case 16:
            TR(" ep-outoftype(%s)", p);
            e = e->withOutputParameterOfTypeReturning("FZT", p,
                                                      &fz_objs[fz_r() % 3]);
            break;
        default:
            TR(" ep-unmod(%s)", p);
            e = e->withUnmodifiedOutputParameter(p);
            break;
        }
    }
    if (fz_r() % 4 == 0) {
        TR(" ep-ignoreOthers");
        e = e->ignoreOtherParameters();
    }
    if (fz_r() % 3 == 0) {
        switch (fz_r() % 12) {
        case 0:
            TR(" ret-bool");
            e = e->andReturnBoolValue((int)(fz_r() % 2));
            break;
        case 1:
            TR(" ret-int");
            e = e->andReturnIntValue((int)(fz_r() % 100) - 50);
            break;
        case 2:
            TR(" ret-uint");
            e = e->andReturnUnsignedIntValue(fz_r() % 100);
            break;
        case 3:
            TR(" ret-long");
            e = e->andReturnLongIntValue((long)(fz_r() % 100) - 50);
            break;
        case 4:
            TR(" ret-ulong");
            e = e->andReturnUnsignedLongIntValue(fz_r() % 100);
            break;
        case 5:
            TR(" ret-ll");
            e = e->andReturnLongLongIntValue((long long)(fz_r() % 100) - 50);
            break;
        case 6:
            TR(" ret-ull");
            e = e->andReturnUnsignedLongLongIntValue(fz_r() % 100);
            break;
        case 7:
            TR(" ret-dbl");
            e = e->andReturnDoubleValue((double)(fz_r() % 4) * 0.5);
            break;
        case 8:
            TR(" ret-str");
            e = e->andReturnStringValue(kStrings[fz_r() % 3]);
            break;
        case 9:
            TR(" ret-ptr");
            e = e->andReturnPointerValue(fz_ptr());
            break;
        case 10:
            TR(" ret-cptr");
            e = e->andReturnConstPointerValue(fz_cptr());
            break;
        default:
            TR(" ret-fp");
            e = e->andReturnFunctionPointerValue(fz_fp());
            break;
        }
    }
    (void)e;
}

static MockActualCall_c *add_actual_params(MockActualCall_c *a)
{
    unsigned n = fz_r() % 4;
    for (unsigned i = 0; i < n; i++) {
        const char *p = kParams[fz_r() % 3];
        switch (fz_r() % 16) {
        case 0:
            TR(" ap-bool(%s)", p);
            a = a->withBoolParameters(p, (int)(fz_r() % 2));
            break;
        case 1:
            TR(" ap-int(%s)", p);
            a = a->withIntParameters(p, (int)(fz_r() % 3));
            break;
        case 2:
            TR(" ap-uint(%s)", p);
            a = a->withUnsignedIntParameters(p, fz_r() % 3);
            break;
        case 3:
            TR(" ap-long(%s)", p);
            a = a->withLongIntParameters(p, (long)(fz_r() % 3) - 1);
            break;
        case 4:
            TR(" ap-ulong(%s)", p);
            a = a->withUnsignedLongIntParameters(p, fz_r() % 3);
            break;
        case 5:
            TR(" ap-ll(%s)", p);
            a = a->withLongLongIntParameters(p, (long long)(fz_r() % 3) - 1);
            break;
        case 6:
            TR(" ap-ull(%s)", p);
            a = a->withUnsignedLongLongIntParameters(p, fz_r() % 3);
            break;
        case 7:
            TR(" ap-dbl(%s)", p);
            a = a->withDoubleParameters(p, (double)(fz_r() % 3));
            break;
        case 8:
            TR(" ap-str(%s)", p);
            a = a->withStringParameters(p, kStrings[fz_r() % 3]);
            break;
        case 9:
            TR(" ap-ptr(%s)", p);
            a = a->withPointerParameters(p, fz_ptr());
            break;
        case 10:
            TR(" ap-cptr(%s)", p);
            a = a->withConstPointerParameters(p, fz_cptr());
            break;
        case 11:
            TR(" ap-fp(%s)", p);
            a = a->withFunctionPointerParameters(p, fz_fp());
            break;
        case 12:
            TR(" ap-membuf(%s)", p);
            a = a->withMemoryBufferParameter(p, fz_membuf, 1 + fz_r() % 3);
            break;
        case 13:
            TR(" ap-oftype(%s)", p);
            a = a->withParameterOfType("FZT", p, &fz_objs[fz_r() % 3]);
            break;
        case 14:
            TR(" ap-out(%s)", p);
            a = a->withOutputParameter(p, out_dst);
            break;
        default:
            TR(" ap-outoftype(%s)", p);
            a = a->withOutputParameterOfType("FZT", p, &fz_copy_dst);
            break;
        }
    }
    return a;
}

/* every typed getter, printed; both with and without a queued value */
static void do_actual_getter(MockActualCall_c *a)
{
    switch (fz_r() % 27) {
    case 0:
        TR(" g-has");
        printf("C ahas=%d\n", a->hasReturnValue());
        break;
    case 1:
        TR(" g-bool");
        printf("C abool=%d\n", a->boolReturnValue());
        break;
    case 2:
        TR(" g-boolD");
        printf("C aboolD=%d\n", a->returnBoolValueOrDefault(1));
        break;
    case 3:
        TR(" g-int");
        printf("C aint=%d\n", a->intReturnValue());
        break;
    case 4:
        TR(" g-intD");
        printf("C aintD=%d\n", a->returnIntValueOrDefault(-1));
        break;
    case 5:
        TR(" g-uint");
        printf("C auint=%u\n", a->unsignedIntReturnValue());
        break;
    case 6:
        TR(" g-uintD");
        printf("C auintD=%u\n", a->returnUnsignedIntValueOrDefault(7u));
        break;
    case 7:
        TR(" g-long");
        printf("C along=%ld\n", a->longIntReturnValue());
        break;
    case 8:
        TR(" g-longD");
        printf("C alongD=%ld\n", a->returnLongIntValueOrDefault(-7L));
        break;
    case 9:
        TR(" g-ulong");
        printf("C aulong=%lu\n", a->unsignedLongIntReturnValue());
        break;
    case 10:
        TR(" g-ulongD");
        printf("C aulongD=%lu\n", a->returnUnsignedLongIntValueOrDefault(9UL));
        break;
    case 11:
        TR(" g-ll");
        printf("C all=%lld\n", a->longLongIntReturnValue());
        break;
    case 12:
        TR(" g-llD");
        printf("C allD=%lld\n", a->returnLongLongIntValueOrDefault(-11LL));
        break;
    case 13:
        TR(" g-ull");
        printf("C aull=%llu\n", a->unsignedLongLongIntReturnValue());
        break;
    case 14:
        TR(" g-ullD");
        printf("C aullD=%llu\n",
               a->returnUnsignedLongLongIntValueOrDefault(13ULL));
        break;
    case 15:
        TR(" g-dbl");
        printf("C adbl=%g\n", a->doubleReturnValue());
        break;
    case 16:
        TR(" g-dblD");
        printf("C adblD=%g\n", a->returnDoubleValueOrDefault(0.5));
        break;
    case 17:
        TR(" g-str");
        printf("C astr=%s\n", a->stringReturnValue());
        break;
    case 18:
        TR(" g-strD");
        printf("C astrD=%s\n", a->returnStringValueOrDefault("dflt"));
        break;
    case 19:
        TR(" g-ptr");
        printf("C aptr=%p\n", a->pointerReturnValue());
        break;
    case 20:
        TR(" g-ptrD");
        printf("C aptrD=%p\n",
               a->returnPointerValueOrDefault((void *)(size_t)0x99));
        break;
    case 21:
        TR(" g-cptr");
        printf("C acptr=%p\n", a->constPointerReturnValue());
        break;
    case 22:
        TR(" g-cptrD");
        printf("C acptrD=%p\n",
               a->returnConstPointerValueOrDefault((const void *)(size_t)0x88));
        break;
    case 23:
        TR(" g-fp");
        printf("C afp=%p\n", (void *)(size_t)a->functionPointerReturnValue());
        break;
    case 24:
        TR(" g-fpD");
        printf("C afpD=%p\n",
               (void *)(size_t)a->returnFunctionPointerValueOrDefault(
                   (fz_fp_t)(size_t)0x77));
        break;
    case 25: {
        TR(" g-raw");
        MockValue_c v = a->returnValue();
        printf("C araw type=%d\n", (int)v.type);
        break;
    }
    default:
        TR(" g-none");
        break;
    }
}

/* support-level getters (the shared-table path) */
static void do_support_getter(MockSupport_c *m)
{
    switch (fz_r() % 6) {
    case 0:
        TR(" sg-has");
        printf("C shas=%d\n", m->hasReturnValue());
        break;
    case 1:
        TR(" sg-int");
        printf("C sint=%d\n", m->intReturnValue());
        break;
    case 2:
        TR(" sg-str");
        printf("C sstr=%s\n", m->stringReturnValue());
        break;
    case 3:
        TR(" sg-dbl");
        printf("C sdbl=%g\n", m->doubleReturnValue());
        break;
    case 4: {
        TR(" sg-raw");
        MockValue_c v = m->returnValue();
        printf("C sraw type=%d\n", (int)v.type);
        break;
    }
    default:
        TR(" sg-intD");
        printf("C sintD=%d\n", m->returnIntValueOrDefault(-2));
        break;
    }
}

static void do_data_op(MockSupport_c *m)
{
    const char *dn = kDataNames[fz_r() % 2];
    switch (fz_r() % 14) {
    case 0:
        TR(" d-bool(%s)", dn);
        m->setBoolData(dn, (int)(fz_r() % 2));
        break;
    case 1:
        TR(" d-int(%s)", dn);
        m->setIntData(dn, (int)(fz_r() % 50) - 25);
        break;
    case 2:
        TR(" d-uint(%s)", dn);
        m->setUnsignedIntData(dn, fz_r() % 50);
        break;
    case 3:
        TR(" d-long(%s)", dn);
        m->setLongIntData(dn, (long)(fz_r() % 50) - 25);
        break;
    case 4:
        TR(" d-ulong(%s)", dn);
        m->setUnsignedLongIntData(dn, fz_r() % 50);
        break;
    case 5:
        TR(" d-str(%s)", dn);
        m->setStringData(dn, kStrings[fz_r() % 3]);
        break;
    case 6:
        TR(" d-dbl(%s)", dn);
        m->setDoubleData(dn, (double)(fz_r() % 4) * 0.25);
        break;
    case 7:
        TR(" d-ptr(%s)", dn);
        m->setPointerData(dn, fz_ptr());
        break;
    case 8:
        TR(" d-cptr(%s)", dn);
        m->setConstPointerData(dn, fz_cptr());
        break;
    case 9:
        TR(" d-fp(%s)", dn);
        m->setFunctionPointerData(dn, fz_fp());
        break;
    case 10:
        TR(" d-obj(%s)", dn);
        m->setDataObject(dn, "FZT", &fz_objs[fz_r() % 3]);
        break;
    case 11:
        TR(" d-cobj(%s)", dn);
        m->setDataConstObject(dn, "FZT", &fz_objs[fz_r() % 3]);
        break;
    default: {
        TR(" d-get(%s)", dn);
        MockValue_c v = m->getData(dn);
        switch (v.type) {
        case MOCKVALUETYPE_BOOL:
            printf("C dbool=%d\n", v.value.boolValue);
            break;
        case MOCKVALUETYPE_INTEGER:
            printf("C dint=%d\n", v.value.intValue);
            break;
        case MOCKVALUETYPE_UNSIGNED_INTEGER:
            printf("C duint=%u\n", v.value.unsignedIntValue);
            break;
        case MOCKVALUETYPE_LONG_INTEGER:
            printf("C dlong=%ld\n", v.value.longIntValue);
            break;
        case MOCKVALUETYPE_UNSIGNED_LONG_INTEGER:
            printf("C dulong=%lu\n", v.value.unsignedLongIntValue);
            break;
        case MOCKVALUETYPE_DOUBLE:
            printf("C ddbl=%g\n", v.value.doubleValue);
            break;
        case MOCKVALUETYPE_STRING:
            printf("C dstr=%s\n", v.value.stringValue);
            break;
        case MOCKVALUETYPE_POINTER:
            printf("C dptr=%p\n", v.value.pointerValue);
            break;
        case MOCKVALUETYPE_CONST_POINTER:
            printf("C dcptr=%p\n", v.value.constPointerValue);
            break;
        case MOCKVALUETYPE_FUNCTIONPOINTER:
            printf("C dfp=%p\n",
                   (void *)(size_t)v.value.functionPointerValue);
            break;
        default:
            printf("C dother type=%d\n", (int)v.type);
            break;
        }
        break;
    }
    }
}

void fz_mock_c_sequence(unsigned long long seed)
{
    fz_rng = seed * 2654435761ull + 12345;
    unsigned ops = 6 + fz_r() % 18;
    TR("--- C seq seed %llu (%u ops)\n", seed, ops);

    mock_c()->installComparator("FZT", fzt_equal, fzt_to_string);
    mock_c()->installCopier("FZT", fzt_copy);
    act_fresh = 0;

    for (unsigned i = 0; i < ops; i++) {
        switch (fz_r() % 12) {
        case 0:
        case 1:
        case 2: { /* expectation */
            MockSupport_c *m = pick_mock();
            const char *name = kNames[fz_r() % 3];
            unsigned kind = fz_r() % 4;
            if (kind == 2) {
                TR(".expectNoCall(%s)\n", name);
                m->expectNoCall(name);
            } else if (kind == 1) {
                unsigned n = 1 + fz_r() % 3;
                TR(".expectNCalls(%u,%s)", n, name);
                add_expect_params(m->expectNCalls(n, name));
                TR("\n");
            } else {
                TR(".expectOneCall(%s)", name);
                add_expect_params(m->expectOneCall(name));
                TR("\n");
            }
            break;
        }
        case 3:
        case 4:
        case 5:
        case 6: { /* actual call */
            MockSupport_c *m = pick_mock();
            const char *an = kNames[fz_r() % 3];
            TR(".actualCall(%s)", an);
            MockActualCall_c *a = add_actual_params(m->actualCall(an));
            act_fresh = 1;
            if (fz_r() % 2 == 0)
                do_actual_getter(a);
            TR("\n");
            break;
        }
        case 7: { /* support-level getter — needs a live last call */
            if (act_fresh) {
                MockSupport_c *m = pick_mock();
                TR(".supportGetter");
                do_support_getter(m);
                TR("\n");
            }
            break;
        }
        case 8: {
            MockSupport_c *m = pick_mock();
            if (fz_r() % 2) {
                TR(".strictOrder\n");
                m->strictOrder();
            } else {
                TR(".ignoreOtherCalls\n");
                m->ignoreOtherCalls();
            }
            break;
        }
        case 9: {
            MockSupport_c *m = pick_mock();
            if (fz_r() % 3 == 0) {
                TR(".disable\n");
                m->disable();
            } else {
                TR(".enable\n");
                m->enable();
            }
            break;
        }
        case 10: {
            MockSupport_c *m = pick_mock();
            TR(".data");
            do_data_op(m);
            TR("\n");
            break;
        }
        default:
            switch (fz_r() % 4) {
            case 0:
                TR(".clear\n");
                pick_mock()->clear();
                act_fresh = 0;
                break;
            case 1:
                TR(".expectedCallsLeft\n");
                printf("C left=%d\n", mock_c()->expectedCallsLeft());
                break;
            default:
                /* checkExpectations does NOT invalidate the last actual
                 * call on either side (only clear does), so getters stay
                 * comparable through it */
                TR(".checkExpectations\n");
                mock_c()->checkExpectations();
                break;
            }
            break;
        }
    }
    mock_c()->checkExpectations();
    act_fresh = 0;
}
