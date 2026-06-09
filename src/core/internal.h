#ifndef CPPUTEST_CORE_INTERNAL_H
#define CPPUTEST_CORE_INTERNAL_H

#include <cpputest_core/core.h>

typedef enum {
    CU_OUTPUT_NORMAL,
    CU_OUTPUT_VERBOSE,
    CU_OUTPUT_VERY_VERBOSE
} cu_output_level;

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
} cu_result;

typedef struct cu_output {
    cu_output_level level;
    int color;
    size_t dot_count;
    const char *progress_indicator; /* "." or "!" for the test in flight */
} cu_output;

typedef struct cu_filter {
    char *text;
    int strict;
    int invert;
    struct cu_filter *next;
} cu_filter;

typedef enum {
    CU_OUTPUT_TYPE_CONSOLE, /* upstream OUTPUT_ECLIPSE: -onormal / -oeclipse */
    CU_OUTPUT_TYPE_JUNIT,
    CU_OUTPUT_TYPE_TEAMCITY
} cu_output_type;

typedef struct cu_args {
    int need_help;
    int verbose;
    int very_verbose;
    int color;
    int run_separate_process;
    int reversing;
    int list_groups;
    int list_names;
    int list_locations;
    int run_ignored;
    int crash_on_fail;
    int rethrow_exceptions;
    int shuffling;
    int shuffling_preseeded;
    size_t repeat;
    unsigned shuffle_seed;
    cu_output_type output_type;
    char *package_name;
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

/* registry list reordering (UtestShellPointerArray) */
void cu_registry_reverse(void);
void cu_registry_shuffle(size_t seed);

/* output.c — console output, formats are byte-identical to upstream
 * TestOutput.cpp (see docs/INTERFACE.md section 3) */
void cu_out_test_started(cu_output *out, const cu_test *t);
void cu_out_test_ended(cu_output *out, const cu_result *res);
void cu_out_failure(cu_output *out, const cu_test *t,
                    const char *fail_file, size_t fail_line,
                    const char *message);
void cu_out_summary(cu_output *out, const cu_result *res);

/* platform.c */
size_t cu_time_in_millis(void);

#endif
