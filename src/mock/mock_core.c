#define _POSIX_C_SOURCE 200809L

#include "cpputest_core/mock.h"
#include "cpputest_core/core.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* C core of CppUMock. Current slice: expectation counting, parameterless
 * actual-call matching, strict order, and the basic failure messages with
 * upstream-exact text (docs/INTERFACE.md §10). Parameters/returns/scoped
 * recursion arrive in later slices. */

/* --------------------------- string builder ----------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} msb;

static void msb_init(msb *b)
{
    b->cap = 512;
    b->len = 0;
    b->buf = cu_xmalloc(b->cap);
    b->buf[0] = '\0';
}

CU_FORMAT_PRINTF(2, 3)
static void msb_addf(msb *b, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (n > 0) {
        if (b->len + (size_t)n + 1 > b->cap) {
            while (b->len + (size_t)n + 1 > b->cap)
                b->cap *= 2;
            b->buf = cu_xrealloc(b->buf, b->cap);
        }
        vsnprintf(b->buf + b->len, (size_t)n + 1, format, args);
        b->len += (size_t)n;
    }
    va_end(args);
}

static void msb_add(msb *b, const char *s)
{
    msb_addf(b, "%s", s);
}

/* ------------------------------ values ---------------------------------- */

/* MockNamedValue type names, indexed by cum_type */
static const char *type_names[] = {"bool",
                                   "int",
                                   "unsigned int",
                                   "long int",
                                   "unsigned long int",
                                   "long long int",
                                   "unsigned long long int",
                                   "double",
                                   "const char*",
                                   "void*",
                                   "const void*",
                                   "void (*)()",
                                   "const unsigned char*"};

const char *cum_value_type_name(const cum_value *v)
{
    if (v->type == CUM_T_OBJECT || v->type == CUM_T_CONST_OBJECT)
        return v->v.obj.type_name ? v->v.obj.type_name : "";
    return type_names[v->type];
}

static int type_is_integer(cum_type t)
{
    return t == CUM_T_INT || t == CUM_T_UINT || t == CUM_T_LONG ||
           t == CUM_T_ULONG || t == CUM_T_LONGLONG || t == CUM_T_ULONGLONG;
}

static int type_is_signed(cum_type t)
{
    return t == CUM_T_INT || t == CUM_T_LONG || t == CUM_T_LONGLONG;
}

static long long as_signed(const cum_value *v)
{
    switch (v->type) {
    case CUM_T_INT:
        return v->v.i;
    case CUM_T_LONG:
        return v->v.l;
    default:
        return v->v.ll;
    }
}

static unsigned long long as_unsigned(const cum_value *v)
{
    switch (v->type) {
    case CUM_T_UINT:
        return v->v.ui;
    case CUM_T_ULONG:
        return v->v.ul;
    default:
        return v->v.ull;
    }
}

static int mock_doubles_equal(double d1, double d2, double threshold)
{
    if (d1 != d1 || d2 != d2 || threshold != threshold) /* NaN */
        return 0;
    double diff = d1 - d2;
    if (diff < 0)
        diff = -diff;
    return diff <= threshold;
}

/* --------------------- comparators / copiers ---------------------------- */

typedef struct cum_comparator {
    char *type_name;
    void *ctx;
    cum_comparator_equal_fn equal;
    cum_comparator_string_fn to_string;
    cum_copier_fn copy; /* copier entries reuse this struct */
    struct cum_comparator *next;
} cum_comparator;

static cum_comparator *find_in(cum_comparator *list, const char *type_name)
{
    for (cum_comparator *c = list; c; c = c->next)
        if (0 == strcmp(c->type_name, type_name))
            return c;
    return NULL;
}

/* Comparator/copier binding captured at parameter CREATION: upstream's
 * MockNamedValue::setObjectPointer resolves from the repository at that
 * moment and the binding survives removeAllComparatorsAndCopiers (it
 * references the user's comparator object, not our registry node — which
 * is why the user fn/ctx pointers are copied out of the node). */
typedef struct {
    void *ctx;
    cum_comparator_equal_fn equal;
    cum_comparator_string_fn to_string;
    cum_copier_fn copy;
    int present;
} cum_binding;

static void free_comparator_list(cum_comparator **list)
{
    cum_comparator *c = *list;
    while (c) {
        cum_comparator *next = c->next;
        free(c->type_name);
        free(c);
        c = next;
    }
    *list = NULL;
}

/* install/remove/has are defined after cum_scope below: repositories are
 * PER SCOPE upstream (each MockSupport owns one), with mock(name) pointing
 * the process-wide binding source at the touched scope's repository */

/* MockNamedValue::equals — the integer cross-type matrix collapses to
 * sign-aware mathematical equality, which is exactly what upstream's
 * pairwise rules implement. `e` is the expected side (its tolerance is
 * used for doubles). */
static int value_equals(const cum_value *e, const cum_value *a,
                        const cum_binding *eb)
{
    if (type_is_integer(e->type) && type_is_integer(a->type)) {
        int es = type_is_signed(e->type), as = type_is_signed(a->type);
        if (es && as)
            return as_signed(e) == as_signed(a);
        if (!es && !as)
            return as_unsigned(e) == as_unsigned(a);
        const cum_value *sv = es ? e : a;
        const cum_value *uv = es ? a : e;
        return as_signed(sv) >= 0 &&
               (unsigned long long)as_signed(sv) == as_unsigned(uv);
    }
    if (e->type != a->type)
        return 0;
    switch (e->type) {
    case CUM_T_BOOL:
        return e->v.b == a->v.b;
    case CUM_T_DOUBLE:
        return mock_doubles_equal(e->v.dbl.value, a->v.dbl.value,
                                  e->v.dbl.tolerance);
    case CUM_T_STRING:
        return 0 == strcmp(e->v.str ? e->v.str : "", a->v.str ? a->v.str : "");
    case CUM_T_POINTER:
        return e->v.ptr == a->v.ptr;
    case CUM_T_CONST_POINTER:
        return e->v.cptr == a->v.cptr;
    case CUM_T_FUNCTIONPOINTER:
        return e->v.fptr == a->v.fptr;
    case CUM_T_MEMBUFFER:
        return e->v.mem.size == a->v.mem.size &&
               0 == memcmp(e->v.mem.buf, a->v.mem.buf, e->v.mem.size);
    case CUM_T_OBJECT:
    case CUM_T_CONST_OBJECT: {
        if (0 != strcmp(cum_value_type_name(e), cum_value_type_name(a)))
            return 0;
        /* the EXPECTED param's creation-time binding decides equality
         * (upstream MockNamedValue::equals uses this->comparator_) */
        if (eb && eb->present && eb->equal)
            return eb->equal(eb->ctx, e->v.obj.ptr, a->v.obj.ptr);
        return 0;
    }
    default:
        return 0;
    }
}

/* MockNamedValue::toString — malloc'd. `b` is the creation-time binding
 * for object values (NULL where objects cannot occur). */
