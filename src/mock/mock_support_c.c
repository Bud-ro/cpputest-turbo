#define _POSIX_C_SOURCE 200809L

#include "CppUTestExt/MockSupport_c.h"
#include <cpputest_core/mock.h>
#include <cpputest_core/core.h>

#include <stdlib.h>
#include <string.h>

/* Pure-C implementation of MockSupport_c over the mock core. Like upstream,
 * the function-pointer tables operate on the scope selected by the latest
 * mock_c()/mock_scope_c() call and the latest expectation/actual call. */

typedef void (*cum_fp)(void);

static cum_scope *cur_scope;
static cum_expectation *cur_expectation;
static cum_actual *cur_actual;

static cum_scope *scope(void)
{
    if (!cur_scope)
        cur_scope = cum_scope_get("");
    return cur_scope;
}

/* ------------------------- value conversion ----------------------------- */

static MockValue_c to_mock_value(const cum_value *v, int has)
{
    MockValue_c m;
    memset(&m, 0, sizeof m);
    if (!has) {
        m.type = MOCKVALUETYPE_INTEGER;
        return m;
    }
    switch (v->type) {
    case CUM_T_BOOL:
        m.type = MOCKVALUETYPE_BOOL;
        m.value.boolValue = v->v.b;
        break;
    case CUM_T_INT:
        m.type = MOCKVALUETYPE_INTEGER;
        m.value.intValue = v->v.i;
        break;
    case CUM_T_UINT:
        m.type = MOCKVALUETYPE_UNSIGNED_INTEGER;
        m.value.unsignedIntValue = v->v.ui;
        break;
    case CUM_T_LONG:
        m.type = MOCKVALUETYPE_LONG_INTEGER;
        m.value.longIntValue = v->v.l;
        break;
    case CUM_T_ULONG:
        m.type = MOCKVALUETYPE_UNSIGNED_LONG_INTEGER;
        m.value.unsignedLongIntValue = v->v.ul;
        break;
    case CUM_T_LONGLONG:
        m.type = MOCKVALUETYPE_LONG_LONG_INTEGER;
        m.value.longLongIntValue = v->v.ll;
        break;
    case CUM_T_ULONGLONG:
        m.type = MOCKVALUETYPE_UNSIGNED_LONG_LONG_INTEGER;
        m.value.unsignedLongLongIntValue = v->v.ull;
        break;
    case CUM_T_DOUBLE:
        m.type = MOCKVALUETYPE_DOUBLE;
        m.value.doubleValue = v->v.dbl.value;
        break;
    case CUM_T_STRING:
        m.type = MOCKVALUETYPE_STRING;
        m.value.stringValue = v->v.str;
        break;
    case CUM_T_POINTER:
        m.type = MOCKVALUETYPE_POINTER;
        m.value.pointerValue = v->v.ptr;
        break;
    case CUM_T_CONST_POINTER:
        m.type = MOCKVALUETYPE_CONST_POINTER;
        m.value.constPointerValue = v->v.cptr;
        break;
    case CUM_T_FUNCTIONPOINTER:
        m.type = MOCKVALUETYPE_FUNCTIONPOINTER;
        m.value.functionPointerValue = v->v.fptr;
        break;
    case CUM_T_MEMBUFFER:
        m.type = MOCKVALUETYPE_MEMORYBUFFER;
        m.value.memoryBufferValue = v->v.mem.buf;
        break;
    case CUM_T_OBJECT:
    case CUM_T_CONST_OBJECT:
        m.type = MOCKVALUETYPE_OBJECT;
        m.value.constObjectValue = v->v.obj.ptr;
        break;
    }
    return m;
}

static cum_value make_value(cum_type type)
{
    cum_value v;
    memset(&v, 0, sizeof v);
    v.type = type;
    return v;
}

/* --------------------------- expected call ------------------------------ */

static MockExpectedCall_c expected_table;

#define EXP_PARAM(fnname, cumtype, field, ctype)                               \
    static MockExpectedCall_c *exp_##fnname(const char *name, ctype value)     \
    {                                                                          \
        cum_value v = make_value(cumtype);                                     \
        v.v.field = value;                                                     \
        cum_expectation_with_parameter(cur_expectation, name, v);              \
        return &expected_table;                                                \
    }

