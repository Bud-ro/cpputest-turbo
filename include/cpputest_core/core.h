#ifndef CPPUTEST_CORE_CORE_H
#define CPPUTEST_CORE_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cu_test cu_test;

/* Per-test callbacks. The fixture pointer is opaque to the core (the C++ shim
 * passes the Utest object; pure-C suites pass whatever they like). */
typedef struct cu_test_ops {
    void *(*create)(cu_test *t);
    void (*destroy)(cu_test *t, void *fixture);
    void (*setup)(cu_test *t, void *fixture);
    void (*body)(cu_test *t, void *fixture);
    void (*teardown)(cu_test *t, void *fixture);
} cu_test_ops;

struct cu_test {
    const char *group;
    const char *name;
    const char *file;
    size_t line;
    int is_ignored;   /* registered via IGNORE_TEST */
    int run_ignored;  /* -ri: run ignored tests anyway */
    int has_failed;   /* valid during/after this test's run */
    void *user;       /* owning shim object (e.g. C++ UtestShell) */
    const cu_test_ops *ops;
    cu_test *next;
};

/* Registration. Prepends, so tests run in reverse registration order —
 * upstream TestRegistry::addTest does tests_ = test->addTest(tests_). */
void cu_registry_add(cu_test *t);
void cu_registry_undo_last_add(void);
cu_test *cu_registry_tests(void);
void cu_registry_set_tests(cu_test *head); /* test-support: swap list */

/* Current test (NULL outside a run). */
cu_test *cu_current_test(void);

/* Assertion support. cu_fail_* print the failure, bump the failure count and
 * longjmp out of the current protected section (never return). */
void cu_count_check(void);
void cu_fail_with_message(const char *file, size_t line, const char *message);
void cu_fail_check(const char *file, size_t line, const char *check_string,
                   const char *condition_string, const char *text);
void cu_fail_text(const char *file, size_t line, const char *text);
void cu_exit_current_test(void); /* TEST_EXIT: leave test without failing */

/* Full runner: parses args, runs registered tests, returns exit code. */
int cu_run_all(int argc, const char *const *argv);

const char *cpputest_core_version(void);

#ifdef __cplusplus
}
#endif

#endif
