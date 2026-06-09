#include "internal.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

/* Mirrors upstream's PlatformSpecificSetJmp stack (UtestPlatform.cpp):
 * setup, body and teardown each run in their own protected section; an
 * assertion failure longjmps out of the innermost one only. */
#define CU_JMP_DEPTH 10

static jmp_buf jmp_stack[CU_JMP_DEPTH];
static int jmp_index;

static cu_test *current_test;
static cu_result *current_result;
static cu_output *current_output;

cu_test *cu_current_test(void)
{
    return current_test;
}

cu_result *cu_result_current(void)
{
    return current_result;
}

cu_output *cu_output_current(void)
{
    return current_output;
}

/* Returns 1 if fn completed, 0 if it longjmp'd out. */
static int cu_protected_call(void (*fn)(cu_test *, void *), cu_test *t, void *fixture)
{
    if (0 == setjmp(jmp_stack[jmp_index])) {
        jmp_index++;
        fn(t, fixture);
        jmp_index--;
        return 1;
    }
    return 0;
}

static void cu_longjmp_out(void)
{
    if (jmp_index > 0) {
        jmp_index--;
        longjmp(jmp_stack[jmp_index], 1);
    }
}

void cu_count_check(void)
{
    if (current_result)
        current_result->check_count++;
}

void cu_fail_with_message(const char *file, size_t line, const char *message)
{
    if (current_result) {
        if (current_test)
            current_test->has_failed = 1;
        cu_out_failure(current_output, current_test, file, line, message);
        current_result->failure_count++;
    }
    cu_longjmp_out();
}

/* CheckFailure (TestFailure.cpp): createUserText(text) + check "(" cond ") failed".
 * createUserText: empty text -> nothing; else "Message: " text "\n\t". */
void cu_fail_check(const char *file, size_t line, const char *check_string,
                   const char *condition_string, const char *text)
{
    static char buf[4096];
    if (text && text[0])
        snprintf(buf, sizeof buf, "Message: %s\n\t%s(%s) failed",
                 text, check_string, condition_string);
    else
        snprintf(buf, sizeof buf, "%s(%s) failed", check_string, condition_string);
    cu_fail_with_message(file, line, buf);
}

/* FailFailure: the text verbatim, no "Message: " wrapper. */
void cu_fail_text(const char *file, size_t line, const char *text)
{
    cu_fail_with_message(file, line, text ? text : "");
}

void cu_exit_current_test(void)
{
    cu_longjmp_out();
}

static void do_setup(cu_test *t, void *fixture)    { t->ops->setup(t, fixture); }
static void do_body(cu_test *t, void *fixture)     { t->ops->body(t, fixture); }
static void do_teardown(cu_test *t, void *fixture) { t->ops->teardown(t, fixture); }

/* Upstream Utest::run() in no-exceptions mode: protected setup; body only if
 * setup completed; teardown always (Utest.cpp). */
static void cu_run_fixture(cu_test *t)
{
    void *fixture = t->ops->create(t);
    if (cu_protected_call(do_setup, t, fixture))
        cu_protected_call(do_body, t, fixture);
    cu_protected_call(do_teardown, t, fixture);
    t->ops->destroy(t, fixture);
}

/* UtestShell::runOneTest / IgnoredUtestShell::runOneTest */
static void cu_run_one_test(cu_test *t, cu_result *res)
{
    if (t->is_ignored && !t->run_ignored) {
        res->ignored_count++;
        return;
    }
    t->has_failed = 0;
    res->run_count++;
    cu_run_fixture(t);
}

static int group_changes(const cu_test *t)
{
    return t->next == NULL || 0 != strcmp(t->group, t->next->group);
}

/* TestRegistry::runAllTests */
static void cu_run_all_tests(cu_result *res, cu_output *out)
{
    res->time_started = cu_time_in_millis();
    for (cu_test *t = cu_registry_tests(); t != NULL; t = t->next) {
        res->test_count++;
        /* filters arrive in Phase 3; everything runs for now */
        cu_out_test_started(out, t);
        res->current_test_time_started = cu_time_in_millis();
        current_test = t;
        cu_run_one_test(t, res);
        current_test = NULL;
        res->current_test_ms = cu_time_in_millis() - res->current_test_time_started;
        cu_out_test_ended(out, res);
        (void)group_changes(t); /* group hooks arrive with JUnit output (Phase 3) */
    }
    res->total_ms = cu_time_in_millis() - res->time_started;
    cu_out_summary(out, res);
}

/* TestResult::isFailure */
static int cu_is_failure(const cu_result *res)
{
    return res->failure_count != 0 || (res->run_count + res->ignored_count) == 0;
}

int cu_run_all(int argc, const char *const *argv)
{
    cu_result res;
    cu_output out;

    memset(&res, 0, sizeof res);
    memset(&out, 0, sizeof out);
    out.level = CU_OUTPUT_NORMAL;
    out.progress_indicator = ".";

    /* Minimal arg handling until Phase 1.3 lands the real parser. */
    for (int i = 1; i < argc; i++) {
        if (0 == strcmp(argv[i], "-v"))
            out.level = CU_OUTPUT_VERBOSE;
        else if (0 == strcmp(argv[i], "-c"))
            out.color = 1;
    }

    current_result = &res;
    current_output = &out;
    cu_run_all_tests(&res, &out);
    current_result = NULL;
    current_output = NULL;

    /* CommandLineTestRunner::runAllTests: exit code is the failure count, or
     * the number of failed runs when nothing failed but nothing ran. */
    if (res.failure_count != 0)
        return (int)res.failure_count;
    return cu_is_failure(&res) ? 1 : 0;
}