EXP_PARAM(with_bool, CUM_T_BOOL, b, int)
EXP_PARAM(with_int, CUM_T_INT, i, int)
EXP_PARAM(with_uint, CUM_T_UINT, ui, unsigned int)
EXP_PARAM(with_long, CUM_T_LONG, l, long int)
EXP_PARAM(with_ulong, CUM_T_ULONG, ul, unsigned long int)
EXP_PARAM(with_ll, CUM_T_LONGLONG, ll, cpputest_longlong)
EXP_PARAM(with_ull, CUM_T_ULONGLONG, ull, cpputest_ulonglong)
EXP_PARAM(with_string, CUM_T_STRING, str, const char *)
EXP_PARAM(with_pointer, CUM_T_POINTER, ptr, void *)
EXP_PARAM(with_const_pointer, CUM_T_CONST_POINTER, cptr, const void *)

static MockExpectedCall_c *exp_with_double(const char *name, double value)
{
    cum_value v = make_value(CUM_T_DOUBLE);
    v.v.dbl.value = value;
    v.v.dbl.tolerance = 0.005;
    cum_expectation_with_parameter(cur_expectation, name, v);
    return &expected_table;
}

static MockExpectedCall_c *exp_with_double_tol(const char *name, double value,
                                               double tolerance)
{
    cum_value v = make_value(CUM_T_DOUBLE);
    v.v.dbl.value = value;
    v.v.dbl.tolerance = tolerance;
    cum_expectation_with_parameter(cur_expectation, name, v);
    return &expected_table;
}

static MockExpectedCall_c *exp_with_fp(const char *name, void (*value)(void))
{
    cum_value v = make_value(CUM_T_FUNCTIONPOINTER);
    v.v.fptr = value;
    cum_expectation_with_parameter(cur_expectation, name, v);
    return &expected_table;
}

static MockExpectedCall_c *
exp_with_membuf(const char *name, const unsigned char *value, size_t size)
{
    cum_value v = make_value(CUM_T_MEMBUFFER);
    v.v.mem.buf = value;
    v.v.mem.size = size;
    cum_expectation_with_parameter(cur_expectation, name, v);
    return &expected_table;
}

static MockExpectedCall_c *exp_with_of_type(const char *type, const char *name,
                                            const void *value)
{
    cum_value v = make_value(CUM_T_CONST_OBJECT);
    v.v.obj.type_name = type;
    v.v.obj.ptr = value;
    cum_expectation_with_parameter(cur_expectation, name, v);
    return &expected_table;
}

static MockExpectedCall_c *exp_with_out(const char *name, const void *value,
                                        size_t size)
{
    cum_expectation_with_output_parameter(cur_expectation, name, value, size);
    return &expected_table;
}

static MockExpectedCall_c *
exp_with_out_of_type(const char *type, const char *name, const void *value)
{
    cum_expectation_with_output_parameter_of_type(cur_expectation, type, name,
                                                  value);
    return &expected_table;
}

static MockExpectedCall_c *exp_with_unmodified_out(const char *name)
{
    cum_expectation_with_unmodified_output_parameter(cur_expectation, name);
    return &expected_table;
}

static MockExpectedCall_c *exp_ignore_other_parameters(void)
{
    cum_expectation_ignore_other_parameters(cur_expectation);
    return &expected_table;
}

#define EXP_RETURN(fnname, cumtype, field, ctype)                              \
    static MockExpectedCall_c *exp_ret_##fnname(ctype value)                   \
    {                                                                          \
        cum_value v = make_value(cumtype);                                     \
        v.v.field = value;                                                     \
        cum_expectation_and_return(cur_expectation, v);                        \
        return &expected_table;                                                \
    }

EXP_RETURN(bool, CUM_T_BOOL, b, int)
EXP_RETURN(int, CUM_T_INT, i, int)
EXP_RETURN(uint, CUM_T_UINT, ui, unsigned int)
EXP_RETURN(long, CUM_T_LONG, l, long int)
EXP_RETURN(ulong, CUM_T_ULONG, ul, unsigned long int)
EXP_RETURN(ll, CUM_T_LONGLONG, ll, cpputest_longlong)
EXP_RETURN(ull, CUM_T_ULONGLONG, ull, cpputest_ulonglong)
EXP_RETURN(string, CUM_T_STRING, str, const char *)
EXP_RETURN(pointer, CUM_T_POINTER, ptr, void *)
EXP_RETURN(const_pointer, CUM_T_CONST_POINTER, cptr, const void *)
EXP_RETURN(fp, CUM_T_FUNCTIONPOINTER, fptr, cum_fp)

