#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
/* Darwin hides mkdtemp behind _DARWIN_C_SOURCE under strict POSIX */
#define _DARWIN_C_SOURCE
#endif

#include "internal.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* POSIX process isolation.
 *
 * -p (upstream-compatible): each test runs in a forked child; the parent
 * converts abnormal exits into failures with upstream's exact messages
 * (UtestPlatform.cpp GccPlatformSpecificRunTestInASeperateProcess).
 *
 * -jN (cpputest-turbo extension): test groups are distributed round-robin
 * over N forked workers; each worker writes its output to a temp file and
 * its counters to a stats file, and the parent replays outputs in worker
 * order and prints one merged summary, so the run is deterministic. */

static void add_process_failure(cu_test *t, const char *message)
{
    /* TestFailure(shell, message): file/line = the test's own */
    cu_add_failure(t->file, t->line, message);
}

/* upstream SetTestFailureByStatusCode */
static void set_failure_by_status_code(cu_test *t, int status)
{
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        add_process_failure(t, "Failed in separate process");
    } else if (WIFSIGNALED(status)) {
        char message[64];
        snprintf(message, sizeof message,
                 "Failed in separate process - killed by signal %d",
                 WTERMSIG(status));
        add_process_failure(t, message);
    } else if (WIFSTOPPED(status)) {
        add_process_failure(t, "Stopped in separate process - continuing");
    }
}

void cu_fork_run_one_test(cu_test *t, cu_result *res)
{
    pid_t cpid = fork();

    if (cpid == -1) {
        add_process_failure(t, "Call to fork() failed");
        return;
    }

    if (cpid == 0) { /* child */
        size_t initial_failure_count = res->failure_count;
        cu_run_test_actions(t);
        fflush(NULL);
        _exit(initial_failure_count < res->failure_count);
    }

    /* parent */
    int status = 0;
    pid_t w;
    size_t retries = 0;
    do {
        w = waitpid(cpid, &status, WUNTRACED);
        if (w == -1) {
            if (errno == EINTR) {
                if (retries > 30) {
                    add_process_failure(
                        t,
                        "Call to waitpid() failed with EINTR. Tried 30 times "
                        "and giving up! Sometimes happens in debugger");
                    return;
                }
                retries++;
            } else {
                add_process_failure(t, "Call to waitpid() failed");
                return;
            }
        } else {
            set_failure_by_status_code(t, status);
            if (WIFSTOPPED(status))
                kill(w, SIGCONT);
        }
    } while (w == -1 || (!WIFEXITED(status) && !WIFSIGNALED(status)));
}

/* ------------------------------ parallel -------------------------------- */

typedef struct {
    cu_test *first; /* first test of the group */
    size_t count;   /* tests in the group */
} group_range;

int cu_run_parallel(const cu_args *a, cu_output *out, cu_result *total)
{
    /* snapshot groups */
    size_t group_count = 0;
    cu_test *t = cu_registry_tests();
    const char *last_group = NULL;
    for (; t; t = t->next) {
        if (!last_group || 0 != strcmp(t->group, last_group)) {
            group_count++;
            last_group = t->group;
        }
    }
    if (group_count == 0) {
        cu_out_summary(out, total);
        return 0;
    }

    group_range *groups = calloc(group_count, sizeof *groups);
    size_t gi = 0;
    last_group = NULL;
    for (t = cu_registry_tests(); t; t = t->next) {
        if (!last_group || 0 != strcmp(t->group, last_group)) {
            groups[gi].first = t;
            gi++;
            last_group = t->group;
        }
        groups[gi - 1].count++;
    }

    int workers = a->parallel_workers;
    if ((size_t)workers > group_count)
        workers = (int)group_count;

    /* per-run private directory: avoids collisions between concurrent runs
     * sharing $TMPDIR and symlink games with predictable names */
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir)
        tmpdir = "/tmp";
    char workdir[512];
    snprintf(workdir, sizeof workdir, "%s/cpputest-par-XXXXXX", tmpdir);
    if (!mkdtemp(workdir)) {
        fprintf(stderr, "cpputest: mkdtemp failed: %s\n", workdir);
        total->failure_count++; /* surface as a failing run */
        cu_out_summary(out, total);
        free(groups);
        return 1;
    }

    pid_t *pids = calloc((size_t)workers, sizeof *pids);
    char path[600];

    total->time_started = cu_time_in_millis();

    for (int w = 0; w < workers; w++) {
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "cpputest: fork() failed for worker %d\n", w);
            pids[w] = -1;
            continue;
        }
        if (pid == 0) { /* worker child */
            /* rebuild the registry list with only this worker's groups,
             * preserving relative order */
            cu_test *head = NULL;
            cu_test **link = &head;
            for (size_t g = 0; g < group_count; g++) {
                if ((int)(g % (size_t)workers) != w)
                    continue;
                cu_test *gt = groups[g].first;
                for (size_t k = 0; k < groups[g].count; k++) {
                    *link = gt;
                    link = &(*link)->next;
                    gt = gt->next;
                }
            }
            *link = NULL;
            cu_registry_set_tests(head);

            snprintf(path, sizeof path, "%s/worker-%d.out", workdir, w);
            if (!freopen(path, "w", stdout))
                _exit(99);

            cu_result res;
            memset(&res, 0, sizeof res);
            cu_output wout = *out;
            wout.suppress_summary = 1;
            if (wout.type == CU_OUTPUT_TYPE_JUNIT)
                wout.junit = cu_junit_create();
            cu_set_current_result_output(&res, &wout);
            cu_run_all_tests_internal(a, &res, &wout);
            fflush(NULL);

            snprintf(path, sizeof path, "%s/worker-%d.stats", workdir, w);
            FILE *sf = fopen(path, "wb");
            if (sf) {
                fwrite(&res, sizeof res, 1, sf);
                fclose(sf);
            }
            _exit(0);
        }
        pids[w] = pid;
    }

    /* collect in worker order: deterministic merged output */
    for (int w = 0; w < workers; w++) {
        if (pids[w] == -1)
            continue;
        int status = 0;
        while (waitpid(pids[w], &status, 0) == -1 && errno == EINTR)
            ;

        snprintf(path, sizeof path, "%s/worker-%d.out", workdir, w);
        FILE *of = fopen(path, "r");
        if (of) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof buf, of)) > 0)
                fwrite(buf, 1, n, stdout);
            fclose(of);
        }
        remove(path);

        snprintf(path, sizeof path, "%s/worker-%d.stats", workdir, w);
        FILE *sf = fopen(path, "rb");
        cu_result res;
        memset(&res, 0, sizeof res);
        if (sf) {
            if (1 != fread(&res, sizeof res, 1, sf))
                memset(&res, 0, sizeof res);
            fclose(sf);
        }
        remove(path);

        if (WIFSIGNALED(status) ||
            (WIFEXITED(status) && WEXITSTATUS(status) != 0)) {
            /* a worker that died abnormally counts as one failure */
            res.failure_count++;
            fprintf(stdout, "\ncpputest: worker %d terminated abnormally\n", w);
        }

        total->test_count += res.test_count;
        total->run_count += res.run_count;
        total->check_count += res.check_count;
        total->failure_count += res.failure_count;
        total->ignored_count += res.ignored_count;
        total->filtered_out_count += res.filtered_out_count;
    }

    total->total_ms = cu_time_in_millis() - total->time_started;
    cu_out_summary(out, total);

    rmdir(workdir);
    free(groups);
    free(pids);
    return 0;
}
