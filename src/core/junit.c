#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Port of upstream JUnitTestOutput.cpp: one XML file per test group, written
 * when the group ends. All formats and quirks preserved, including:
 * - totalCheckCount_ carrying across groups (assertions are per-test deltas
 *   of the run-wide check count)
 * - <system-out> accumulating every print() for the whole run, never reset
 * - fully-filtered groups writing an (empty) "cpputest_.xml"
 * - only the first failure per test recorded */

typedef struct junit_case {
    char *name;
    char *file;
    size_t line;
    int ignored;
    size_t exec_time;
    size_t check_count;
    int has_failure;
    char *failure_file;
    size_t failure_line;
    char *failure_message;
    struct junit_case *next;
} junit_case;

struct cu_junit_state {
    char *group;
    size_t test_count;
    size_t failure_count;
    size_t total_check_count; /* persists across groups, like upstream */
    junit_case *head;
    junit_case *tail;
    char *std_output; /* accumulated print() text, never reset */
    size_t std_output_len;
};

cu_junit_state *cu_junit_create(void)
{
    cu_junit_state *j = cu_xcalloc(1, sizeof *j);
    j->group = cu_xstrdup("");
    j->std_output = cu_xstrdup("");
    return j;
}

static void reset_group(cu_junit_state *j)
{
    free(j->group);
    j->group = cu_xstrdup("");
    j->test_count = 0;
    j->failure_count = 0;
    junit_case *cur = j->head;
    while (cur) {
        junit_case *next = cur->next;
        free(cur->name);
        free(cur->file);
        free(cur->failure_file);
        free(cur->failure_message);
        free(cur);
        cur = next;
    }
    j->head = j->tail = NULL;
}

void cu_junit_destroy(cu_junit_state *j)
{
    if (!j)
        return;
    reset_group(j);
    free(j->group);
    free(j->std_output);
    free(j);
}

void cu_junit_test_started(cu_output *out, const cu_test *t)
{
    cu_junit_state *j = out->junit;
    j->test_count++;
    free(j->group);
    j->group = cu_xstrdup(t->group);

    junit_case *node = cu_xcalloc(1, sizeof *node);
    node->name = cu_xstrdup(t->name);
    node->file = cu_xstrdup(t->file);
    node->line = t->line;
    node->ignored = t->is_ignored && !t->run_ignored;
    if (j->tail)
        j->tail->next = node;
    else
        j->head = node;
    j->tail = node;
}

void cu_junit_test_ended(cu_output *out, const cu_result *res)
{
    cu_junit_state *j = out->junit;
    if (!j->tail)
        return;
    j->tail->exec_time = res->current_test_ms;
    j->tail->check_count = res->check_count;
}

void cu_junit_failure(cu_output *out, const cu_test *t, const char *fail_file,
                      size_t fail_line, const char *message)
{
    (void)t;
    cu_junit_state *j = out->junit;
    if (!j->tail || j->tail->has_failure)
        return;
    j->failure_count++;
    j->tail->has_failure = 1;
    j->tail->failure_file = cu_xstrdup(fail_file);
    j->tail->failure_line = fail_line;
    j->tail->failure_message = cu_xstrdup(message);
}

void cu_junit_print(cu_output *out, const char *s)
{
    cu_junit_state *j = out->junit;
    size_t add = strlen(s);
    /* the cached length keeps chatty suites (UT_PRINT in loops) linear */
    j->std_output = cu_xrealloc(j->std_output, j->std_output_len + add + 1);
    memcpy(j->std_output + j->std_output_len, s, add + 1);
    j->std_output_len += add;
}