static char *value_to_string(const cum_value *v, const cum_binding *b)
{
    switch (v->type) {
    case CUM_T_BOOL:
        return cu_str_printf("%s", v->v.b ? "true" : "false");
    case CUM_T_INT:
        return cu_str_printf("%d (0x%x)", v->v.i, (unsigned)v->v.i);
    case CUM_T_UINT:
        return cu_str_printf("%u (0x%x)", v->v.ui, v->v.ui);
    case CUM_T_LONG:
        return cu_str_printf("%ld (0x%lx)", v->v.l, (unsigned long)v->v.l);
    case CUM_T_ULONG:
        return cu_str_printf("%lu (0x%lx)", v->v.ul, v->v.ul);
    case CUM_T_LONGLONG:
        return cu_str_printf("%lld (0x%llx)", v->v.ll,
                             (unsigned long long)v->v.ll);
    case CUM_T_ULONGLONG:
        return cu_str_printf("%llu (0x%llx)", v->v.ull, v->v.ull);
    case CUM_T_DOUBLE:
        return cu_str_from_double(v->v.dbl.value, 6);
    case CUM_T_STRING:
        return cu_str_printf("%s", v->v.str ? v->v.str : "");
    case CUM_T_POINTER:
        return cu_str_printf("0x%llx", (unsigned long long)(size_t)v->v.ptr);
    case CUM_T_CONST_POINTER:
        return cu_str_printf("0x%llx", (unsigned long long)(size_t)v->v.cptr);
    case CUM_T_FUNCTIONPOINTER:
        return cu_str_printf("0x%llx", (unsigned long long)(size_t)v->v.fptr);
    case CUM_T_MEMBUFFER: {
        if (!v->v.mem.buf)
            return cu_str_printf("(null)");
        size_t shown = v->v.mem.size > 128 ? 128 : v->v.mem.size;
        char *hex = cu_str_from_binary(v->v.mem.buf, shown);
        char *result = cu_str_printf("Size = %u | HexContents = %s%s",
                                     (unsigned)v->v.mem.size, hex,
                                     v->v.mem.size > shown ? " ..." : "");
        cu_str_free(hex);
        return result;
    }
    case CUM_T_OBJECT:
    case CUM_T_CONST_OBJECT: {
        if (b && b->present && b->to_string)
            return b->to_string(b->ctx, v->v.obj.ptr);
        return cu_str_printf("No comparator found for type: \"%s\"",
                             cum_value_type_name(v));
    }
    default:
        return cu_str_printf("?");
    }
}

/* ------------------------------ data model ------------------------------ */

#define CUM_NO_CALL_ORDER 0

typedef struct cum_param {
    char *name;
    cum_value value;
    char *owned_type;  /* copy of the custom type name for object values */
    cum_binding bound; /* comparator bound when the param was created */
    int matched;       /* matched by the in-progress actual call */
    struct cum_param *next;
} cum_param;

typedef struct cum_out_param {
    char *name;
    char *type_name;   /* NULL = plain "const void*" output parameter */
    const void *value; /* NULL = withUnmodifiedOutputParameter */
    size_t size;
    cum_binding bound; /* copier bound when the param was created */
    int matched;
    struct cum_out_param *next;
} cum_out_param;

/* capture the current registry entry for a type, by value */
static cum_binding bind_from(cum_comparator *list, const char *type_name)
{
    cum_binding b = {NULL, NULL, NULL, NULL, 0};
    cum_comparator *c = type_name ? find_in(list, type_name) : NULL;
    if (c) {
        b.ctx = c->ctx;
        b.equal = c->equal;
        b.to_string = c->to_string;
        b.copy = c->copy;
        b.present = 1;
    }
    return b;
}

struct cum_expectation {
    char *name;
    cum_param *params; /* input parameters, declaration order */
    cum_out_param *out_params;
    unsigned expected_calls;
    unsigned actual_calls;
    unsigned order_start; /* CUM_NO_CALL_ORDER = unordered */
    unsigned order_end;
    int out_of_order;
    int ignore_other_parameters;
    int object_specific; /* onObject() was used */
    const void *object_ptr;
    int passed_to_object; /* 1 unless object_specific and not yet passed */
    int candidate;        /* potentially matching the in-progress actual call */
    int has_return;
    cum_value return_value;
    void *user; /* C++ facade */
    struct cum_expectation *next;
};

typedef enum {
    CUM_CALL_IN_PROGRESS,
    CUM_CALL_SUCCEED,
    CUM_CALL_FAILED
} cum_call_state;

typedef struct cum_out_dst {
    char *name;
    char *type_name; /* NULL = plain */
    void *dst;
    struct cum_out_dst *next;
} cum_out_dst;

struct cum_actual {
    char *name;
    cum_out_dst *out_dsts;
    unsigned call_order;
    cum_call_state state;
    int expectations_checked;
    cum_expectation *matching;
    cum_scope *scope;
};

int cum_actual_is_checked(const cum_actual *a)
{
    return a != NULL;
}

struct cum_scope {
    char *name;
    int enabled;
    int zombie; /* upstream's global clear DELETES child scopes; a zombie is
                   re-initialized (re-cloned) on next access */
    int strict_ordering;
    int ignore_other_calls;
    unsigned actual_call_order;
    unsigned expected_call_order;
    cum_expectation *expectations; /* append order preserved */
    cum_actual *last_actual;
    cum_comparator *comparators; /* per-scope repository, like upstream */
    cum_comparator *copiers;
    struct cum_scope *next;
};

static cum_scope *scopes;
/* upstream MockNamedValue::defaultRepository_: bindings and the has-checks
 * read the repository of the scope LAST RETURNED by cum_scope_get (every
 * mock()/mock_scope_c() access re-points it) */
static cum_scope *bind_scope;
static int crash_on_failure;
static void (*facade_free)(void *);

static int scope_name_is_global(const cum_scope *s)
{
    return s->name == NULL || s->name[0] == '\0';
}

static void install_node(cum_comparator **list, const char *type_name,
                         void *ctx, cum_comparator_equal_fn equal,
                         cum_comparator_string_fn to_string, cum_copier_fn copy)
{
    cum_comparator *c = cu_xcalloc(1, sizeof *c);
    c->type_name = cu_xstrdup(type_name);
    c->ctx = ctx;
    c->equal = equal;
    c->to_string = to_string;
    c->copy = copy;
    c->next = *list;
    *list = c;
}

/* upstream MockSupport::installComparator: into own repository (PREPEND),
 * then the same entry into every existing child's repository */
void cum_install_comparator(cum_scope *s, const char *type_name, void *ctx,
                            cum_comparator_equal_fn equal,
                            cum_comparator_string_fn to_string)
{
    install_node(&s->comparators, type_name, ctx, equal, to_string, NULL);
    if (scope_name_is_global(s))
        for (cum_scope *c = scopes; c; c = c->next)
            if (c != s && c->zombie == 0)
                install_node(&c->comparators, type_name, ctx, equal, to_string,
                             NULL);
}

