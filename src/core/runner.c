#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
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
static int crash_on_fail;

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
    /* -f swaps the test terminator for a crashing one (UtestShell::
     * setCrashOnFail): the failure is reported first, then we crash so a
     * debugger can take over. PlatformSpecificAbort == abort. */
    if (crash_on_fail)
        abort();
    cu_longjmp_out();
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

static int test_should_run(const cu_args *a, const cu_test *t, cu_result *res)
{
    if (cu_filters_match(a->group_filters, t->group) &&
        cu_filters_match(a->name_filters, t->name))
        return 1;
    res->filtered_out_count++;
    return 0;
}

/* TestRegistry::runAllTests */
static void cu_run_all_tests(const cu_args *a, cu_result *res, cu_output *out)
{
    res->time_started = cu_time_in_millis();
    for (cu_test *t = cu_registry_tests(); t != NULL; t = t->next) {
        if (a->run_ignored)
            t->run_ignored = 1;
        res->test_count++;
        if (test_should_run(a, t, res)) {
            cu_out_test_started(out, t);
            res->current_test_time_started = cu_time_in_millis();
            current_test = t;
            cu_run_one_test(t, res);
            current_test = NULL;
            res->current_test_ms = cu_time_in_millis() - res->current_test_time_started;
            cu_out_test_ended(out, res);
        }
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

/* TestRegistry::listTestGroupNames / listTestGroupAndCaseNames: unique
 * entries in registry order, joined by single spaces, no trailing space or
 * newline. Dedup matches upstream's #token# substring trick in effect. */
static void list_unique(const cu_args *a, int with_names)
{
    size_t printed = 0;
    for (cu_test *t = cu_registry_tests(); t != NULL; t = t->next) {
        if (with_names) {
            cu_result dummy = {0};
            if (!test_should_run(a, t, &dummy))
                continue;
        }
        int seen = 0;
        for (cu_test *p = cu_registry_tests(); p != t && !seen; p = p->next) {
            if (with_names) {
                cu_result dummy = {0};
                if (!test_should_run(a, p, &dummy))
                    continue;
                seen = 0 == strcmp(p->group, t->group) && 0 == strcmp(p->name, t->name);
            } else {
                seen = 0 == strcmp(p->group, t->group);
            }
        }
        if (seen)
            continue;
        if (printed++)
            fputs(" ", stdout);
        if (with_names)
            printf("%s.%s", t->group, t->name);
        else
            fputs(t->group, stdout);
    }
}

static void list_locations(void)
{
    for (cu_test *t = cu_registry_tests(); t != NULL; t = t->next)
        printf("%s.%s.%s.%d\n", t->group, t->name, t->file, (int)t->line);
}

int cu_run_all(int argc, const char *const *argv)
{
    cu_args args;
    cu_output out;

    if (!cu_args_parse(&args, argc, argv)) {
        fputs(args.need_help ? cu_help_text() : cu_usage_text(), stdout);
        cu_args_free(&args);
        return 1;
    }

    memset(&out, 0, sizeof out);
    out.level = args.very_verbose ? CU_OUTPUT_VERY_VERBOSE
              : args.verbose ? CU_OUTPUT_VERBOSE : CU_OUTPUT_NORMAL;
    out.color = args.color;
    out.progress_indicator = ".";
    crash_on_fail = args.crash_on_fail;
    /* args.output_type junit/teamcity: file outputs land in Phase 3; console
     * fallback until then. args.run_separate_process (-p): Phase 8. */

    if (args.list_groups || args.list_names) {
        /* upstream checks -lg before -ln when both are given */
        list_unique(&args, args.list_groups ? 0 : 1);
        cu_args_free(&args);
        return 0;
    }
    if (args.list_locations) {
        list_locations();
        cu_args_free(&args);
        return 0;
    }

    if (args.reversing)
        cu_registry_reverse();

    if (args.shuffling)
        printf("Test order shuffling enabled with seed: %lu\n",
               (unsigned long)args.shuffle_seed);

    size_t failed_test_count = 0;
    size_t failed_execution_count = 0;
    for (size_t loop = 1; loop <= args.repeat; loop++) {
        if (args.shuffling)
            cu_registry_shuffle(args.shuffle_seed);

        /* TestOutput::printTestRun */
        if (args.repeat > 1)
            printf("Test run %lu of %lu\n",
                   (unsigned long)loop, (unsigned long)args.repeat);

        cu_result res;
        memset(&res, 0, sizeof res);
        current_result = &res;
        current_output = &out;
        cu_run_all_tests(&args, &res, &out);
        current_result = NULL;
        current_output = NULL;

        failed_test_count += res.failure_count;
        if (cu_is_failure(&res))
            failed_execution_count++;
    }

    cu_args_free(&args);
    return (int)(failed_test_count != 0 ? failed_test_count : failed_execution_count);
}