static MockExpectedCall_c *exp_ret_double(double value)
{
    cum_value v = make_value(CUM_T_DOUBLE);
    v.v.dbl.value = value;
    v.v.dbl.tolerance = 0.0;
    cum_expectation_and_return(cur_expectation, v);
    return &expected_table;
}

static MockExpectedCall_c expected_table = {exp_with_bool,
                                            exp_with_int,
                                            exp_with_uint,
                                            exp_with_long,
                                            exp_with_ulong,
                                            exp_with_ll,
                                            exp_with_ull,
                                            exp_with_double,
                                            exp_with_double_tol,
                                            exp_with_string,
                                            exp_with_pointer,
                                            exp_with_const_pointer,
                                            exp_with_fp,
                                            exp_with_membuf,
                                            exp_with_of_type,
                                            exp_with_out,
                                            exp_with_out_of_type,
                                            exp_with_unmodified_out,
                                            exp_ignore_other_parameters,
                                            exp_ret_bool,
                                            exp_ret_uint,
                                            exp_ret_int,
                                            exp_ret_long,
                                            exp_ret_ulong,
                                            exp_ret_ll,
                                            exp_ret_ull,
                                            exp_ret_double,
                                            exp_ret_string,
                                            exp_ret_pointer,
                                            exp_ret_const_pointer,
                                            exp_ret_fp};

/* ---------------------------- actual call ------------------------------- */

static MockActualCall_c actual_table;

#define ACT_PARAM(fnname, cumtype, field, ctype)                               \
    static MockActualCall_c *act_##fnname(const char *name, ctype value)       \
    {                                                                          \
        cum_value v = make_value(cumtype);                                     \
        v.v.field = value;                                                     \
        cum_actual_with_parameter(cur_actual, name, v);                        \
        return &actual_table;                                                  \
    }

ACT_PARAM(with_bool, CUM_T_BOOL, b, int)
ACT_PARAM(with_int, CUM_T_INT, i, int)
ACT_PARAM(with_uint, CUM_T_UINT, ui, unsigned int)
ACT_PARAM(with_long, CUM_T_LONG, l, long int)
ACT_PARAM(with_ulong, CUM_T_ULONG, ul, unsigned long int)
ACT_PARAM(with_ll, CUM_T_LONGLONG, ll, cpputest_longlong)
ACT_PARAM(with_ull, CUM_T_ULONGLONG, ull, cpputest_ulonglong)
ACT_PARAM(with_string, CUM_T_STRING, str, const char *)
ACT_PARAM(with_pointer, CUM_T_POINTER, ptr, void *)
ACT_PARAM(with_const_pointer, CUM_T_CONST_POINTER, cptr, const void *)

static MockActualCall_c *act_with_double(const char *name, double value)
{
    cum_value v = make_value(CUM_T_DOUBLE);
    v.v.dbl.value = value;
    v.v.dbl.tolerance = 0.0;
    cum_actual_with_parameter(cur_actual, name, v);
    return &actual_table;
}

static MockActualCall_c *act_with_fp(const char *name, void (*value)(void))
{
    cum_value v = make_value(CUM_T_FUNCTIONPOINTER);
    v.v.fptr = value;
    cum_actual_with_parameter(cur_actual, name, v);
    return &actual_table;
}

static MockActualCall_c *
act_with_membuf(const char *name, const unsigned char *value, size_t size)
{
    cum_value v = make_value(CUM_T_MEMBUFFER);
    v.v.mem.buf = value;
    v.v.mem.size = size;
    cum_actual_with_parameter(cur_actual, name, v);
    return &actual_table;
}

static MockActualCall_c *act_with_of_type(const char *type, const char *name,
                                          const void *value)
{
    if (!cum_has_comparator(type)) {
        cum_fail_no_way_to_compare(type);
        return &actual_table;
    }
    cum_value v = make_value(CUM_T_CONST_OBJECT);
    v.v.obj.type_name = type;
    v.v.obj.ptr = value;
    cum_actual_with_parameter(cur_actual, name, v);
    return &actual_table;
}

static MockActualCall_c *act_with_out(const char *name, void *value)
{
    cum_actual_with_output_parameter(cur_actual, name, value);
    return &actual_table;
}