void cum_install_copier(cum_scope *s, const char *type_name, void *ctx,
                        cum_copier_fn copy)
{
    install_node(&s->copiers, type_name, ctx, NULL, NULL, copy);
    if (scope_name_is_global(s))
        for (cum_scope *c = scopes; c; c = c->next)
            if (c != s && c->zombie == 0)
                install_node(&c->copiers, type_name, ctx, NULL, NULL, copy);
}

/* upstream MockSupport::removeAllComparatorsAndCopiers: own repository,
 * then every child's */
void cum_remove_all_comparators_and_copiers(cum_scope *s)
{
    free_comparator_list(&s->comparators);
    free_comparator_list(&s->copiers);
    if (scope_name_is_global(s)) {
        for (cum_scope *c = scopes; c; c = c->next) {
            if (c == s)
                continue;
            free_comparator_list(&c->comparators);
            free_comparator_list(&c->copiers);
        }
    }
}

/* the has-checks consult the binding source (defaultRepository_) */
int cum_has_comparator(const char *type_name)
{
    return bind_scope && find_in(bind_scope->comparators, type_name) != NULL;
}

int cum_has_copier(const char *type_name)
{
    return bind_scope && find_in(bind_scope->copiers, type_name) != NULL;
}

/* upstream clone(): the child repository is filled by iterating the
 * source head-to-tail while PREPENDING, which REVERSES the order — so the
 * OLDEST install wins lookups inside cloned scopes (quirk preserved) */
static void clone_repositories(cum_scope *child, const cum_scope *global)
{
    for (const cum_comparator *c = global->comparators; c; c = c->next)
        install_node(&child->comparators, c->type_name, c->ctx, c->equal,
                     c->to_string, c->copy);
    for (const cum_comparator *c = global->copiers; c; c = c->next)
        install_node(&child->copiers, c->type_name, c->ctx, c->equal,
                     c->to_string, c->copy);
}

/* ------------------------------- scopes --------------------------------- */

cum_scope *cum_scope_get(const char *name)
{
    if (!name)
        name = "";
    for (cum_scope *s = scopes; s; s = s->next)
        if (0 == strcmp(s->name, name)) {
            if (s->zombie && name[0] != '\0') {
                /* deleted by a global clear: next access re-creates it
                 * upstream (clone) — re-inherit, and no existence check */
                cum_scope *global = cum_scope_get("");
                s->zombie = 0;
                s->enabled = global->enabled;
                s->ignore_other_calls = global->ignore_other_calls;
                s->strict_ordering = global->strict_ordering;
                clone_repositories(s, global);
                bind_scope = s;
                return s;
            }
            /* upstream getMockSupportScope runs an internal STRCMP_EQUAL
             * sanity assert when fetching an EXISTING scope — which counts
             * one user-visible check per access (quirk preserved) */
            if (name[0] != '\0')
                cu_count_check();
            bind_scope = s;
            return s;
        }
    cum_scope *s = cu_xcalloc(1, sizeof *s);
    s->name = cu_xstrdup(name);
    s->enabled = 1;
    if (name[0] != '\0') {
        /* upstream clone(): a lazily-created scope inherits the global
         * mock's enabled/ignoreOtherCalls/strictOrdering state and a
         * REVERSED copy of its comparator/copier repository */
        cum_scope *global = cum_scope_get("");
        s->enabled = global->enabled;
        s->ignore_other_calls = global->ignore_other_calls;
        s->strict_ordering = global->strict_ordering;
        clone_repositories(s, global);
    }
    /* append: failure dumps walk the global mock first, then child scopes
     * in creation order, like upstream's data_ list */
    s->next = NULL;
    cum_scope **link = &scopes;
    while (*link)
        link = &(*link)->next;
    *link = s;
    bind_scope = s;
    return s;
}

static char *scoped_name(const cum_scope *s, const char *name)
{
    if (s->name[0])
        return cu_str_printf("%s::%s", s->name, name);
    return cu_xstrdup(name);
}

/* --------------------------- failure plumbing --------------------------- */

/* MockFailureReporter::failTest: only fail if the test hasn't already
 * failed; MockFailure attributes to the test's own file/line. */
static void mock_fail(msb *message)
{
    cu_test *t = cu_current_test();
    char *msg = message->buf;
    if (t && !t->has_failed) {
        /* the message must survive the longjmp out of cu_fail_with_message;
         * park it in a static slot (released on the next failure) instead
         * of a fixed-size copy that would truncate large histories */
        static char *parked;
        free(parked);
        parked = msg;
        if (crash_on_failure) {
            /* upstream reports the failure FIRST, then crashes so a
             * debugger lands after the message is out */
            cu_add_failure(t->file, t->line, parked);
            __builtin_trap();
        }
        cu_fail_with_message(t->file, t->line, parked); /* longjmps */
    }
    free(msg);
}

/* MockCheckedExpectedCall::callToString */
static void msb_call_to_string(msb *b, const cum_expectation *e)
{
    if (e->object_specific)
        msb_addf(b, "(object address: %p)::", e->object_ptr);
    msb_add(b, e->name);
    msb_add(b, " -> ");
    if (e->order_start != CUM_NO_CALL_ORDER) {
        if (e->order_start == e->order_end)
            msb_addf(b, "expected call order: <%u> -> ", e->order_start);
        else
            msb_addf(b, "expected calls order: <%u..%u> -> ", e->order_start,
                     e->order_end);
    }
    if (!e->params && !e->out_params) {
        msb_add(b, e->ignore_other_parameters ? "all parameters ignored"
                                              : "no parameters");
    } else {
        for (cum_param *p = e->params; p; p = p->next) {
            /* upstream renders the value via getInputParameterValueString,
             * a first-by-name lookup: duplicate names all show the first
             * entry's value (quirk preserved) */
            const cum_param *named = e->params;
            while (named && 0 != strcmp(named->name, p->name))
                named = named->next;
            const cum_param *shown = named ? named : p;
            char *value = value_to_string(&shown->value, &shown->bound);
            msb_addf(b, "%s %s: <%s>", cum_value_type_name(&p->value), p->name,
                     value);
            cu_str_free(value);
            if (p->next)
                msb_add(b, ", ");
        }
        if (e->params && e->out_params)
            msb_add(b, ", ");
        for (cum_out_param *o = e->out_params; o; o = o->next) {
            msb_addf(b, "%s %s: <output>",
                     o->type_name ? o->type_name : "const void*", o->name);
            if (o->next)
                msb_add(b, ", ");
        }
        if (e->ignore_other_parameters)
            msb_add(b, ", other parameters are ignored");
    }
    msb_addf(b, " (expected %u call%s, called %u time%s)", e->expected_calls,
             e->expected_calls == 1 ? "" : "s", e->actual_calls,
             e->actual_calls == 1 ? "" : "s");
}

/* MockCheckedExpectedCall::missingParametersToString */
static void msb_missing_params(msb *b, const cum_expectation *e)
{
    int first = 1;
    for (cum_param *p = e->params; p; p = p->next) {
        if (p->matched)
            continue;
        if (!first)
            msb_add(b, ", ");
        first = 0;
        msb_addf(b, "%s %s", cum_value_type_name(&p->value), p->name);
    }
    for (cum_out_param *o = e->out_params; o; o = o->next) {
        if (o->matched)
            continue;
        if (!first)
            msb_add(b, ", ");
        first = 0;
        msb_addf(b, "%s %s", o->type_name ? o->type_name : "const void*",
                 o->name);
    }
}

