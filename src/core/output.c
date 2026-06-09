#include "internal.h"

#include <stdio.h>
#include <string.h>

/* Console output with byte-identical formats to upstream TestOutput.cpp;
 * every literal below is quoted in docs/INTERFACE.md section 3. */

static int test_will_run(const cu_test *t)
{
    return !(t->is_ignored && !t->run_ignored);
}

/* UtestShell::getFormattedName(): "<macro>(<group>, <name>)"; macro is
 * IGNORE_TEST only when the test is ignored and not forced to run. */
static void print_formatted_name(const cu_test *t)
{
    const char *macro = test_will_run(t) && t->is_ignored ? "TEST"
                      : t->is_ignored ? "IGNORE_TEST" : "TEST";
    printf("%s(%s, %s)", macro, t->group, t->name);
}

void cu_out_test_started(cu_output *out, const cu_test *t)
{
    out->progress_indicator = test_will_run(t) ? "." : "!";
    if (out->level >= CU_OUTPUT_VERBOSE)
        print_formatted_name(t);
}

void cu_out_test_ended(cu_output *out, const cu_result *res)
{
    if (out->level >= CU_OUTPUT_VERBOSE) {
        printf(" - %lu ms\n", (unsigned long)res->current_test_ms);
    } else {
        fputs(out->progress_indicator, stdout);
        if (++out->dot_count % 50 == 0)
            fputs("\n", stdout);
    }
}

/* "\n<file>:<line>:" + " error:" — Eclipse working environment (default off
 * Windows). */
static void print_error_location(const char *file, size_t line)
{
    printf("\n%s:%lu: error:", file, (unsigned long)line);
}

void cu_out_failure(cu_output *out, const cu_test *t,
                    const char *fail_file, size_t fail_line,
                    const char *message)
{
    /* TestFailure: outside test file, or in a helper function (failure line
     * above the TEST() line in the same file) -> print both locations. */
    const char *test_file = t ? t->file : "unknown file";
    size_t test_line = t ? t->line : 0;
    int outside_file = 0 != strcmp(fail_file, test_file);
    int in_helper = !outside_file && fail_line < test_line;

    (void)out;
    if (outside_file || in_helper) {
        print_error_location(test_file, test_line);
        printf(" Failure in ");
        if (t)
            print_formatted_name(t);
        print_error_location(fail_file, fail_line);
    } else {
        print_error_location(fail_file, fail_line);
        printf(" Failure in ");
        if (t)
            print_formatted_name(t);
    }
    printf("\n\t%s\n\n", message);
}

static int result_is_failure(const cu_result *res)
{
    return res->failure_count != 0 || (res->run_count + res->ignored_count) == 0;
}

void cu_out_summary(cu_output *out, const cu_result *res)
{
    int ran_nothing = result_is_failure(res) && res->failure_count == 0;

    fputs("\n", stdout);
    if (result_is_failure(res)) {
        if (out->color)
            fputs("\033[31;1m", stdout);
        if (ran_nothing)
            fputs("Errors (ran nothing, ", stdout);
        else
            printf("Errors (%lu failures, ", (unsigned long)res->failure_count);
    } else {
        if (out->color)
            fputs("\033[32;1m", stdout);
        fputs("OK (", stdout);
    }
    printf("%lu tests, %lu ran, %lu checks, %lu ignored, %lu filtered out, %lu ms)",
           (unsigned long)res->test_count, (unsigned long)res->run_count,
           (unsigned long)res->check_count, (unsigned long)res->ignored_count,
           (unsigned long)res->filtered_out_count, (unsigned long)res->total_ms);
    if (out->color)
        fputs("\033[m", stdout);
    if (ran_nothing)
        fputs("\nNote: test run failed because no tests were run or ignored. Assuming something went wrong. This often happens because of linking errors or typos in test filter.", stdout);
    fputs("\n\n", stdout);
}