static MockActualCall_c *act_with_out_of_type(const char *type,
                                              const char *name, void *value)
{
    if (!cum_has_copier(type)) {
        cum_fail_no_way_to_copy(type);
        return &actual_table;
    }
    cum_actual_with_output_parameter_of_type(cur_actual, type, name, value);
    return &actual_table;
}

static int act_has_return_value(void)
{
    cum_value v;
    return cum_actual_return_value(cur_actual, &v);
}

static MockValue_c act_return_value(void)
{
    cum_value v;
    int has = cum_actual_return_value(cur_actual, &v);
    return to_mock_value(&v, has);
}

/* Upstream's typed return getters go through MockNamedValue::get*Value,
 * whose STRCMP_EQUAL type assert counts one check and FAILS the test on a
 * type mismatch. An ignored call (disabled mock, cur_actual NULL) skips the
 * assert and returns the default, like MockIgnoredActualCall. */
static void act_ret_type_assert(const char *want)
{
    cum_value v;
    int has = cum_actual_return_value(cur_actual, &v);
    cu_assert_cstr_equal(want, has ? cum_value_type_name(&v) : "", NULL,
                         __FILE__, (size_t)__LINE__);
}

#define ACT_RETURN(fnname, cumtype, field, ctype, zero, typestr)               \
    static ctype act_ret_##fnname(void)                                        \
    {                                                                          \
        cum_value v;                                                           \
        if (!cur_actual)                                                       \
            return zero;                                                       \
        act_ret_type_assert(typestr);                                          \
        if (cum_actual_return_value(cur_actual, &v) && v.type == cumtype)      \
            return v.v.field;                                                  \
        return zero;                                                           \
    }                                                                          \
    static ctype act_ret_##fnname##_default(ctype defaultValue)                \
    {                                                                          \
        cum_value v;                                                           \
        if (!cur_actual || !cum_actual_return_value(cur_actual, &v))           \
            return defaultValue;                                               \
        return act_ret_##fnname();                                             \
    }

/* integer getters mirror upstream MockNamedValue's WIDENING coercion
 * (MockNamedValue.cpp:204-285): a compatible narrower value is returned
 * without the counting type assert; only the fallback asserts. The accept
 * expression sees the fetched value as `v` and assigns `coerced`. */
#define ACT_RETURN_COERCED(fnname, field, ctype, typestr, accept)              \
    static ctype act_ret_##fnname(void)                                        \
    {                                                                          \
        cum_value v;                                                           \
        if (!cur_actual)                                                       \
            return 0;                                                          \
        if (cum_actual_return_value(cur_actual, &v)) {                         \
            ctype coerced;                                                     \
            if (accept)                                                        \
                return coerced;                                                \
        }                                                                      \
        act_ret_type_assert(typestr);                                          \
        cum_actual_return_value(cur_actual, &v);                               \
        return (ctype)v.v.field;                                               \
    }                                                                          \
    static ctype act_ret_##fnname##_default(ctype defaultValue)                \
    {                                                                          \
        cum_value v;                                                           \
        if (!cur_actual || !cum_actual_return_value(cur_actual, &v))           \
            return defaultValue;                                               \
        return act_ret_##fnname();                                             \
    }

#define COERCE(cond, expr) ((cond) ? (coerced = (expr), 1) : 0)

ACT_RETURN(bool, CUM_T_BOOL, b, int, 0, "bool")
ACT_RETURN(int, CUM_T_INT, i, int, 0, "int")
ACT_RETURN_COERCED(uint, ui, unsigned int, "unsigned int",
                   COERCE(v.type == CUM_T_INT && v.v.i >= 0,
                          (unsigned int)v.v.i))
ACT_RETURN_COERCED(long, l, long int, "long int",
                   COERCE(v.type == CUM_T_INT, v.v.i) ||
                       COERCE(v.type == CUM_T_UINT, (long int)v.v.ui))
ACT_RETURN_COERCED(ulong, ul, unsigned long int, "unsigned long int",
                   COERCE(v.type == CUM_T_UINT, v.v.ui) ||
                       COERCE(v.type == CUM_T_INT && v.v.i >= 0,
                              (unsigned long int)v.v.i) ||
                       COERCE(v.type == CUM_T_LONG && v.v.l >= 0,
                              (unsigned long int)v.v.l))
