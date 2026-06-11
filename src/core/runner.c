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
size_t *cu_check_count_ptr; /* fast-path counter; see core.h */

static void bind_result(cu_result *res)
{
    current_result = res;
    cu_check_count_ptr = res ? &res->check_count : NULL;
}

static cu_plugin_action_fn plugin_pre;
static cu_plugin_action_fn plugin_post;
cu_plugin_parse_fn cu_plugin_parse_hook; /* read by args.c */

void cu_set_current_result_output(cu_result *res, cu_output *out)
{
    bind_result(res);
    current_output = out;
}

void cu_set_plugin_hooks(cu_plugin_action_fn pre, cu_plugin_action_fn post,
                         cu_plugin_parse_fn parse)
{
    plugin_pre = pre;
    plugin_post = post;
    cu_plugin_parse_hook = parse;
}

void cu_add_failure(const char *file, size_t line, const char *message)
{
    if (current_result) {
        if (current_test)
            current_test->has_failed = 1;
        cu_out_failure(current_output, current_test, file, line, message);
        current_result->failure_count++;
    }
}

size_t cu_failure_count(void)
{
    return current_result ? current_result->failure_count : 0;
}

void cu_print_text(const char *text)
{
    if (current_output)
        cu_out_print_str(current_output, text);
}

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
static int cu_protected_call(void (*fn)(cu_test *, void *), cu_test *t,
                             void *fixture)
{
    /* nested fixture runs (TestTestingFixture inside a test) share this
     * stack; ~10 levels deep means runaway recursion — fail loudly instead
     * of overflowing the jmp_buf array */
    if (jmp_index >= CU_JMP_DEPTH) {
        fprintf(stderr,
                "cpputest-turbo: protected-section depth exceeded (%d) — "
                "runaway nested test runs\n",
                CU_JMP_DEPTH);
        abort();
    }
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
    cu_fast_count_check();
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

void cu_exit_current_test(void)
{
    cu_longjmp_out();
}

static void do_setup(cu_test *t, void *fixture)
{
    t->ops->setup(t, fixture);
}
static void do_body(cu_test *t, void *fixture)
{
    t->ops->body(t, fixture);
}
static void do_teardown(cu_test *t, void *fixture)
{
    t->ops->teardown(t, fixture);
}

/* Upstream Utest::run() in no-exceptions mode: protected setup; body only
 * if setup completed; teardown always (Utest.cpp). */
static void cu_run_fixture(cu_test *t)
{
    void *fixture = t->ops->create(t);

    int setup_ok = cu_protected_call(do_setup, t, fixture);
    if (setup_ok)
        cu_protected_call(do_body, t, fixture);
    cu_protected_call(do_teardown, t, fixture);

    t->ops->destroy(t, fixture);
}

/* the pre/fixture/post sequence (UtestShell::runOneTestInCurrentProcess);
 * also runs inside forked -jN workers */
void cu_run_test_actions(cu_test *t)
{
    if (plugin_pre)
        plugin_pre(t);
    cu_run_fixture(t);
    if (plugin_post)
        plugin_post(t);
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
    cu_run_test_actions(t);
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
void cu_run_all_tests_internal(const cu_args *a, cu_result *res, cu_output *out)
{
    int group_start = 1;

    res->time_started = cu_time_in_millis();
    for (cu_test *t = cu_registry_tests(); t != NULL; t = t->next) {
        if (a->run_ignored)
            t->run_ignored = 1;
        if (group_start) {
            cu_out_group_started(out, t);
            res->current_group_time_started = cu_time_in_millis();
            group_start = 0;
        }
        res->test_count++;
        if (test_should_run(a, t, res)) {
            cu_out_test_started(out, t);
            res->current_test_time_started = cu_time_in_millis();
            current_test = t;
            cu_run_one_test(t, res);
            current_test = NULL;
            res->current_test_ms =
                cu_time_in_millis() - res->current_test_time_started;
            cu_out_test_ended(out, res);
        }
        if (group_changes(t)) {
            group_start = 1;
            res->current_group_ms =
                cu_time_in_millis() - res->current_group_time_started;
            cu_out_group_ended(out, res);
        }
    }
    res->total_ms = cu_time_in_millis() - res->time_started;
    cu_out_summary(out, res);
}

/* TestResult::isFailure */
static int cu_is_failure(const cu_result *res)
{
    return res->failure_count != 0 ||
           (res->run_count + res->ignored_count) == 0;
}

/* TestTestingFixture-style run: no CLI, default console output, stats out.
 * Supports NESTED runs (a fixture running tests from inside a test): all
 * runner state is saved/restored, and plugin hooks are suppressed — the
 * upstream fixture uses a fresh registry with its own empty plugin chain. */
void cu_run_registered_tests(cu_run_stats *stats_out, int verbose)
{
    cu_run_registered_tests_ex(stats_out, verbose, 0);
}

void cu_run_registered_tests_ex(cu_run_stats *stats_out, int verbose,
                                int run_plugins)
{
    cu_args args;
    cu_output out;
    cu_result res;

    cu_test *saved_test = current_test;
    cu_result *saved_result = current_result;
    cu_output *saved_output = current_output;
    cu_plugin_action_fn saved_pre = plugin_pre;
    cu_plugin_action_fn saved_post = plugin_post;
    if (run_plugins == 0) {
        plugin_pre = NULL;
        plugin_post = NULL;
    }

    memset(&args, 0, sizeof args);
    memset(&out, 0, sizeof out);
    out.level = verbose ? CU_OUTPUT_VERBOSE : CU_OUTPUT_NORMAL;
    out.progress_indicator = ".";
    memset(&res, 0, sizeof res);

    bind_result(&res);
    current_output = &out;
    cu_run_all_tests_internal(&args, &res, &out);

    current_test = saved_test;
    bind_result(saved_result);
    current_output = saved_output;
    plugin_pre = saved_pre;
    plugin_post = saved_post;

    if (stats_out) {
        stats_out->test_count = res.test_count;
        stats_out->run_count = res.run_count;
        stats_out->check_count = res.check_count;
        stats_out->failure_count = res.failure_count;
        stats_out->ignored_count = res.ignored_count;
        stats_out->filtered_out_count = res.filtered_out_count;
    }
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
    out.level = args.verbose ? CU_OUTPUT_VERBOSE : CU_OUTPUT_NORMAL;
    out.progress_indicator = ".";

    cu_result res;
    memset(&res, 0, sizeof res);
    bind_result(&res);
    current_output = &out;
    if (args.parallel_workers > 1)
        cu_run_parallel(&args, &out, &res);
    else
        cu_run_all_tests_internal(&args, &res, &out);
    bind_result(NULL);
    current_output = NULL;

    size_t failed_test_count = res.failure_count;
    size_t failed_execution_count = cu_is_failure(&res) ? 1u : 0u;
    cu_args_free(&args);
    return (int)(failed_test_count != 0 ? failed_test_count
                                        : failed_execution_count);
}
