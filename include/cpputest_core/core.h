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

/* Assertion support. Each cu_assert_* counts one check, then on mismatch
 * prints the failure (byte-identical to upstream TestFailure.cpp), bumps the
 * failure count and longjmps out of the current protected section. The
 * `text` parameter is the user message (NULL when none). */
void cu_count_check(void);
void cu_fail_with_message(const char *file, size_t line, const char *message);
void cu_exit_current_test(void); /* TEST_EXIT: leave test without failing */

void cu_assert_true(int condition, const char *check_string,
                    const char *condition_string, const char *text,
                    const char *file, size_t line);
void cu_fail(const char *text, const char *file, size_t line);
void cu_assert_cstr_equal(const char *expected, const char *actual,
                          const char *text, const char *file, size_t line);
void cu_assert_cstr_nequal(const char *expected, const char *actual,
                           size_t length, const char *text,
                           const char *file, size_t line);
void cu_assert_cstr_nocase_equal(const char *expected, const char *actual,
                                 const char *text, const char *file, size_t line);
void cu_assert_cstr_contains(const char *expected, const char *actual,
                             const char *text, const char *file, size_t line);
void cu_assert_cstr_nocase_contains(const char *expected, const char *actual,
                                    const char *text, const char *file, size_t line);
void cu_assert_longs_equal(long expected, long actual, const char *text,
                           const char *file, size_t line);
void cu_assert_unsigned_longs_equal(unsigned long expected, unsigned long actual,
                                    const char *text, const char *file, size_t line);
void cu_assert_longlongs_equal(long long expected, long long actual,
                               const char *text, const char *file, size_t line);
void cu_assert_unsigned_longlongs_equal(unsigned long long expected,
                                        unsigned long long actual,
                                        const char *text, const char *file, size_t line);
void cu_assert_signed_bytes_equal(signed char expected, signed char actual,
                                  const char *text, const char *file, size_t line);
void cu_assert_pointers_equal(const void *expected, const void *actual,
                              const char *text, const char *file, size_t line);
void cu_assert_functionpointers_equal(void (*expected)(void), void (*actual)(void),
                                      const char *text, const char *file, size_t line);
void cu_assert_doubles_equal(double expected, double actual, double threshold,
                             const char *text, const char *file, size_t line);
void cu_assert_binary_equal(const void *expected, const void *actual, size_t length,
                            const char *text, const char *file, size_t line);
void cu_assert_bits_equal(unsigned long expected, unsigned long actual,
                          unsigned long mask, size_t byte_count,
                          const char *text, const char *file, size_t line);
/* CHECK_EQUAL / ENUMS_EQUAL: strings already rendered by the caller */
void cu_assert_equals(int failed, const char *expected, const char *actual,
                      const char *text, const char *file, size_t line);
void cu_assert_compare(int comparison, const char *check_string,
                       const char *comparison_string, const char *text,
                       const char *file, size_t line);
/* UT_PRINT */
void cu_print(const char *text, const char *file, size_t line);

/* String-rendering helpers shared with the C++ SimpleString shim; all return
 * malloc'd strings the caller frees (cu_str_free). */
char *cu_str_printf(const char *format, ...);
char *cu_str_printable(const char *s); /* upstream SimpleString::printable() */
char *cu_str_from_double(double value, int precision);
char *cu_str_from_binary(const unsigned char *value, size_t size);
char *cu_str_from_masked_bits(unsigned long value, unsigned long mask,
                              size_t byte_count);
char *cu_str_hex_from_signed_char(signed char value);
void cu_str_free(char *s);

/* Full runner: parses args, runs registered tests, returns exit code. */
int cu_run_all(int argc, const char *const *argv);

const char *cpputest_core_version(void);

#ifdef __cplusplus
}
#endif

#endif
