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

/* runner.c */
cu_result *cu_result_current(void);
cu_output *cu_output_current(void);

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