static int is_fulfilled(const cum_expectation *e)
{
    return e->actual_calls == e->expected_calls;
}

/* predicate selector for list rendering */
typedef int (*exp_pred)(const cum_expectation *e, const char *arg);

static int pred_unfulfilled(const cum_expectation *e, const char *arg)
{
    (void)arg;
    return !is_fulfilled(e);
}

static int pred_fulfilled(const cum_expectation *e, const char *arg)
{
    (void)arg;
    return is_fulfilled(e);
}

static int pred_unfulfilled_named(const cum_expectation *e, const char *name)
{
    return !is_fulfilled(e) && 0 == strcmp(e->name, name);
}

static int pred_fulfilled_named(const cum_expectation *e, const char *name)
{
    return is_fulfilled(e) && 0 == strcmp(e->name, name);
}

static int pred_unfulfilled_out_of_order(const cum_expectation *e,
                                         const char *arg)
{
    (void)arg;
    return !is_fulfilled(e) && e->out_of_order;
}

static int pred_fulfilled_out_of_order(const cum_expectation *e,
                                       const char *arg)
{
    (void)arg;
    return is_fulfilled(e) && e->out_of_order;
}

/* render matching expectations one per line with prefix; "<none>" if empty.
 * all_scopes: walk every scope (didnt-happen failure) or just one. */
static void msb_calls_list(msb *b, const char *prefix, cum_scope *only_scope,
                           exp_pred pred, const char *arg)
{
    int count = 0;
    for (cum_scope *s = scopes; s; s = s->next) {
        if (only_scope && s != only_scope)
            continue;
        for (cum_expectation *e = s->expectations; e; e = e->next) {
            if (!pred(e, arg))
                continue;
            if (count++)
                msb_add(b, "\n");
            msb_add(b, prefix);
            msb_call_to_string(b, e);
        }
    }
    if (!count) {
        msb_add(b, prefix);
        msb_add(b, "<none>");
    }
}

static void msb_history(msb *b, cum_scope *only_scope, exp_pred unful,
                        exp_pred ful, const char *arg)
{
    msb_add(b, "\tEXPECTED calls that WERE NOT fulfilled:\n");
    msb_calls_list(b, "\t\t", only_scope, unful, arg);
    msb_add(b, "\n\tEXPECTED calls that WERE fulfilled:\n");
    msb_calls_list(b, "\t\t", only_scope, ful, arg);
}

/* addExpectationsAndCallHistoryRelatedTo */
static void msb_history_related(msb *b, cum_scope *s, const char *name)
{
    msb_addf(
        b, "\tEXPECTED calls that WERE NOT fulfilled related to function: %s\n",
        name);
    msb_calls_list(b, "\t\t", s, pred_unfulfilled_named, name);
    msb_addf(b,
             "\n\tEXPECTED calls that WERE fulfilled related to function: %s\n",
             name);
    msb_calls_list(b, "\t\t", s, pred_fulfilled_named, name);
}

static void fail_unexpected_input_parameter(cum_scope *s, const char *fn,
                                            const char *param_name,
                                            const cum_value *value)
{
    /* name case: no expectation for this function has the parameter name */
    int name_known = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (0 != strcmp(e->name, fn))
            continue;
        for (cum_param *p = e->params; p; p = p->next)
            if (0 == strcmp(p->name, param_name))
                name_known = 1;
    }

    /* the failing param is being created right now, so a fresh registry
     * lookup IS its creation-time binding */
    cum_binding vb = bind_from(bind_scope ? bind_scope->comparators : NULL,
                               cum_value_type_name(value));
    char *value_str = value_to_string(value, &vb);
    msb b;
    msb_init(&b);
    if (!name_known)
        msb_addf(
            &b,
            "Mock Failure: Unexpected parameter name to function \"%s\": %s",
            fn, param_name);
    else
        msb_addf(&b,
                 "Mock Failure: Unexpected parameter value to parameter \"%s\" "
                 "to function \"%s\": <%s>",
                 param_name, fn, value_str);
    msb_add(&b, "\n");
    msb_history_related(&b, s, fn);
    msb_addf(&b, "\n\tACTUAL unexpected parameter passed to function: %s\n",
             fn);
    msb_addf(&b, "\t\t%s %s: <%s>", cum_value_type_name(value), param_name,
             value_str);
    cu_str_free(value_str);
    mock_fail(&b);
}

/* MockUnexpectedOutputParameterFailure. actual_type is the ACTUAL-side
 * parameter type ("void*" for plain withOutputParameter, the custom type
 * for OfType). Upstream picks the headline by scanning ALL expectations
 * related to the function (not just current candidates) for any output
 * parameter with this NAME: found -> wrong-type case, else unknown-name. */
static void fail_unexpected_output_parameter(cum_scope *s, const char *fn,
                                             const char *param_name,
                                             const char *actual_type)
{
    int name_known = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (0 != strcmp(e->name, fn))
            continue;
        for (cum_out_param *o = e->out_params; o; o = o->next)
            if (0 == strcmp(o->name, param_name))
                name_known = 1;
    }

    msb b;
    msb_init(&b);
    if (name_known)
        msb_addf(&b,
                 "Mock Failure: Unexpected parameter type \"%s\" to output "
                 "parameter \"%s\" to function \"%s\"",
                 actual_type, param_name, fn);
    else
        msb_addf(&b,
                 "Mock Failure: Unexpected output parameter name to function "
                 "\"%s\": %s",
                 fn, param_name);
    msb_add(&b, "\n");
    msb_history_related(&b, s, fn);
    msb_addf(&b,
             "\n\tACTUAL unexpected output parameter passed to function: %s\n",
             fn);
    msb_addf(&b, "\t\t%s %s", actual_type, param_name);
    mock_fail(&b);
}

/* MockUnexpectedObjectFailure */
static void fail_unexpected_object(cum_scope *s, const char *fn,
                                   const void *object_ptr)
{
    msb b;
    msb_init(&b);
    msb_addf(&b,
             "MockFailure: Function called on an unexpected object: %s\n"
             "\tActual object for call has address: <%p>\n",
             fn, object_ptr);
    msb_history_related(&b, s, fn);
    mock_fail(&b);
}

/* MockExpectedObjectDidntHappenFailure */
static void fail_expected_object_didnt_happen(cum_scope *s, const char *fn)
{
    msb b;
    msb_init(&b);
    msb_addf(&b,
             "Mock Failure: Expected call on object for function \"%s\" but it "
             "did not happen.\n",
             fn);
    msb_history_related(&b, s, fn);
    mock_fail(&b);
}

