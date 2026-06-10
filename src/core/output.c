#include "internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* All console output funnels through a swappable sink so harnesses (e.g.
 * TestTestingFixture) can capture it. Default: stdout. */

static cu_output_sink_fn sink_fn;
static void *sink_arg;

void cu_set_output_sink(cu_output_sink_fn fn, void *arg)
{
    sink_fn = fn;
    sink_arg = arg;
}

static void emit_str(const char *s)
{
    if (sink_fn)
        sink_fn(s, sink_arg);
    else
        fputs(s, stdout);
}

static void emit(const char *format, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    emit_str(buf);
}

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
    emit("%s(%s, %s)", macro, t->group, t->name);
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
            emit("|%c", *s);
        else if (*s == '\r')
            emit_str("|r");
        else if (*s == '\n')
            emit_str("|n");
        else
            emit("%c", *s);
    }
}

/* ------------------------------ hooks ----------------------------------- */

void cu_out_group_started(cu_output *out, const cu_test *t)
{
    if (out->type == CU_OUTPUT_TYPE_TEAMCITY) {
        out->tc_group = t->group;
        emit_str("##teamcity[testSuiteStarted name='");
        tc_escaped(t->group);
        emit_str("']\n");
    }
}

void cu_out_group_ended(cu_output *out, const cu_result *res)
{
    if (out->type == CU_OUTPUT_TYPE_TEAMCITY) {
        if (out->tc_group == NULL || out->tc_group[0] == '\0')
            return;
        emit_str("##teamcity[testSuiteFinished name='");
        tc_escaped(out->tc_group);
        emit_str("']\n");
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
        emit_str("##teamcity[testStarted name='");
        tc_escaped(t->name);
        emit_str("']\n");
        if (!test_will_run(t)) {
            emit_str("##teamcity[testIgnored name='");
            tc_escaped(t->name);
            emit_str("']\n");
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
        emit("##teamcity[testFinished name='");
        tc_escaped(out->tc_current->name);
        emit("' duration='%lu']\n", (unsigned long)res->current_test_ms);
        return;
    }
    if (!console_active(out))
        return;
    if (out->level >= CU_OUTPUT_VERBOSE) {
        emit(" - %lu ms\n", (unsigned long)res->current_test_ms);
    } else {
        emit_str(out->progress_indicator);
        if (++out->dot_count % 50 == 0)
            emit_str("\n");
    }
}

/* "\n<file>:<line>:" + " error:" — Eclipse working environment */
static void print_error_location(const char *file, size_t line)
{
    emit("\n%s:%lu: error:", file, (unsigned long)line);
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
        emit(" Failure in ");
        if (t)
            print_formatted_name(t);
        print_error_location(fail_file, fail_line);
    } else {
        print_error_location(fail_file, fail_line);
        emit(" Failure in ");
        if (t)
            print_formatted_name(t);
    }
    emit("\n\t%s\n\n", message);
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

        emit_str("##teamcity[testFailed name='");
        tc_escaped(t ? t->name : "");
        emit_str("' message='");
        if (outside_file || in_helper)
            /* test file printed UNescaped, like upstream */
            emit("TEST failed (%s:%lu): ", test_file, (unsigned long)test_line);
        tc_escaped(fail_file);
        emit(":%lu' details='", (unsigned long)fail_line);
        tc_escaped(message);
        emit_str("']\n");
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
    if (out->suppress_summary)
        return;
    if (!console_active(out))
        return;
    int ran_nothing = result_is_failure(res) && res->failure_count == 0;

    emit_str("\n");
    if (result_is_failure(res)) {
        if (out->color)
            emit_str("\033[31;1m");
        if (ran_nothing)
            emit_str("Errors (ran nothing, ");
        else
            emit("Errors (%lu failures, ", (unsigned long)res->failure_count);
    } else {
        if (out->color)
            emit_str("\033[32;1m");
        emit_str("OK (");
    }
    emit("%lu tests, %lu ran, %lu checks, %lu ignored, %lu filtered out, %lu ms)",
           (unsigned long)res->test_count, (unsigned long)res->run_count,
           (unsigned long)res->check_count, (unsigned long)res->ignored_count,
           (unsigned long)res->filtered_out_count, (unsigned long)res->total_ms);
    if (out->color)
        emit_str("\033[m");
    if (ran_nothing)
        emit_str("\nNote: test run failed because no tests were run or ignored. Assuming something went wrong. This often happens because of linking errors or typos in test filter.");
    emit_str("\n\n");
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
    emit_str(s);
}

/* print(long)/print(size_t): upstream JUnit overrides these as no-ops, so
 * numbers vanish from JUnit system-out */
void cu_out_print_num(cu_output *out, unsigned long n)
{
    if (out->type == CU_OUTPUT_TYPE_JUNIT && !out->also_console)
        return;
    emit("%lu", n);
}

void cu_out_very_verbose(cu_output *out, const char *s)
{
    if (out->level != CU_OUTPUT_VERY_VERBOSE)
        return;
    /* JUnit's printBuffer is a no-op; console/TeamCity print */
    if (out->type == CU_OUTPUT_TYPE_JUNIT && !out->also_console)
        return;
    emit_str(s);
}
