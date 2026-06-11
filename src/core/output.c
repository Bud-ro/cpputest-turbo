#include "internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* lite: console output only — formats are byte-identical to upstream
 * TestOutput.cpp. All output funnels through a swappable sink so harnesses
 * can capture it. Default: stdout. */

static cu_output_sink_fn sink_fn;
static void *sink_arg;

void cu_set_output_sink(cu_output_sink_fn fn, void *arg)
{
    sink_fn = fn;
    sink_arg = arg;
}

static void emit_str(const char *s)
{
    if (sink_fn) {
        sink_fn(s, sink_arg);
    } else {
        fputs(s, stdout);
        /* upstream ConsoleTestOutput::flush()es after every print; beyond
         * parity, an empty stdio buffer is what keeps fork() (-jN) from
         * duplicating buffered bytes into the children */
        fflush(stdout);
    }
}

CU_FORMAT_PRINTF(1, 2)
static void emit(const char *format, ...)
{
    /* mock failure histories and string diffs can exceed any fixed buffer;
     * spill to the heap instead of truncating (found by fuzzing) */
    char buf[1024];
    va_list args;
    va_list copy;
    va_start(args, format);
    va_copy(copy, args);
    int n = vsnprintf(buf, sizeof buf, format, copy);
    va_end(copy);
    if (n >= (int)sizeof buf) {
        char *heap = malloc((size_t)n + 1);
        if (heap) {
            vsnprintf(heap, (size_t)n + 1, format, args);
            emit_str(heap);
            free(heap);
        } else {
            emit_str(buf); /* out of memory: emit the truncated prefix */
        }
    } else {
        emit_str(buf);
    }
    va_end(args);
}

static int test_will_run(const cu_test *t)
{
    return !(t->is_ignored && !t->run_ignored);
}

/* UtestShell::getFormattedName(): "<macro>(<group>, <name>)" */
static void print_formatted_name(const cu_test *t)
{
    const char *macro =
        (t->is_ignored && !t->run_ignored) ? "IGNORE_TEST" : "TEST";
    emit("%s(%s, %s)", macro, t->group, t->name);
}

/* ------------------------------ hooks ----------------------------------- */

void cu_out_group_started(cu_output *out, const cu_test *t)
{
    (void)out;
    (void)t;
}

void cu_out_group_ended(cu_output *out, const cu_result *res)
{
    (void)out;
    (void)res;
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

void cu_out_failure(cu_output *out, const cu_test *t, const char *fail_file,
                    size_t fail_line, const char *message)
{
    (void)out;
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

static int result_is_failure(const cu_result *res)
{
    return res->failure_count != 0 ||
           (res->run_count + res->ignored_count) == 0;
}

void cu_out_summary(cu_output *out, const cu_result *res)
{
    /* upstream printTestsEnded resets dotCount_ */
    out->dot_count = 0;
    if (out->suppress_summary)
        return;
    int ran_nothing = result_is_failure(res) && res->failure_count == 0;

    emit_str("\n");
    if (result_is_failure(res)) {
        if (ran_nothing)
            emit_str("Errors (ran nothing, ");
        else
            emit("Errors (%lu failures, ", (unsigned long)res->failure_count);
    } else {
        emit_str("OK (");
    }
    emit("%lu tests, %lu ran, %lu checks, %lu ignored, %lu filtered out, %lu "
         "ms)",
         (unsigned long)res->test_count, (unsigned long)res->run_count,
         (unsigned long)res->check_count, (unsigned long)res->ignored_count,
         (unsigned long)res->filtered_out_count, (unsigned long)res->total_ms);
    if (ran_nothing)
        emit_str("\nNote: test run failed because no tests were run or "
                 "ignored. Assuming something went wrong. This often happens "
                 "because of linking errors or typos in test filter.");
    emit_str("\n\n");
}

/* TestOutput::print */
void cu_out_print_str(cu_output *out, const char *s)
{
    (void)out;
    emit_str(s);
}

void cu_out_print_num(cu_output *out, unsigned long n)
{
    (void)out;
    emit("%lu", n);
}