ACT_RETURN_COERCED(ll, ll, cpputest_longlong, "long long int",
                   COERCE(v.type == CUM_T_INT, v.v.i) ||
                       COERCE(v.type == CUM_T_UINT,
                              (cpputest_longlong)v.v.ui) ||
                       COERCE(v.type == CUM_T_LONG, v.v.l) ||
                       COERCE(v.type == CUM_T_ULONG, (cpputest_longlong)v.v.ul))
ACT_RETURN_COERCED(ull, ull, cpputest_ulonglong, "unsigned long long int",
                   COERCE(v.type == CUM_T_UINT, v.v.ui) ||
                       COERCE(v.type == CUM_T_INT && v.v.i >= 0,
                              (cpputest_ulonglong)v.v.i) ||
                       COERCE(v.type == CUM_T_LONG && v.v.l >= 0,
                              (cpputest_ulonglong)v.v.l) ||
                       COERCE(v.type == CUM_T_ULONG, v.v.ul) ||
                       COERCE(v.type == CUM_T_LONGLONG && v.v.ll >= 0,
                              (cpputest_ulonglong)v.v.ll))
ACT_RETURN(string, CUM_T_STRING, str, const char *, NULL, "const char*")
ACT_RETURN(pointer, CUM_T_POINTER, ptr, void *, NULL, "void*")
ACT_RETURN(const_pointer, CUM_T_CONST_POINTER, cptr, const void *, NULL,
           "const void*")
ACT_RETURN(fp, CUM_T_FUNCTIONPOINTER, fptr, cum_fp, NULL, "void (*)()")

static double act_ret_double(void)
{
    cum_value v;
    if (!cur_actual)
        return 0.0;
    act_ret_type_assert("double");
    if (cum_actual_return_value(cur_actual, &v) && v.type == CUM_T_DOUBLE)
        return v.v.dbl.value;
    return 0.0;
}

static double act_ret_double_default(double defaultValue)
{
    cum_value v;
    if (!cur_actual || !cum_actual_return_value(cur_actual, &v))
        return defaultValue;
    return act_ret_double();
}

static MockActualCall_c actual_table = {act_with_bool,
                                        act_with_int,
                                        act_with_uint,
                                        act_with_long,
                                        act_with_ulong,
                                        act_with_ll,
                                        act_with_ull,
                                        act_with_double,
                                        act_with_string,
                                        act_with_pointer,
                                        act_with_const_pointer,
                                        act_with_fp,
                                        act_with_membuf,
                                        act_with_of_type,
                                        act_with_out,
                                        act_with_out_of_type,
                                        act_has_return_value,
                                        act_return_value,
                                        act_ret_bool,
                                        act_ret_bool_default,
                                        act_ret_int,
                                        act_ret_int_default,
                                        act_ret_uint,
                                        act_ret_uint_default,
                                        act_ret_long,
                                        act_ret_long_default,
                                        act_ret_ulong,
                                        act_ret_ulong_default,
                                        act_ret_ll,
                                        act_ret_ll_default,
                                        act_ret_ull,
                                        act_ret_ull_default,
                                        act_ret_string,
                                        act_ret_string_default,
                                        act_ret_double,
                                        act_ret_double_default,
                                        act_ret_pointer,
                                        act_ret_pointer_default,
                                        act_ret_const_pointer,
                                        act_ret_const_pointer_default,
                                        act_ret_fp,
                                        act_ret_fp_default};

/* ------------------------------ support --------------------------------- */

static MockSupport_c support_table;

static void sup_strict_order(void)
{
    cum_strict_order(scope());
}

static MockExpectedCall_c *sup_expect_n_calls(unsigned int number,
                                              const char *name)
{
    cur_expectation = cum_expect_n_calls(scope(), number, name);
    return &expected_table;
}

static MockExpectedCall_c *sup_expect_one_call(const char *name)
{
    return sup_expect_n_calls(1, name);
}

static void sup_expect_no_call(const char *name)
{
    sup_expect_n_calls(0, name);
}

static MockActualCall_c *sup_actual_call(const char *name)
{
    cur_actual = cum_actual_call(scope(), name);
    return &actual_table;
}

static int sup_has_return_value(void)
{
    cum_value v;
    return cum_scope_return_value(scope(), &v);
}

static MockValue_c sup_return_value(void)
{
    cum_value v;
    int has = cum_scope_return_value(scope(), &v);
    return to_mock_value(&v, has);
}