static void fail_expected_parameter_didnt_happen(cum_scope *s, const char *fn)
{
    msb b;
    msb_init(&b);
    msb_addf(&b,
             "Mock Failure: Expected parameter for function \"%s\" did not "
             "happen.\n",
             fn);
    msb_addf(
        &b,
        "\tEXPECTED calls with MISSING parameters related to function: %s\n",
        fn);
    /* candidates of the in-progress call, two lines each */
    int count = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (!e->candidate)
            continue;
        if (count++)
            msb_add(&b, "\n");
        msb_add(&b, "\t\t");
        msb_call_to_string(&b, e);
        msb_add(&b, "\n\t\t\tMISSING parameters: ");
        msb_missing_params(&b, e);
    }
    if (!count)
        msb_add(&b, "\t\t<none>");
    msb_add(&b, "\n");
    msb_history_related(&b, s, fn);
    mock_fail(&b);
}

static void fail_unexpected_call(cum_scope *s, const char *name)
{
    unsigned fulfilled_actuals = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next)
        if (0 == strcmp(e->name, name))
            fulfilled_actuals += e->actual_calls;

    msb b;
    msb_init(&b);
    if (fulfilled_actuals > 0) {
        unsigned n = fulfilled_actuals + 1;
        const char *suffix = "th";
        if (n < 11 || n > 13) {
            unsigned ones = n % 10;
            if (ones == 3)
                suffix = "rd";
            else if (ones == 2)
                suffix = "nd";
            else if (ones == 1)
                suffix = "st";
        }
        msb_addf(
            &b,
            "Mock Failure: Unexpected additional (%u%s) call to function: ", n,
            suffix);
    } else {
        msb_add(&b, "Mock Failure: Unexpected call to function: ");
    }
    msb_add(&b, name);
    msb_add(&b, "\n");
    msb_history(&b, s, pred_unfulfilled, pred_fulfilled, NULL);
    mock_fail(&b);
}

/* only_scope NULL = whole-tree history (the global mock); a child scope's
 * checkExpectations lists only its own expectations, like upstream's
 * failTestWithExpectedCallsNotFulfilled over expectations_ + children */
static void fail_expected_calls_didnt_happen(cum_scope *only_scope)
{
    msb b;
    msb_init(&b);
    msb_add(&b, "Mock Failure: Expected call WAS NOT fulfilled.\n");
    msb_history(&b, only_scope, pred_unfulfilled, pred_fulfilled, NULL);
    mock_fail(&b);
}

static void fail_out_of_order_calls(cum_scope *only_scope)
{
    msb b;
    msb_init(&b);
    msb_add(&b, "Mock Failure: Out of order calls");
    msb_add(&b, "\n");
    msb_history(&b, only_scope, pred_unfulfilled_out_of_order,
                pred_fulfilled_out_of_order, NULL);
    mock_fail(&b);
}

/* ------------------------------ lifecycle ------------------------------- */

void cum_strict_order(cum_scope *s)
{
    s->strict_ordering = 1;
}

cum_expectation *cum_expect_n_calls(cum_scope *s, unsigned amount,
                                    const char *name)
{
    if (!s->enabled)
        return NULL;
    cu_count_check();

    cum_expectation *e = cu_xcalloc(1, sizeof *e);
    e->name = scoped_name(s, name);
    e->expected_calls = amount;
    e->passed_to_object = 1; /* no specific object expected by default */
    if (s->strict_ordering) {
        e->order_start = s->expected_call_order + 1;
        e->order_end = s->expected_call_order + amount;
        s->expected_call_order += amount;
    }
    /* append, preserving declaration order for history rendering */
    cum_expectation **link = &s->expectations;
    while (*link)
        link = &(*link)->next;
    *link = e;
    return e;
}

void cum_expectation_with_call_order(cum_expectation *e, unsigned start,
                                     unsigned end)
{
    if (!e)
        return;
    e->order_start = start;
    e->order_end = end;
}

void cum_expectation_ignore_other_parameters(cum_expectation *e)
{
    if (e)
        e->ignore_other_parameters = 1;
}

static void actual_finalize(cum_actual *a);
static void complete_call_when_match_found(cum_actual *a);
static void reset_param_match_state(cum_expectation *e);
static int is_finalized_matching(const cum_expectation *e);

static void expectation_add_out_param(cum_expectation *e, const char *type_name,
                                      const char *name, const void *value,
                                      size_t size)
{
    if (!e)
        return;
    cum_out_param *o = cu_xcalloc(1, sizeof *o);
    o->name = cu_xstrdup(name);
    o->type_name = type_name ? cu_xstrdup(type_name) : NULL;
    o->value = value;
    o->size = size;
    /* bind-at-creation, like input params */
    o->bound = bind_from(bind_scope ? bind_scope->copiers : NULL, o->type_name);
    cum_out_param **link = &e->out_params;
    while (*link)
        link = &(*link)->next;
    *link = o;
}

void cum_expectation_with_output_parameter(cum_expectation *e, const char *name,
                                           const void *value, size_t size)
{
    expectation_add_out_param(e, NULL, name, value, size);
}

void cum_expectation_with_output_parameter_of_type(cum_expectation *e,
                                                   const char *type_name,
                                                   const char *name,
                                                   const void *value)
{
    expectation_add_out_param(e, type_name, name, value, 0);
}

void cum_expectation_with_unmodified_output_parameter(cum_expectation *e,
                                                      const char *name)
{
    expectation_add_out_param(e, NULL, name, NULL, 0);
}

static void fail_unexpected_output_parameter(cum_scope *s, const char *fn,
                                             const char *param_name,
                                             const char *actual_type);

static int out_types_match(const cum_out_param *o, const char *type_name)
{
    if (!o->type_name && !type_name)
        return 1;
    return o->type_name && type_name && 0 == strcmp(o->type_name, type_name);
}

/* MockCheckedActualCall::withOutputParameter / checkOutputParameter.
 * type_name NULL = plain "const void*" output parameter. */
static void actual_with_output_parameter(cum_actual *a, const char *type_name,
                                         const char *name, void *dst)
{
    if (!a || a->state == CUM_CALL_FAILED)
        return;
    cum_scope *s = a->scope;

    /* record the destination */
    cum_out_dst *d = cu_xcalloc(1, sizeof *d);
    d->name = cu_xstrdup(name);
    d->type_name = type_name ? cu_xstrdup(type_name) : NULL;
    d->dst = dst;
    d->next = a->out_dsts;
    a->out_dsts = d;

    /* reopen + discard, like input parameters */
    a->state = CUM_CALL_IN_PROGRESS;
    if (a->matching) {
        reset_param_match_state(a->matching);
        a->matching = NULL;
    }
    for (cum_expectation *e = s->expectations; e; e = e->next)
        if (is_finalized_matching(e)) {
            e->candidate = 0;
            reset_param_match_state(e);
        }

    /* keep candidates with this output parameter; upstream consults only
     * the FIRST entry with the name (getValueByName) and matches its type
     * with compatibleForCopying */
    int any = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (!e->candidate)
            continue;
        int has = 0;
        cum_out_param *o = e->out_params;
        while (o && 0 != strcmp(o->name, name))
            o = o->next;
        if (o) {
            /* name found: type compatibility decides — ignoreOtherParameters
             * does NOT rescue a wrong-typed known name (upstream
             * hasOutputParameter ternary) */
            if (out_types_match(o, type_name))
                has = 1;
        } else {
            has = e->ignore_other_parameters;
        }
        if (has)
            any = 1;
        else
            e->candidate = 0; /* onlyKeep* discards do NOT reset the
                                 per-param marks (MockExpectedCallsList) —
                                 failure rendering shows them as matched */
    }
    if (!any) {
        a->state = CUM_CALL_FAILED;
        fail_unexpected_output_parameter(s, a->name, name,
                                         type_name ? type_name : "void*");
        return;
    }

    /* outputParameterWasPassed marks by NAME ONLY on surviving candidates —
     * upstream marks even same-named params of a DIFFERENT type (the type
     * check above only decides candidacy), so one actual output param can
     * satisfy several same-named expected ones (MockExpectedCall.cpp:360) */
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (!e->candidate)
            continue;
        for (cum_out_param *o = e->out_params; o; o = o->next)
            if (0 == strcmp(o->name, name))
                o->matched = 1;
    }

    complete_call_when_match_found(a);
}

