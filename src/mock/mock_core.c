#define _POSIX_C_SOURCE 200809L

#include <cpputest_core/mock.h>
#include <cpputest_core/core.h>

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
    b->buf = malloc(b->cap);
    b->buf[0] = '\0';
}

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
            b->buf = realloc(b->buf, b->cap);
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

/* ------------------------------ data model ------------------------------ */

#define CUM_NO_CALL_ORDER 0

struct cum_expectation {
    char *name;
    unsigned expected_calls;
    unsigned actual_calls;
    unsigned order_start; /* CUM_NO_CALL_ORDER = unordered */
    unsigned order_end;
    int out_of_order;
    int ignore_other_parameters;
    int matched_by_in_progress; /* claimed by the in-progress actual call */
    void *user;                 /* C++ facade */
    struct cum_expectation *next;
};

typedef enum {
    CUM_CALL_IN_PROGRESS,
    CUM_CALL_SUCCEED,
    CUM_CALL_FAILED
} cum_call_state;

struct cum_actual {
    char *name;
    unsigned call_order;
    cum_call_state state;
    int expectations_checked;
    cum_expectation *matching;
    cum_scope *scope;
};

struct cum_scope {
    char *name;
    int enabled;
    int strict_ordering;
    int ignore_other_calls;
    unsigned actual_call_order;
    unsigned expected_call_order;
    cum_expectation *expectations; /* append order preserved */
    cum_actual *last_actual;
    struct cum_scope *next;
};

static cum_scope *scopes;
static int crash_on_failure;
static void (*facade_free)(void *);

/* ------------------------------- scopes --------------------------------- */

cum_scope *cum_scope_get(const char *name)
{
    if (!name)
        name = "";
    for (cum_scope *s = scopes; s; s = s->next)
        if (0 == strcmp(s->name, name))
            return s;
    cum_scope *s = calloc(1, sizeof *s);
    s->name = strdup(name);
    s->enabled = 1;
    s->next = scopes;
    scopes = s;
    return s;
}

static char *scoped_name(const cum_scope *s, const char *name)
{
    if (s->name[0])
        return cu_str_printf("%s::%s", s->name, name);
    return strdup(name);
}

/* --------------------------- failure plumbing --------------------------- */

/* MockFailureReporter::failTest: only fail if the test hasn't already
 * failed; MockFailure attributes to the test's own file/line. */
static void mock_fail(msb *message)
{
    cu_test *t = cu_current_test();
    char *msg = message->buf;
    if (t && !t->has_failed) {
        if (crash_on_failure) {
            free(msg);
            __builtin_trap();
        }
        static char copy[8192];
        snprintf(copy, sizeof copy, "%s", msg);
        free(msg);
        cu_fail_with_message(t->file, t->line, copy); /* longjmps */
    }
    free(msg);
}

/* callToString (subset: no parameters yet) */
static void msb_call_to_string(msb *b, const cum_expectation *e)
{
    msb_add(b, e->name);
    msb_add(b, " -> ");
    if (e->order_start != CUM_NO_CALL_ORDER) {
        if (e->order_start == e->order_end)
            msb_addf(b, "expected call order: <%u> -> ", e->order_start);
        else
            msb_addf(b, "expected calls order: <%u..%u> -> ",
                     e->order_start, e->order_end);
    }
    msb_add(b, e->ignore_other_parameters ? "all parameters ignored" : "no parameters");
    msb_addf(b, " (expected %u call%s, called %u time%s)",
             e->expected_calls, e->expected_calls == 1 ? "" : "s",
             e->actual_calls, e->actual_calls == 1 ? "" : "s");
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

/* named-filter predicates arrive with parameter failures (next slice) */

static int pred_unfulfilled_out_of_order(const cum_expectation *e, const char *arg)
{
    (void)arg;
    return !is_fulfilled(e) && e->out_of_order;
}

static int pred_fulfilled_out_of_order(const cum_expectation *e, const char *arg)
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
            if (ones == 3) suffix = "rd";
            else if (ones == 2) suffix = "nd";
            else if (ones == 1) suffix = "st";
        }
        msb_addf(&b, "Mock Failure: Unexpected additional (%u%s) call to function: ",
                 n, suffix);
    } else {
        msb_add(&b, "Mock Failure: Unexpected call to function: ");
    }
    msb_add(&b, name);
    msb_add(&b, "\n");
    msb_history(&b, s, pred_unfulfilled, pred_fulfilled, NULL);
    mock_fail(&b);
}

