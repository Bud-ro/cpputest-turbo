/* Seeded fuzzer for the failure-message formatters: random byte strings
 * (control chars, high bytes, embedded escapes, long runs) pushed through
 * the assertion failure builders inside a real test context, so the
 * printable-escaping and difference-window arithmetic get exercised end to
 * end. Run under ASan/UBSan. Reproduce with FUZZ_SEED=<n>. */

#include <cpputest_core/core.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long rng;

static unsigned r(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (unsigned)(rng >> 32);
}

static char buf_e[600];
static char buf_a[600];

static void random_string(char *dst, size_t cap)
{
    size_t len = r() % (cap - 1);
    for (size_t i = 0; i < len; i++) {
        unsigned c = r() % 256;
        if (c == 0)
            c = 1; /* NUL terminates anyway; keep lengths interesting */
        dst[i] = (char)c;
    }
    dst[len] = '\0';
}

static void drop_sink(const char *text, void *arg)
{
    (void)text;
    (void)arg;
}

static void *t_create(cu_test *t) { (void)t; return NULL; }
static void t_destroy(cu_test *t, void *f) { (void)t; (void)f; }
static void t_setup(cu_test *t, void *f) { (void)t; (void)f; }
static void t_teardown(cu_test *t, void *f) { (void)t; (void)f; }

static void t_body(cu_test *t, void *f)
{
    (void)t;
    (void)f;
    random_string(buf_e, sizeof buf_e);
    random_string(buf_a, sizeof buf_a);

    switch (r() % 6) {
    case 0:
        cu_assert_cstr_equal(buf_e, buf_a, r() % 2 ? buf_e : NULL,
                             __FILE__, __LINE__);
        break;
    case 1:
        cu_assert_cstr_nocase_equal(buf_e, buf_a, NULL, __FILE__, __LINE__);
        break;
    case 2:
        cu_assert_cstr_contains(buf_e, buf_a, NULL, __FILE__, __LINE__);
        break;
    case 3:
        cu_assert_equals(0 != strcmp(buf_e, buf_a), buf_e, buf_a, NULL,
                         __FILE__, __LINE__);
        break;
    case 4:
        cu_assert_binary_equal(buf_e, buf_a,
                               (r() % 2 ? strlen(buf_e) : strlen(buf_a)),
                               NULL, __FILE__, __LINE__);
        break;
    default:
        cu_assert_cstr_nequal(buf_e, buf_a, r() % 600, NULL,
                              __FILE__, __LINE__);
        break;
    }
}

static const cu_test_ops ops = { t_create, t_destroy, t_setup, t_body, t_teardown };
static cu_test the_test;

int main(void)
{
    const char *seed_env = getenv("FUZZ_SEED");
    const char *iter_env = getenv("FUZZ_ITERS");
    unsigned long long base_seed = seed_env ? strtoull(seed_env, NULL, 10) : 1;
    unsigned long iters = iter_env ? strtoul(iter_env, NULL, 10) : 50000;

    the_test.group = "Fuzz";
    the_test.name = "strings";
    the_test.file = __FILE__;
    the_test.line = 1;
    the_test.ops = &ops;
    cu_registry_add(&the_test);
    cu_set_output_sink(drop_sink, NULL);

    for (unsigned long it = 0; it < iters; it++) {
        rng = base_seed + it * 2654435761ull + 1;
        cu_run_stats stats;
        cu_run_registered_tests(&stats, 0);
    }
    cu_set_output_sink(NULL, NULL);
    printf("fuzz_strings: %lu iterations clean\n", iters);
    return 0;
}
