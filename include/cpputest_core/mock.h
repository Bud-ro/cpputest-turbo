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
    CUM_T_MEMBUFFER,
    CUM_T_OBJECT,
    CUM_T_CONST_OBJECT
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
        struct {
            const char *type_name;
            const void *ptr;
        } obj;
    } v;
} cum_value;

/* "int", "const char*", ... or the custom object type name */
const char *cum_value_type_name(const cum_value *v);

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

/* custom-type comparators/copiers (MockNamedValueComparator/Copier). The
 * C++ shim registers adapters; ctx is the C++ object. to_string returns a
 * malloc'd string (freed with cu_str_free). */
typedef int (*cum_comparator_equal_fn)(void *ctx, const void *o1, const void *o2);
typedef char *(*cum_comparator_string_fn)(void *ctx, const void *o);
typedef void (*cum_copier_fn)(void *ctx, void *dst, const void *src);
void cum_install_comparator(const char *type_name, void *ctx,
                            cum_comparator_equal_fn equal,
                            cum_comparator_string_fn to_string);
void cum_install_copier(const char *type_name, void *ctx, cum_copier_fn copy);
void cum_remove_all_comparators_and_copiers(void);
int cum_has_comparator(const char *type_name);
int cum_has_copier(const char *type_name);
/* MockNoWayToCompare/CopyCustomTypeFailure (fail + longjmp in a test) */
void cum_fail_no_way_to_compare(const char *type_name);
void cum_fail_no_way_to_copy(const char *type_name);

/* OfType parameters use cum_value with CUM_T_OBJECT/CUM_T_CONST_OBJECT;
 * expectation-side type names are copied. Output OfType: */
void cum_expectation_with_output_parameter_of_type(cum_expectation *e,
                                                   const char *type_name,
                                                   const char *name,
                                                   const void *value);
void cum_actual_with_output_parameter_of_type(cum_actual *a,
                                              const char *type_name,
                                              const char *name, void *dst);

/* output parameters: the expected side supplies the bytes (or "unmodified");
 * the actual side registers destinations, filled when the call matches */
void cum_expectation_with_output_parameter(cum_expectation *e, const char *name,
                                           const void *value, size_t size);
void cum_expectation_with_unmodified_output_parameter(cum_expectation *e,
                                                      const char *name);
void cum_actual_with_output_parameter(cum_actual *a, const char *name, void *dst);

/* return values: set on the expectation, read from the actual call (reading
 * finalizes the call, like upstream's checkExpectations-in-returnValue) */
void cum_expectation_and_return(cum_expectation *e, cum_value value);
int cum_actual_return_value(cum_actual *a, cum_value *out); /* 1 if present */
int cum_scope_return_value(cum_scope *s, cum_value *out);   /* last actual call */

/* per-scope data store (MockSupport::setData/getData/hasData). Object type
 * names are copied; everything else stores raw pointers like upstream. */
void cum_scope_set_data(cum_scope *s, const char *name, cum_value value);
int cum_scope_get_data(cum_scope *s, const char *name, cum_value *out);
int cum_scope_has_data(cum_scope *s, const char *name);

/* facade bookkeeping: shim objects attached to records, freed via callback */
void cum_expectation_set_user(cum_expectation *e, void *user);
void *cum_expectation_user(cum_expectation *e);
void cum_set_facade_free(void (*fn)(void *));

#ifdef __cplusplus
}
#endif

#endif
