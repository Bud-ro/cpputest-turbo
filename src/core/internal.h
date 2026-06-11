#ifndef CPPUTEST_CORE_INTERNAL_H
#define CPPUTEST_CORE_INTERNAL_H

#include "cpputest_core/core.h"

typedef enum { CU_OUTPUT_NORMAL, CU_OUTPUT_VERBOSE } cu_output_level;

typedef struct cu_result {
    size_t test_count;
    size_t run_count;
    size_t check_count;
    size_t failure_count;
    size_t filtered_out_count;
    size_t ignored_count;
    size_t total_ms;
    size_t time_started;
    size_t current_test_time_started;
    size_t current_test_ms;
    size_t current_group_time_started;
    size_t current_group_ms;
} cu_result;

typedef struct cu_output {
    cu_output_level level;
    int suppress_summary; /* parallel workers: parent prints the merged one */
    size_t dot_count;
    const char *progress_indicator; /* "." or "!" for the test in flight */
} cu_output;

typedef struct cu_filter {
    char *text;
    int strict;
    int invert;
    struct cu_filter *next;
} cu_filter;

typedef struct cu_args {
    int parallel_workers; /* -jN extension (0/1 = sequential) */
    int need_help;
    int verbose;
    int run_ignored;
    cu_filter *group_filters;
    cu_filter *name_filters;
} cu_args;

/* args.c — mirrors upstream CommandLineArguments.cpp exactly, including the
 * else-if dispatch order and the usage/help texts. Returns 1 on success. */
int cu_args_parse(cu_args *a, int argc, const char *const *argv);
void cu_args_free(cu_args *a);
const char *cu_usage_text(void);
const char *cu_help_text(void);

/* filter matching (TestFilter.cpp): no filters in a dimension -> match;
 * otherwise OR over filters; strict = equality, default = substring,
 * invert = negate. */
int cu_filters_match(const cu_filter *filters, const char *s);

/* runner.c */
cu_result *cu_result_current(void);
cu_output *cu_output_current(void);
extern cu_plugin_parse_fn cu_plugin_parse_hook;

/* fork.c — parallel workers (-jN) */
int cu_run_parallel(const cu_args *a, cu_output *out, cu_result *total);

/* runner.c internals shared with fork.c */
void cu_run_test_actions(cu_test *t);
void cu_run_all_tests_internal(const cu_args *a, cu_result *res,
                               cu_output *out);
void cu_set_current_result_output(cu_result *res, cu_output *out);

/* output.c — console output, formats byte-identical to upstream
 * TestOutput.cpp */
void cu_out_group_started(cu_output *out, const cu_test *t);
void cu_out_group_ended(cu_output *out, const cu_result *res);
void cu_out_test_started(cu_output *out, const cu_test *t);
void cu_out_test_ended(cu_output *out, const cu_result *res);
void cu_out_failure(cu_output *out, const cu_test *t, const char *fail_file,
                    size_t fail_line, const char *message);
void cu_out_summary(cu_output *out, const cu_result *res);
/* TestOutput::print(const char*) / print(number) */
void cu_out_print_str(cu_output *out, const char *s);
void cu_out_print_num(cu_output *out, unsigned long n);

/* platform.c */
size_t cu_time_in_millis(void);

#endif
