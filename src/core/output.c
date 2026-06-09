#include "internal.h"

#include <stdio.h>
#include <string.h>

/* Output dispatch. Console formats are byte-identical to upstream
 * TestOutput.cpp; TeamCity to TeamCityTestOutput.cpp (which extends the
 * console output, so progress/summary still print); JUnit records silently
 * (its writer lives in junit.c) unless composed with console via -v. */

static int test_will_run(const cu_test *t)
{
    return !(t->is_ignored && !t->run_ignored);
}

/* UtestShell::getFormattedName(): "<macro>(<group>, <name>)" */
static void print_formatted_name(const cu_test *t)
{
    const char *macro = (t->is_ignored && !t->run_ignored) ? "IGNORE_TEST" : "TEST";
    printf("%s(%s, %s)", macro, t->group, t->name);
}

static int console_active(const cu_output *out)
{
    return out->type != CU_OUTPUT_TYPE_JUNIT || out->also_console;
}

/* TeamCityTestOutput::printEscaped */
static void tc_escaped(const char *s)
{
    for (; *s; s++) {
        if (*s == '\'' || *s == '|' || *s == '[' || *s == ']')
            printf("|%c", *s);
        else if (*s == '\r')
            fputs("|r", stdout);
        else if (*s == '\n')
            fputs("|n", stdout);
        else
            putchar(*s);
    }
}

/* ------------------------------ hooks ----------------------------------- */

void cu_out_group_started(cu_output *out, const cu_test *t)
{
    if (out->type == CU_OUTPUT_TYPE_TEAMCITY) {
        out->tc_group = t->group;
        fputs("##teamcity[testSuiteStarted name='", stdout);
        tc_escaped(t->group);
        fputs("']\n", stdout);
    }
}

void cu_out_group_ended(cu_output *out, const cu_result *res)
{
    if (out->type == CU_OUTPUT_TYPE_TEAMCITY) {
        if (out->tc_group == NULL || out->tc_group[0] == '\0')
            return;
        fputs("##teamcity[testSuiteFinished name='", stdout);
        tc_escaped(out->tc_group);
        fputs("']\n", stdout);
        return;
    }
    if (out->type == CU_OUTPUT_TYPE_JUNIT)
        cu_junit_group_ended(out, res);
}

void cu_out_test_started(cu_output *out, const cu_test *t)
{
    if (out->type == CU_OUTPUT_TYPE_JUNIT)
        cu_junit_test_started(out, t);
    if (out->type == CU_OUTPUT_TYPE_TEAMCITY) {
        fputs("##teamcity[testStarted name='", stdout);
        tc_escaped(t->name);
        fputs("']\n", stdout);
        if (!test_will_run(t)) {
            fputs("##teamcity[testIgnored name='", stdout);
            tc_escaped(t->name);
            fputs("']\n", stdout);
        }
        out->tc_current = t;
        return;
    }
    if (!console_active(out))
        return;
    out->progress_indicator = test_will_run(t) ? "." : "!";
    if (out->level >= CU_OUTPUT_VERBOSE)
        print_formatted_name(t);
}

void cu_out_test_ended(cu_output *out, const cu_result *res)
{
    if (out->type == CU_OUTPUT_TYPE_JUNIT)
        cu_junit_test_ended(out, res);
    if (out->type == CU_OUTPUT_TYPE_TEAMCITY) {
        if (!out->tc_current)
            return;
        printf("##teamcity[testFinished name='");
        tc_escaped(out->tc_current->name);
        printf("' duration='%lu']\n", (unsigned long)res->current_test_ms);
        return;
    }
    if (!console_active(out))
        return;
    if (out->level >= CU_OUTPUT_VERBOSE) {
        printf(" - %lu ms\n", (unsigned long)res->current_test_ms);
    } else {
        fputs(out->progress_indicator, stdout);
        if (++out->dot_count % 50 == 0)
            fputs("\n", stdout);
    }
}

/* "\n<file>:<line>:" + " error:" — Eclipse working environment */
static void print_error_location(const char *file, size_t line)
{
    printf("\n%s:%lu: error:", file, (unsigned long)line);
}

static void console_failure(const cu_test *t, const char *fail_file,
                            size_t fail_line, const char *message)
{
    const char *test_file = t ? t->file : "unknown file";
    size_t test_line = t ? t->line : 0;
    int outside_file = 0 != strcmp(fail_file, test_file);
    int in_helper = !outside_file && fail_line < test_line;

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

void cu_out_failure(cu_output *out, const cu_test *t,
                    const char *fail_file, size_t fail_line,
                    const char *message)
{
    if (out->type == CU_OUTPUT_TYPE_JUNIT) {
        cu_junit_failure(out, t, fail_file, fail_line, message);
        if (!out->also_console)
            return;
    }
    if (out->type == CU_OUTPUT_TYPE_TEAMCITY) {
        const char *test_file = t ? t->file : "unknown file";
        size_t test_line = t ? t->line : 0;
        int outside_file = 0 != strcmp(fail_file, test_file);
        int in_helper = !outside_file && fail_line < test_line;

        fputs("##teamcity[testFailed name='", stdout);
        tc_escaped(t ? t->name : "");
        fputs("' message='", stdout);
        if (outside_file || in_helper)
            /* test file printed UNescaped, like upstream */
            printf("TEST failed (%s:%lu): ", test_file, (unsigned long)test_line);
        tc_escaped(fail_file);
        printf(":%lu' details='", (unsigned long)fail_line);
        tc_escaped(message);
        fputs("']\n", stdout);
        return;
    }
    console_failure(t, fail_file, fail_line, message);
}

static int result_is_failure(const cu_result *res)
{
    return res->failure_count != 0 || (res->run_count + res->ignored_count) == 0;
}

void cu_out_summary(cu_output *out, const cu_result *res)
{
    if (!console_active(out))
        return;
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

/* TestOutput::print(const char*): JUnit accumulates into <system-out> (and
 * the console half of a composite still prints) */
void cu_out_print_str(cu_output *out, const char *s)
{
    if (out->type == CU_OUTPUT_TYPE_JUNIT) {
        cu_junit_print(out, s);
        if (!out->also_console)
            return;
    }
    fputs(s, stdout);
}

/* print(long)/print(size_t): upstream JUnit overrides these as no-ops, so
 * numbers vanish from JUnit system-out */
void cu_out_print_num(cu_output *out, unsigned long n)
{
    if (out->type == CU_OUTPUT_TYPE_JUNIT && !out->also_console)
        return;
    printf("%lu", n);
}

void cu_out_very_verbose(cu_output *out, const char *s)
{
    if (out->level != CU_OUTPUT_VERY_VERBOSE)
        return;
    /* JUnit's printBuffer is a no-op; console/TeamCity print */
    if (out->type == CU_OUTPUT_TYPE_JUNIT && !out->also_console)
        return;
    fputs(s, stdout);
}