void cum_actual_with_output_parameter(cum_actual *a, const char *name,
                                      void *dst)
{
    actual_with_output_parameter(a, NULL, name, dst);
}

void cum_actual_with_output_parameter_of_type(cum_actual *a,
                                              const char *type_name,
                                              const char *name, void *dst)
{
    actual_with_output_parameter(a, type_name, name, dst);
}

void cum_fail_no_way_to_compare(const char *type_name)
{
    msb b;
    msb_init(&b);
    msb_addf(&b,
             "MockFailure: No way to compare type <%s>. Please install a "
             "MockNamedValueComparator.",
             type_name);
    mock_fail(&b);
}

void cum_fail_no_way_to_copy(const char *type_name)
{
    msb b;
    msb_init(&b);
    msb_addf(&b,
             "MockFailure: No way to copy type <%s>. Please install a "
             "MockNamedValueCopier.",
             type_name);
    mock_fail(&b);
}

void cum_expectation_and_return(cum_expectation *e, cum_value value)
{
    if (!e)
        return;
    e->has_return = 1;
    e->return_value = value;
}

/* reading the return value finalizes the call, like upstream */
int cum_actual_return_value(cum_actual *a, cum_value *out)
{
    if (!a)
        return CUM_RET_IGNORED;
    actual_finalize(a);
    if (a->matching && a->matching->has_return) {
        *out = a->matching->return_value;
        return CUM_RET_VALUE;
    }
    return a->matching ? CUM_RET_NONE : CUM_RET_UNMATCHED;
}

int cum_scope_return_value(cum_scope *s, cum_value *out)
{
    return s->last_actual ? cum_actual_return_value(s->last_actual, out)
                          : CUM_RET_IGNORED;
}

void cum_expectation_with_parameter(cum_expectation *e, const char *name,
                                    cum_value value)
{
    if (!e)
        return;
    cum_param *p = cu_xcalloc(1, sizeof *p);
    p->name = cu_xstrdup(name);
    if (value.type == CUM_T_OBJECT || value.type == CUM_T_CONST_OBJECT) {
        p->owned_type =
            cu_xstrdup(value.v.obj.type_name ? value.v.obj.type_name : "");
        value.v.obj.type_name = p->owned_type;
        /* bind-at-creation: a later (re)install does not retroactively
         * change this param's comparator (upstream setObjectPointer) */
        p->bound = bind_from(bind_scope ? bind_scope->comparators : NULL,
                             p->owned_type);
    }
    p->value = value;
    cum_param **link = &e->params;
    while (*link)
        link = &(*link)->next;
    *link = p;
}

static void reset_param_match_state(cum_expectation *e)
{
    e->passed_to_object = e->object_specific ? 0 : 1;
    for (cum_param *p = e->params; p; p = p->next)
        p->matched = 0;
    for (cum_out_param *o = e->out_params; o; o = o->next)
        o->matched = 0;
}

static int all_params_matched(const cum_expectation *e)
{
    for (cum_param *p = e->params; p; p = p->next)
        if (!p->matched)
            return 0;
    for (cum_out_param *o = e->out_params; o; o = o->next)
        if (!o->matched)
            return 0;
    return 1;
}

/* isMatchingActualCall: parameters all matched AND (when onObject was used)
 * the call was passed to the expected object */
static int is_matching(const cum_expectation *e)
{
    return e->candidate && all_params_matched(e) && e->passed_to_object;
}

/* isMatchingActualCallAndFinalized: ignore-others expectations only finalize
 * explicitly (at call end) */
static int is_finalized_matching(const cum_expectation *e)
{
    return is_matching(e) && !e->ignore_other_parameters;
}

/* hasInputParameter: upstream uses getValueByName — only the FIRST
 * parameter with the name is consulted, even when duplicates exist */
static int expectation_accepts_param(const cum_expectation *e, const char *name,
                                     const cum_value *value)
{
    for (cum_param *p = e->params; p; p = p->next)
        if (0 == strcmp(p->name, name))
            return value_equals(&p->value, value, &p->bound);
    return e->ignore_other_parameters;
}

void cum_expectation_set_user(cum_expectation *e, void *user)
{
    if (e)
        e->user = user;
}

void *cum_expectation_user(cum_expectation *e)
{
    return e ? e->user : NULL;
}

void cum_set_facade_free(void (*fn)(void *))
{
    facade_free = fn;
}

/* MockCheckedExpectedCall::callWasMade */
static void expectation_call_was_made(cum_expectation *e, unsigned call_order)
{
    e->actual_calls++;
    if (e->order_start != CUM_NO_CALL_ORDER &&
        (call_order < e->order_start || call_order > e->order_end))
        e->out_of_order = 1;
    /* upstream callWasMade resets the match state (MockExpectedCall.cpp:325)
     * so a partially-fulfilled expectNCalls can match the next call; this is
     * the ONLY reset besides completion-on-candidates and reopen-discard —
     * onlyKeep* discards and candidacy entry never reset */
    reset_param_match_state(e);
}

static void clear_candidates(cum_scope *s)
{
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (e->candidate) {
            e->candidate = 0;
            reset_param_match_state(e);
        }
    }
}

/* completeCallWhenMatchIsFound: a finalized (non-ignoring) full match wins
 * immediately; ignore-others matches wait for call finalization. */
static void copy_output_parameters(cum_actual *a, cum_expectation *e)
{
    for (cum_out_dst *d = a->out_dsts; d; d = d->next) {
        /* upstream getOutputParameter(name): first-by-name lookup */
        cum_out_param *o = e->out_params;
        while (o && 0 != strcmp(o->name, d->name))
            o = o->next;
        if (!o || !out_types_match(o, d->type_name))
            continue;
        if (o->type_name) {
            /* creation-time copier binding (upstream reads the copier off
             * the expected MockNamedValue); a typed param with NO bound
             * copier fails HERE, at copy time, like upstream's
             * MockNoWayToCopyCustomTypeFailure in copyOutputParameters */
            if (!o->bound.present || !o->bound.copy) {
                a->state = CUM_CALL_FAILED;
                cum_fail_no_way_to_copy(o->type_name);
                return;
            }
            if (o->value)
                o->bound.copy(o->bound.ctx, d->dst, o->value);
        } else {
            /* upstream's plain copy path reads the source via
             * getConstPointerValue(), whose type assert counts one check —
             * including for unmodified (NULL source, size 0) parameters */
            cu_count_check();
            if (o->value)
                memcpy(d->dst, o->value, o->size);
        }
    }
}