#define SUP_RETURN(fnname, cumtype, field, ctype, zero)                        \
    static ctype sup_ret_##fnname(void)                                        \
    {                                                                          \
        cum_value v;                                                           \
        if (cum_scope_return_value(scope(), &v) && v.type == cumtype)          \
            return v.v.field;                                                  \
        return zero;                                                           \
    }                                                                          \
    static ctype sup_ret_##fnname##_default(ctype defaultValue)                \
    {                                                                          \
        cum_value v;                                                           \
        if (cum_scope_return_value(scope(), &v) && v.type == cumtype)          \
            return v.v.field;                                                  \
        return defaultValue;                                                   \
    }

SUP_RETURN(bool, CUM_T_BOOL, b, int, 0)
SUP_RETURN(int, CUM_T_INT, i, int, 0)
SUP_RETURN(uint, CUM_T_UINT, ui, unsigned int, 0)
SUP_RETURN(long, CUM_T_LONG, l, long int, 0)
SUP_RETURN(ulong, CUM_T_ULONG, ul, unsigned long int, 0)
SUP_RETURN(ll, CUM_T_LONGLONG, ll, cpputest_longlong, 0)
SUP_RETURN(ull, CUM_T_ULONGLONG, ull, cpputest_ulonglong, 0)
SUP_RETURN(string, CUM_T_STRING, str, const char *, NULL)
SUP_RETURN(pointer, CUM_T_POINTER, ptr, void *, NULL)
SUP_RETURN(const_pointer, CUM_T_CONST_POINTER, cptr, const void *, NULL)
SUP_RETURN(fp, CUM_T_FUNCTIONPOINTER, fptr, cum_fp, NULL)

static double sup_ret_double(void)
{
    cum_value v;
    if (cum_scope_return_value(scope(), &v) && v.type == CUM_T_DOUBLE)
        return v.v.dbl.value;
    return 0.0;
}

static double sup_ret_double_default(double defaultValue)
{
    cum_value v;
    if (cum_scope_return_value(scope(), &v) && v.type == CUM_T_DOUBLE)
        return v.v.dbl.value;
    return defaultValue;
}

#define SUP_SET_DATA(fnname, cumtype, field, ctype)                            \
    static void sup_set_##fnname(const char *name, ctype value)                \
    {                                                                          \
        cum_value v = make_value(cumtype);                                     \
        v.v.field = value;                                                     \
        cum_scope_set_data(scope(), name, v);                                  \
    }

SUP_SET_DATA(bool_data, CUM_T_BOOL, b, int)
SUP_SET_DATA(int_data, CUM_T_INT, i, int)
SUP_SET_DATA(uint_data, CUM_T_UINT, ui, unsigned int)
SUP_SET_DATA(long_data, CUM_T_LONG, l, long int)
SUP_SET_DATA(ulong_data, CUM_T_ULONG, ul, unsigned long int)
SUP_SET_DATA(string_data, CUM_T_STRING, str, const char *)
SUP_SET_DATA(pointer_data, CUM_T_POINTER, ptr, void *)
SUP_SET_DATA(const_pointer_data, CUM_T_CONST_POINTER, cptr, const void *)
SUP_SET_DATA(fp_data, CUM_T_FUNCTIONPOINTER, fptr, cum_fp)

static void sup_set_double_data(const char *name, double value)
{
    cum_value v = make_value(CUM_T_DOUBLE);
    v.v.dbl.value = value;
    v.v.dbl.tolerance = 0.0;
    cum_scope_set_data(scope(), name, v);
}

static void sup_set_data_object(const char *name, const char *type, void *value)
{
    cum_value v = make_value(CUM_T_OBJECT);
    v.v.obj.type_name = type;
    v.v.obj.ptr = value;
    cum_scope_set_data(scope(), name, v);
}

static void sup_set_data_const_object(const char *name, const char *type,
                                      const void *value)
{
    cum_value v = make_value(CUM_T_CONST_OBJECT);
    v.v.obj.type_name = type;
    v.v.obj.ptr = value;
    cum_scope_set_data(scope(), name, v);
}

static MockValue_c sup_get_data(const char *name)
{
    cum_value v;
    int has = cum_scope_get_data(scope(), name, &v);
    return to_mock_value(&v, has);
}