static void fail_expected_calls_didnt_happen(void)
{
    msb b;
    msb_init(&b);
    msb_add(&b, "Mock Failure: Expected call WAS NOT fulfilled.\n");
    msb_history(&b, NULL, pred_unfulfilled, pred_fulfilled, NULL);
    mock_fail(&b);
}

static void fail_out_of_order_calls(void)
{
    msb b;
    msb_init(&b);
    msb_add(&b, "Mock Failure: Out of order calls");
    msb_add(&b, "\n");
    msb_history(&b, NULL, pred_unfulfilled_out_of_order,
                pred_fulfilled_out_of_order, NULL);
    mock_fail(&b);
}

/* ------------------------------ lifecycle ------------------------------- */

void cum_strict_order(cum_scope *s)
{
    s->strict_ordering = 1;
}

cum_expectation *cum_expect_n_calls(cum_scope *s, unsigned amount, const char *name)
{
    if (!s->enabled)
        return NULL;
    cu_count_check();

    cum_expectation *e = calloc(1, sizeof *e);
    e->name = scoped_name(s, name);
    e->expected_calls = amount;
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

void cum_expectation_with_call_order(cum_expectation *e, unsigned start, unsigned end)
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
}

/* finalize the in-progress actual call (checkExpectations on the call) */
static void actual_finalize(cum_actual *a)
{
    if (!a || a->expectations_checked)
        return;
    a->expectations_checked = 1;
    if (a->state == CUM_CALL_SUCCEED && a->matching) {
        expectation_call_was_made(a->matching, a->call_order);
        a->matching->matched_by_in_progress = 0;
    }
    /* CALL_FAILED / parameterless IN_PROGRESS handled at creation time in
     * this slice (parameters arrive later) */
}

static void scope_finalize_last_actual(cum_scope *s)
{
    if (s->last_actual) {
        actual_finalize(s->last_actual);
        free(s->last_actual->name);
        free(s->last_actual);
        s->last_actual = NULL;
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

    cum_actual *a = calloc(1, sizeof *a);
    a->name = full_name;
    a->call_order = ++s->actual_call_order;
    a->state = CUM_CALL_IN_PROGRESS;
    a->scope = s;
    s->last_actual = a;

    /* potentially matching = unfulfilled expectations with this name */
    cum_expectation *first_match = NULL;
    int any_related = 0;
    for (cum_expectation *e = s->expectations; e; e = e->next) {
        if (0 != strcmp(e->name, full_name))
            continue;
        if (!is_fulfilled(e)) {
            any_related = 1;
            if (!first_match)
                first_match = e;
        }
    }
    (void)any_related;
    if (!first_match) {
        a->state = CUM_CALL_FAILED;
        fail_unexpected_call(s, full_name); /* longjmps when in a test */
        return a;
    }

    /* parameterless slice: a no-parameter expectation matches immediately */
    a->matching = first_match;
    first_match->matched_by_in_progress = 1;
    a->state = CUM_CALL_SUCCEED;
    return a;
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
    for (cum_scope *s = scopes; s; s = s->next) {
        scope_finalize_last_actual(s);
        if (scope_has_unfulfilled(s))
            return 1;
    }
    return 0;
}

void cum_check_expectations_all(void)
{
    int last_call_failed = 0;
    int unfulfilled = 0;
    int out_of_order = 0;

    for (cum_scope *s = scopes; s; s = s->next) {
        if (s->last_actual && s->last_actual->state == CUM_CALL_FAILED)
            last_call_failed = 1;
        scope_finalize_last_actual(s);
        if (scope_has_unfulfilled(s))
            unfulfilled = 1;
        if (scope_has_out_of_order(s))
            out_of_order = 1;
    }

    /* wasLastActualCallFulfilled(): don't double-report after a failed call */
    if (!last_call_failed && unfulfilled)
        fail_expected_calls_didnt_happen();
    if (out_of_order)
        fail_out_of_order_calls();
}

static void scope_clear(cum_scope *s)
{
    scope_finalize_last_actual(s);
    cum_expectation *e = s->expectations;
    while (e) {
        cum_expectation *next = e->next;
        if (e->user && facade_free)
            facade_free(e->user);
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
    for (cum_scope *s = scopes; s; s = s->next)
        scope_clear(s);
    crash_on_failure = 0;
}

void cum_ignore_other_calls(cum_scope *s)
{
    s->ignore_other_calls = 1;
}

void cum_enable(cum_scope *s, int enabled)
{
    s->enabled = enabled;
}

int cum_is_enabled(const cum_scope *s)
{
    return s->enabled;
}

void cum_crash_on_failure(int crash)
{
    crash_on_failure = crash;
}