static void complete_call_when_match_found(cum_actual *a)
{
    for (cum_expectation *e = a->scope->expectations; e; e = e->next) {
        if (is_finalized_matching(e)) {
            e->candidate = 0; /* removed from candidacy (claimed) */
            a->matching = e;
            a->state = CUM_CALL_SUCCEED;
            copy_output_parameters(a, e);
            return;
        }
    }
    /* an ignore-others candidate that currently matches still gets its
     * output parameters copied (upstream getFirstMatchingExpectation) */
    for (cum_expectation *e = a->scope->expectations; e; e = e->next) {
        if (is_matching(e)) {
            copy_output_parameters(a, e);
            return;
        }
    }
}

void cum_expectation_on_object(cum_expectation *e, void *object_ptr)
{
    if (!e)
        return; /* disabled mock */
    e->object_specific = 1;
    e->passed_to_object = 0;
    e->object_ptr = object_ptr;
}

/* MockCheckedExpectedCall::withName — replaces the (already scoped) name */
void cum_expectation_set_name(cum_expectation *e, const char *name)
{
    if (!e)
        return;
    free(e->name);
    e->name = cu_xstrdup(name);
}

/* MockCheckedActualCall::withCallOrder is a no-op (upstream ignores it on
 * checked calls; it only mattered in tracing mode) */
void cum_actual_with_call_order(cum_actual *a, unsigned order)
{
    (void)a;
    (void)order;
}

/* MockCheckedActualCall::withName — rename and re-filter candidates */
void cum_actual_with_name(cum_actual *a, const char *name)
{
    if (!a)
        return;
    cum_scope *s = a->scope;
    free(a->name);
    a->name = cu_xstrdup(name);
    a->state = CUM_CALL_IN_PROGRESS;

    int any = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (!e->candidate)
            continue;
        if (0 == strcmp(e->name, name)) {
            any = 1;
        } else {
            e->candidate = 0; /* no mark reset: see onlyKeep* note */
        }
    }
    if (!any) {
        a->state = CUM_CALL_FAILED;
        fail_unexpected_call(s, name);
        return;
    }
    complete_call_when_match_found(a);
}

/* MockCheckedActualCall::onObject */
void cum_actual_on_object(cum_actual *a, const void *object_ptr)
{
    if (!a || a->state == CUM_CALL_FAILED)
        return;
    cum_scope *s = a->scope;

    /* keep candidates relating to this object; expectations without a
     * specific object ignore it */
    int any = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (!e->candidate)
            continue;
        if (!e->object_specific || e->object_ptr == object_ptr) {
            any = 1;
        } else {
            e->candidate = 0; /* no mark reset: see onlyKeep* note */
        }
    }
    if (a->state != CUM_CALL_SUCCEED && !any) {
        a->state = CUM_CALL_FAILED;
        fail_unexpected_object(s, a->name, object_ptr);
        return;
    }

    for (cum_expectation *e = s->expectations; e; e = e->next)
        if (e->candidate)
            e->passed_to_object = 1;

    if (a->state != CUM_CALL_SUCCEED)
        complete_call_when_match_found(a);
}

/* MockCheckedActualCall::checkExpectations — finalize the in-progress call */
static void actual_finalize(cum_actual *a)
{
    if (!a || a->expectations_checked)
        return;
    a->expectations_checked = 1;
    cum_scope *s = a->scope;

    if (a->state != CUM_CALL_IN_PROGRESS) {
        if (a->state == CUM_CALL_SUCCEED && a->matching)
            expectation_call_was_made(a->matching, a->call_order);
        clear_candidates(s);
        return;
    }

    /* Still in progress: an ignore-others (or zero-param trailing) match may
     * finalize now. NO output copy here — upstream's checkExpectations path
     * (removeFirstMatchingExpectation -> finalizeActualCallMatch) never
     * copies; the copy already ran in complete_call_when_match_found when
     * the last parameter arrived (re-copying also double-counts the
     * getConstPointerValue check). */
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (is_matching(e)) {
            e->candidate = 0;
            a->matching = e;
            a->state = CUM_CALL_SUCCEED;
            expectation_call_was_made(e, a->call_order);
            clear_candidates(s);
            return;
        }
    }

    /* no match: parameters missing on some candidate, or (params all
     * matched) the call was never passed to the expected object */
    a->state = CUM_CALL_FAILED;
    {
        int params_missing = 0;
        for (cum_expectation *e = s->expectations; e; e = e->next)
            if (e->candidate && !all_params_matched(e))
                params_missing = 1;
        if (params_missing)
            fail_expected_parameter_didnt_happen(s, a->name); /* longjmps */
        else
            fail_expected_object_didnt_happen(s, a->name);
    }
    clear_candidates(s);
}

static void scope_free_last_actual(cum_scope *s)
{
    if (s->last_actual) {
        cum_out_dst *d = s->last_actual->out_dsts;
        while (d) {
            cum_out_dst *next = d->next;
            free(d->name);
            free(d->type_name);
            free(d);
            d = next;
        }
        free(s->last_actual->name);
        free(s->last_actual);
        s->last_actual = NULL;
    }
}

static void scope_finalize_last_actual(cum_scope *s)
{
    if (s->last_actual) {
        actual_finalize(s->last_actual);
        scope_free_last_actual(s);
    }
}

cum_actual *cum_actual_call(cum_scope *s, const char *name)
{
    scope_finalize_last_actual(s);

    if (!s->enabled)
        return NULL;

    char *full_name = scoped_name(s, name);

    /* callIsIgnored: ignoreOtherCalls and no expectation related to name */
    if (s->ignore_other_calls) {
        int related = 0;
        for (cum_expectation *e = s->expectations; e; e = e->next)
            if (0 == strcmp(e->name, full_name)) {
                related = 1;
                break;
            }
        if (!related) {
            free(full_name);
            return NULL;
        }
    }

    cum_actual *a = cu_xcalloc(1, sizeof *a);
    a->name = full_name;
    a->call_order = ++s->actual_call_order;
    a->state = CUM_CALL_IN_PROGRESS;
    a->scope = s;
    s->last_actual = a;

    /* Potentially matching = unfulfilled expectations with this name.
     * NO mark reset on entry: upstream's addPotentiallyMatchingExpectations
     * keeps whatever per-param marks an expectation carried from an earlier
     * call it was discarded from (only completion resets, and only for the
     * expectations still in candidacy then) — quirk preserved. */
    clear_candidates(s);
    int any = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (0 == strcmp(e->name, full_name) && !is_fulfilled(e)) {
            e->candidate = 1;
            any = 1;
        }
    }
    if (!any) {
        a->state = CUM_CALL_FAILED;
        fail_unexpected_call(s, full_name); /* longjmps when in a test */
        return a;
    }

    complete_call_when_match_found(a);
    return a;
}

