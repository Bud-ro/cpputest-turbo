/* Pure-C consumer test: registers a test directly with the C core and runs
 * it — no C++ anywhere. Linked against the CPPUTEST_C_ONLY=1 library with
 * plain gcc. */

#include <cpputest_core/core.h>

#include <stdio.h>

static int body_ran;
static int setup_ran;
static int teardown_ran;

static void *test_create(cu_test *t)
{
    (void)t;
    return NULL;
}

static void test_destroy(cu_test *t, void *fixture)
{
    (void)t;
    (void)fixture;
}

static void test_setup(cu_test *t, void *fixture)
{
    (void)t;
    (void)fixture;
    setup_ran++;
}

static void test_body(cu_test *t, void *fixture)
{
    (void)t;
    (void)fixture;
    body_ran++;
    cu_assert_longs_equal(1, 1, NULL, __FILE__, __LINE__);
    cu_assert_cstr_equal("abc", "abc", NULL, __FILE__, __LINE__);
}

static void test_teardown(cu_test *t, void *fixture)
{
    (void)t;
    (void)fixture;
    teardown_ran++;
}

static const cu_test_ops ops = {
    test_create, test_destroy, test_setup, test_body, test_teardown
};

static cu_test the_test;

int main(int argc, const char *const *argv)
{
    the_test.group = "PureC";
    the_test.name = "coreRegistration";
    the_test.file = __FILE__;
    the_test.line = __LINE__;
    the_test.ops = &ops;
    cu_registry_add(&the_test);

    int rc = cu_run_all(argc, argv);
    if (rc != 0) {
        fprintf(stderr, "expected green run, got %d\n", rc);
        return 1;
    }
    if (body_ran != 1 || setup_ran != 1 || teardown_ran != 1) {
        fprintf(stderr, "lifecycle mismatch: setup=%d body=%d teardown=%d\n",
                setup_ran, body_ran, teardown_ran);
        return 2;
    }
    return 0;
}