/* encodeXmlText, replacements applied in upstream order */
static char *encode_xml(const char *text)
{
    static const struct {
        const char *from, *to;
    } reps[] = {
        {"&", "&amp;"}, {"\"", "&quot;"}, {"<", "&lt;"},
        {">", "&gt;"},  {"\r", "&#13;"},  {"\n", "&#10;"},
    };
    char *buf = cu_xstrdup(text);
    for (size_t r = 0; r < sizeof reps / sizeof reps[0]; r++) {
        size_t flen = strlen(reps[r].from);
        size_t tlen = strlen(reps[r].to);
        size_t count = 0;
        for (const char *p = buf; (p = strstr(p, reps[r].from)) != NULL;
             p += flen)
            count++;
        if (!count)
            continue;
        char *next = cu_xmalloc(strlen(buf) + count * (tlen - flen) + 1);
        const char *src = buf;
        char *dst = next;
        const char *hit;
        while ((hit = strstr(src, reps[r].from)) != NULL) {
            memcpy(dst, src, (size_t)(hit - src));
            dst += hit - src;
            memcpy(dst, reps[r].to, tlen);
            dst += tlen;
            src = hit + flen;
        }
        strcpy(dst, src);
        free(buf);
        buf = next;
    }
    return buf;
}

/* createFileName + encodeFileName */
static char *file_name_for_group(const cu_output *out, const char *group)
{
    const char *package = out->package_name ? out->package_name : "";
    char *name;
    if (package[0])
        name = cu_str_printf("cpputest_%s_%s", package, group);
    else
        name = cu_str_printf("cpputest_%s", group);
    for (char *p = name; *p; p++)
        if (strchr("/\\?%*:|\"<>", *p))
            *p = '_';
    char *result = cu_str_printf("%s.xml", name);
    free(name);
    return result;
}

void cu_junit_group_ended(cu_output *out, const cu_result *res)
{
    cu_junit_state *j = out->junit;
    const char *package = out->package_name ? out->package_name : "";
    size_t group_ms = res->current_group_ms;

    /* O_NOFOLLOW: don't write through a symlink pre-planted in the CWD
     * under our predictable cpputest_*.xml name (O_TRUNC, not O_EXCL —
     * re-running in the same directory legitimately overwrites) */
    char *fname = file_name_for_group(out, j->group);
    FILE *f = NULL;
    int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0666);
    free(fname);
    if (fd >= 0) {
        f = fdopen(fd, "w");
        if (!f)
            close(fd);
    }
    if (!f) {
        reset_group(j);
        return;
    }

    char timestamp[64];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%S", &tm_now);

    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n", f);
    fprintf(f,
            "<testsuite errors=\"0\" failures=\"%d\" hostname=\"localhost\" "
            "name=\"%s\" tests=\"%d\" time=\"%d.%03d\" timestamp=\"%s\">\n",
            (int)j->failure_count, j->group, (int)j->test_count,
            (int)(group_ms / 1000), (int)(group_ms % 1000), timestamp);
    fputs("<properties>\n</properties>\n", f);

    for (junit_case *cur = j->head; cur; cur = cur->next) {
        fprintf(f,
                "<testcase classname=\"%s%s%s\" name=\"%s\" assertions=\"%d\" "
                "time=\"%d.%03d\" file=\"%s\" line=\"%d\">\n",
                package, package[0] ? "." : "", j->group, cur->name,
                (int)(cur->check_count - j->total_check_count),
                (int)(cur->exec_time / 1000), (int)(cur->exec_time % 1000),
                cur->file, (int)cur->line);
        j->total_check_count = cur->check_count;
        if (cur->has_failure) {
            char *encoded = encode_xml(cur->failure_message);
            fprintf(f,
                    "<failure message=\"%s:%d: %s\" "
                    "type=\"AssertionFailedError\">\n",
                    cur->failure_file, (int)cur->failure_line, encoded);
            free(encoded);
            fputs("</failure>\n", f);
        } else if (cur->ignored) {
            fputs("<skipped />\n", f);
        }
        fputs("</testcase>\n", f);
    }

    char *encoded_out = encode_xml(j->std_output);
    fputs("<system-out>", f);
    fputs(encoded_out, f);
    fputs("</system-out>\n", f);
    free(encoded_out);
    fputs("<system-err></system-err>\n", f);
    fputs("</testsuite>\n", f);
    fclose(f);

    reset_group(j);
}