/* MockCheckedActualCall::checkInputParameter */
void cum_actual_with_parameter(cum_actual *a, const char *name, cum_value value)
{
    if (!a || a->state == CUM_CALL_FAILED)
        return;
    cum_scope *s = a->scope;

    /* re-open: discardCurrentlyMatchingExpectations — the previously claimed
     * expectation does NOT rejoin candidacy; finalized-matching candidates
     * are dropped (their state reset) */
    a->state = CUM_CALL_IN_PROGRESS;
    if (a->matching) {
        reset_param_match_state(a->matching);
        a->matching = NULL;
    }
    for (cum_expectation *e = s->expectations; e; e = e->next)
        if (is_finalized_matching(e)) {
            e->candidate = 0;
            reset_param_match_state(e);
        }

    /* keep only candidates accepting this parameter */
    int any = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (!e->candidate)
            continue;
        if (expectation_accepts_param(e, name, &value))
            any = 1;
        else {
            e->candidate = 0; /* no mark reset: see onlyKeep* note */
        }
    }
    if (!any) {
        a->state = CUM_CALL_FAILED;
        fail_unexpected_input_parameter(s, a->name, name, &value);
        return;
    }

    /* parameterWasPassed on all remaining candidates */
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (!e->candidate)
            continue;
        for (cum_param *p = e->params; p; p = p->next)
            if (0 == strcmp(p->name, name))
                p->matched = 1;
    }

    complete_call_when_match_found(a);
}

static int scope_has_unfulfilled(const cum_scope *s)
{
    for (cum_expectation *e = s->expectations; e; e = e->next)
        if (!is_fulfilled(e))
            return 1;
    return 0;
}

static int scope_has_out_of_order(const cum_scope *s)
{
    for (cum_expectation *e = s->expectations; e; e = e->next)
        if (e->out_of_order)
            return 1;
    return 0;
}

int cum_expected_calls_left_all(void)
{
    /* Upstream recurses through EVERY child scope (finalizing each scope's
     * pending actual call) before summing — no early exit. Finalize WITHOUT
     * freeing: upstream's checkExpectationsOfLastActualCall keeps
     * lastActualFunctionCall_ alive until the next actualCall or clear, so
     * cached MockActualCall handles (and return getters reading the last
     * call) stay valid — freeing here was a fuzz-caught use-after-free. */
    int left = 0;
    for (cum_scope *s = scopes; s; s = s->next) {
        actual_finalize(s->last_actual);
        if (scope_has_unfulfilled(s))
            left = 1;
    }
    return left;
}

void cum_check_expectations_all(void)
{
    int last_call_failed = 0;
    int unfulfilled = 0;
    int out_of_order = 0;

    for (cum_scope *s = scopes; s; s = s->next) {
        if (s->last_actual && s->last_actual->state == CUM_CALL_FAILED)
            last_call_failed = 1;
        /* finalize, don't free: see cum_expected_calls_left_all */
        actual_finalize(s->last_actual);
        if (scope_has_unfulfilled(s))
            unfulfilled = 1;
        if (scope_has_out_of_order(s))
            out_of_order = 1;
    }

    /* wasLastActualCallFulfilled(): don't double-report after a failed call */
    if (!last_call_failed && unfulfilled)
        fail_expected_calls_didnt_happen(NULL);
    if (out_of_order)
        fail_out_of_order_calls(NULL);
}

/* upstream MockSupport::checkExpectations / expectedCallsLeft on a CHILD
 * scope examine that scope only (children live in the global mock's data
 * store; named scopes have none) — the _all variants are the global mock */
void cum_check_expectations_scope(cum_scope *s)
{
    int last_call_failed =
        s->last_actual && s->last_actual->state == CUM_CALL_FAILED;
    /* finalize, don't free: see cum_expected_calls_left_all */
    actual_finalize(s->last_actual);

    if (!last_call_failed && scope_has_unfulfilled(s))
        fail_expected_calls_didnt_happen(s);
    if (scope_has_out_of_order(s))
        fail_out_of_order_calls(s);
}

int cum_expected_calls_left_scope(cum_scope *s)
{
    /* finalize, don't free: see cum_expected_calls_left_all */
    actual_finalize(s->last_actual);
    return scope_has_unfulfilled(s);
}

static void scope_clear(cum_scope *s)
{
    /* upstream clear() DELETES the pending actual call without checking it
     * (no finalization, no missing-parameters failure) */
    scope_free_last_actual(s);
    cum_expectation *e = s->expectations;
    while (e) {
        cum_expectation *next = e->next;
        if (e->user && facade_free)
            facade_free(e->user);
        cum_param *p = e->params;
        while (p) {
            cum_param *pnext = p->next;
            free(p->name);
            free(p->owned_type);
            free(p);
            p = pnext;
        }
        cum_out_param *o = e->out_params;
        while (o) {
            cum_out_param *onext = o->next;
            free(o->name);
            free(o->type_name);
            free(o);
            o = onext;
        }
        free(e->name);
        free(e);
        e = next;
    }
    s->expectations = NULL;
    s->ignore_other_calls = 0;
    s->enabled = 1;
    s->strict_ordering = 0;
    s->actual_call_order = 0;
    s->expected_call_order = 0;
}

void cum_clear_all(void)
{
    for (cum_scope *s = scopes; s; s = s->next) {
        scope_clear(s);
        /* the global clear deletes child scopes upstream — including their
         * repositories (the GLOBAL repository survives clear) */
        if (s->name[0] != '\0') {
            s->zombie = 1;
            free_comparator_list(&s->comparators);
            free_comparator_list(&s->copiers);
            if (bind_scope == s)
                bind_scope = NULL;
        }
    }
    crash_on_failure = 0;
}

void cum_clear_scope(cum_scope *s)
{
    /* upstream MockSupport::clear on a CHILD scope: clears its own state
     * only; the child stays registered in the parent (no deletion) */
    scope_clear(s);
}

const char *cum_scope_name(const cum_scope *s)
{
    return s->name;
}

/* upstream propagates ignoreOtherCalls/disable/enable from the global mock
 * to all existing child scopes (MockSupport.cpp:218-240); named scopes have
 * no children, so they only affect themselves */
static int scope_is_global(const cum_scope *s)
{
    return s->name[0] == '\0';
}

void cum_ignore_other_calls(cum_scope *s)
{
    s->ignore_other_calls = 1;
    if (scope_is_global(s))
        for (cum_scope *c = scopes; c; c = c->next)
            c->ignore_other_calls = 1;
}

void cum_enable(cum_scope *s, int enabled)
{
    s->enabled = enabled;
    if (scope_is_global(s))
        for (cum_scope *c = scopes; c; c = c->next)
            c->enabled = enabled;
}

int cum_is_enabled(const cum_scope *s)
{
    return s->enabled;
}

void cum_crash_on_failure(int crash)
{
    crash_on_failure = crash;
}
