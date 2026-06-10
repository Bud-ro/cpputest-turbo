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

/* Plugin hook points. The C++ shim's TestRegistry installs trampolines here
 * that walk its TestPlugin chain. parse is consulted for unrecognized
 * "-pXXX" arguments (upstream forwards those to plugin->parseAllArguments;
 * the index is passed by value, so plugins cannot consume extra args). */
typedef void (*cu_plugin_action_fn)(cu_test *t);
typedef int (*cu_plugin_parse_fn)(int argc, const char *const *argv, int index);
void cu_set_plugin_hooks(cu_plugin_action_fn pre, cu_plugin_action_fn post,
                         cu_plugin_parse_fn parse);

/* Report a failure without aborting the test (upstream
 * TestResult::addFailure as used from plugin postTestAction). */
void cu_add_failure(const char *file, size_t line, const char *message);
size_t cu_failure_count(void);
/* TestResult::print — routed through the active output (JUnit captures) */
void cu_print_text(const char *text);

/* ------------------------ memory leak detection -------------------------
 * Port of upstream MemoryLeakDetector. Allocation types: */
#define CU_MEM_NEW 0
#define CU_MEM_NEW_ARRAY 1
#define CU_MEM_MALLOC 2

void *cu_mem_alloc_tracked(size_t size, const char *file, size_t line, int type);
void cu_mem_free_tracked(void *p, const char *file, size_t line, int type);
void *cu_mem_realloc_tracked(void *p, size_t size, const char *file, size_t line);
void cu_mem_tracking_set(int on);   /* turnOn/OffNewDeleteOverloads */
int cu_mem_tracking(void);          /* areNewDeleteOverloaded */
void cu_mem_save_and_disable_tracking(void); /* ref-counted */
void cu_mem_restore_tracking(void);
void cu_mem_start_checking(void);   /* per-test checking period */
void cu_mem_stop_checking(void);
void cu_mem_mark_checking_as_global(void);
size_t cu_mem_leak_count(int checking_only);
const char *cu_mem_leak_report(int checking_only); /* static buffer */
void cu_mem_destroy_all(void);

/* upstream public C allocation API (TestHarness_c.h surface) */
void *cpputest_malloc(size_t size);
void *cpputest_calloc(size_t count, size_t size);
void *cpputest_realloc(void *p, size_t size);
void cpputest_free(void *p);
char *cpputest_strdup(const char *s);
char *cpputest_strndup(const char *s, size_t n);
void *cpputest_malloc_location(size_t size, const char *file, size_t line);
void *cpputest_calloc_location(size_t count, size_t size, const char *file, size_t line);
void *cpputest_realloc_location(void *p, size_t size, const char *file, size_t line);
void cpputest_free_location(void *p, const char *file, size_t line);
char *cpputest_strdup_location(const char *s, const char *file, size_t line);
char *cpputest_strndup_location(const char *s, size_t n, const char *file, size_t line);
void cpputest_malloc_set_out_of_memory(void);
void cpputest_malloc_set_not_out_of_memory(void);
void cpputest_malloc_set_out_of_memory_countdown(int countdown);
void cpputest_malloc_count_reset(void);
int cpputest_malloc_get_count(void);

/* Full runner: parses args, runs registered tests, returns exit code. */
int cu_run_all(int argc, const char *const *argv);

const char *cpputest_core_version(void);

#ifdef __cplusplus
}
#endif

#endif