static void sup_disable(void)
{
    cum_enable(scope(), 0);
}
static void sup_enable(void)
{
    cum_enable(scope(), 1);
}
static void sup_ignore_other_calls(void)
{
    cum_ignore_other_calls(scope());
}
static void sup_check_expectations(void)
{
    cum_check_expectations_all();
}
static int sup_expected_calls_left(void)
{
    return cum_expected_calls_left_all();
}
static void sup_clear(void)
{
    cum_clear_all();
}
static void sup_crash_on_failure(unsigned shouldCrash)
{
    cum_crash_on_failure(shouldCrash ? 1 : 0);
}

/* comparator/copier adapters: C function pairs live in a small pool used as
 * the core's ctx */

typedef struct {
    MockTypeEqualFunction_c equal;
    MockTypeValueToStringFunction_c to_string;
    MockTypeCopyFunction_c copy;
} c_adapter;

#define MAX_C_ADAPTERS 32
static c_adapter adapters[MAX_C_ADAPTERS];
static int adapter_count;

static int adapter_equal(void *ctx, const void *o1, const void *o2)
{
    return ((c_adapter *)ctx)->equal(o1, o2);
}

static char *adapter_to_string(void *ctx, const void *o)
{
    return cu_str_printf("%s", ((c_adapter *)ctx)->to_string(o));
}

static void adapter_copy(void *ctx, void *dst, const void *src)
{
    ((c_adapter *)ctx)->copy(dst, src);
}

static void
sup_install_comparator(const char *typeName, MockTypeEqualFunction_c isEqual,
                       MockTypeValueToStringFunction_c valueToString)
{
    if (adapter_count >= MAX_C_ADAPTERS)
        return;
    c_adapter *a = &adapters[adapter_count++];
    a->equal = isEqual;
    a->to_string = valueToString;
    cum_install_comparator(typeName, a, adapter_equal, adapter_to_string);
}

static void sup_install_copier(const char *typeName,
                               MockTypeCopyFunction_c copier)
{
    if (adapter_count >= MAX_C_ADAPTERS)
        return;
    c_adapter *a = &adapters[adapter_count++];
    a->copy = copier;
    cum_install_copier(typeName, a, adapter_copy);
}

static void sup_remove_all_comparators_and_copiers(void)
{
    cum_remove_all_comparators_and_copiers();
    adapter_count = 0;
}

static MockSupport_c support_table = {sup_strict_order,
                                      sup_expect_one_call,
                                      sup_expect_no_call,
                                      sup_expect_n_calls,
                                      sup_actual_call,
                                      sup_has_return_value,
                                      sup_return_value,
                                      sup_ret_bool,
                                      sup_ret_bool_default,
                                      sup_ret_int,
                                      sup_ret_int_default,
                                      sup_ret_uint,
                                      sup_ret_uint_default,
                                      sup_ret_long,
                                      sup_ret_long_default,
                                      sup_ret_ulong,
                                      sup_ret_ulong_default,
                                      sup_ret_ll,
                                      sup_ret_ll_default,
                                      sup_ret_ull,
                                      sup_ret_ull_default,
                                      sup_ret_string,
                                      sup_ret_string_default,
                                      sup_ret_double,
                                      sup_ret_double_default,
                                      sup_ret_pointer,
                                      sup_ret_pointer_default,
                                      sup_ret_const_pointer,
                                      sup_ret_const_pointer_default,
                                      sup_ret_fp,
                                      sup_ret_fp_default,
                                      sup_set_bool_data,
                                      sup_set_int_data,
                                      sup_set_uint_data,
                                      sup_set_long_data,
                                      sup_set_ulong_data,
                                      sup_set_string_data,
                                      sup_set_double_data,
                                      sup_set_pointer_data,
                                      sup_set_const_pointer_data,
                                      sup_set_fp_data,
                                      sup_set_data_object,
                                      sup_set_data_const_object,
                                      sup_get_data,
                                      sup_disable,
                                      sup_enable,
                                      sup_ignore_other_calls,
                                      sup_check_expectations,
                                      sup_expected_calls_left,
                                      sup_clear,
                                      sup_crash_on_failure,
                                      sup_install_comparator,
                                      sup_install_copier,
                                      sup_remove_all_comparators_and_copiers};

MockSupport_c *mock_c(void)
{
    cur_scope = cum_scope_get("");
    return &support_table;
}

MockSupport_c *mock_scope_c(const char *scope_name)
{
    cur_scope = cum_scope_get(scope_name ? scope_name : "");
    return &support_table;
}
