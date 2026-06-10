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

/* tagged value (MockNamedValue). Strings and memory buffers are NOT copied
 * (upstream stores the pointers; lifetime is the caller's problem). */
typedef enum {
    CUM_T_BOOL,
    CUM_T_INT,
    CUM_T_UINT,
    CUM_T_LONG,
    CUM_T_ULONG,
    CUM_T_LONGLONG,
    CUM_T_ULONGLONG,
    CUM_T_DOUBLE,
    CUM_T_STRING,
    CUM_T_POINTER,
    CUM_T_CONST_POINTER,
    CUM_T_FUNCTIONPOINTER,
    CUM_T_MEMBUFFER
} cum_type;

typedef struct cum_value {
    cum_type type;
    union {
        int b;
        int i;
        unsigned int ui;
        long l;
        unsigned long ul;
        long long ll;
        unsigned long long ull;
        struct {
            double value;
            double tolerance;
        } dbl;
        const char *str;
        void *ptr;
        const void *cptr;
        void (*fptr)(void);
        struct {
            const unsigned char *buf;
            size_t size;
        } mem;
    } v;
} cum_value;

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
void cum_expectation_with_parameter(cum_expectation *e, const char *name,
                                    cum_value value);

/* actual-call input parameter (checkInputParameter flow; may fail+longjmp) */
void cum_actual_with_parameter(cum_actual *a, const char *name, cum_value value);

/* facade bookkeeping: shim objects attached to records, freed via callback */
void cum_expectation_set_user(cum_expectation *e, void *user);
void *cum_expectation_user(cum_expectation *e);
void cum_set_facade_free(void (*fn)(void *));

#ifdef __cplusplus
}
#endif

#endif
