#ifndef CPPUTEST_CORE_MOCK_H
#define CPPUTEST_CORE_MOCK_H

/* C core of CppUMock. The C++ MockSupport/MockExpectedCall/MockActualCall
 * classes in CppUTestExt/ are thin facades over these calls. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cum_scope cum_scope;
typedef struct cum_expectation cum_expectation;
typedef struct cum_actual cum_actual;

/* scope registry; "" is the global scope. Creates on first use. */
cum_scope *cum_scope_get(const char *name);

/* MockSupport surface (subset — grows with the shim) */
void cum_strict_order(cum_scope *s);
cum_expectation *cum_expect_n_calls(cum_scope *s, unsigned amount,
                                    const char *name); /* NULL when disabled */
cum_actual *cum_actual_call(cum_scope *s, const char *name); /* NULL: disabled/ignored */
void cum_check_expectations_all(void); /* recursive over every scope */
void cum_clear_all(void);
int cum_expected_calls_left_all(void);
void cum_ignore_other_calls(cum_scope *s);
void cum_enable(cum_scope *s, int enabled);
int cum_is_enabled(const cum_scope *s);
void cum_crash_on_failure(int crash);

/* expectation knobs */
void cum_expectation_with_call_order(cum_expectation *e, unsigned start, unsigned end);
void cum_expectation_ignore_other_parameters(cum_expectation *e);

/* facade bookkeeping: shim objects attached to records, freed via callback */
void cum_expectation_set_user(cum_expectation *e, void *user);
void *cum_expectation_user(cum_expectation *e);
void cum_set_facade_free(void (*fn)(void *));

#ifdef __cplusplus
}
#endif

#endif
