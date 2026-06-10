#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Port of upstream MemoryLeakDetector.cpp. Tracked allocations carry 3 guard
 * bytes ("BAS") after the user block; freed memory is poisoned with 0xCD;
 * the report is built in a fixed 4096-byte buffer with upstream's
 * write-limit/footer-reservation mechanics. Failure messages and the leak
 * report are byte-identical to upstream (docs/INTERFACE.md §7). */

#define CU_MEM_BUFFER_LEN 4096
#define GUARD_SIZE 3
#define BUCKETS 1024

static const char GuardBytes[GUARD_SIZE] = { 'B', 'A', 'S' };
static const char *UNKNOWN = "<unknown>";

static const char *alloc_names[] = { "new", "new []", "malloc", "unknown" };
static const char *free_names[] = { "delete", "delete []", "free", "unknown" };

typedef struct mem_node {
    void *ptr;
    size_t size;
    const char *file;
    size_t line;
    unsigned number;
    int type; /* CU_MEM_NEW / CU_MEM_NEW_ARRAY / CU_MEM_MALLOC */
    int checking; /* allocated during the per-test checking period */
    struct mem_node *next;
} mem_node;

static mem_node *buckets[BUCKETS];
static unsigned alloc_counter;
static int tracking_on = 1;
static int checking_now;
static int save_counter;
static int saved_tracking_state;

/* out-of-memory simulation for the cpputest_malloc family */
static int malloc_out_of_memory;
static int malloc_countdown_active;
static int malloc_countdown;
static int malloc_count;

static size_t bucket_of(const void *p)
{
    return ((size_t)p >> 4) & (BUCKETS - 1);
}

/* ------------------------ report string buffer -------------------------- */

#define MEM_LEAK_TOO_MUCH "\netc etc etc etc. !!!! Too many memory leaks to report. Bailing out\n"
#define MEM_LEAK_FOOTER "Total number of leaks: "
#define MEM_LEAK_ADDITION_MALLOC_WARNING "NOTE:\n" \
    "\tMemory leak reports about malloc and free can be caused by allocating using the cpputest version of malloc,\n" \
    "\tbut deallocate using the standard free.\n" \
    "\tIf this is the case, check whether your malloc/free replacements are working (#define malloc cpputest_malloc etc).\n"

static struct {
    char buf[CU_MEM_BUFFER_LEN];
    size_t filled;
    size_t write_limit;
} rb = { { 0 }, 0, CU_MEM_BUFFER_LEN - 1 };

static void rb_clear(void)
{
    rb.filled = 0;
    rb.buf[0] = '\0';
    rb.write_limit = CU_MEM_BUFFER_LEN - 1;
}

static void rb_add(const char *format, ...)
{
    size_t left = rb.write_limit - rb.filled;
    if (left == 0)
        return;
    va_list args;
    va_start(args, format);
    int count = vsnprintf(rb.buf + rb.filled, left + 1, format, args);
    va_end(args);
    if (count > 0)
        rb.filled += (size_t)count;
    if (rb.filled > rb.write_limit)
        rb.filled = rb.write_limit;
}

/* SimpleStringBuffer::addMemoryDump */
static void rb_add_memory_dump(const void *memory, size_t memory_size)
{
    const unsigned char *bytes = memory;
    const size_t max_line = 16;
    size_t pos = 0;

    while (pos < memory_size) {
        rb_add("    %04lx: ", (unsigned long)pos);
        size_t in_line = memory_size - pos;
        if (in_line > max_line)
            in_line = max_line;
        size_t leftover = max_line - in_line;

        for (size_t p = 0; p < in_line; p++) {
            rb_add("%02hx ", (unsigned short)bytes[pos + p]);
            if (p == max_line / 2 - 1)
                rb_add(" ");
        }
        for (size_t p = 0; p < leftover; p++)
            rb_add("   ");
        if (leftover > max_line / 2)
            rb_add(" ");

        rb_add("|");
        for (size_t p = 0; p < in_line; p++) {
            char c = (char)bytes[pos + p];
            if (c < ' ' || c > '~')
                c = '.';
            rb_add("%c", (int)c);
        }
        rb_add("|\n");
        pos += in_line;
    }
}

/* ----------------------------- tracking --------------------------------- */

void *cu_mem_alloc_tracked(size_t size, const char *file, size_t line, int type)
{
    char *raw = malloc(size + GUARD_SIZE);
    if (!raw)
        return NULL;
    memcpy(raw + size, GuardBytes, GUARD_SIZE);

    mem_node *node = malloc(sizeof *node);
    node->ptr = raw;
    node->size = size;
    node->file = file ? file : UNKNOWN;
    node->line = line;
    node->number = ++alloc_counter;
    node->type = type;
    node->checking = checking_now;
    size_t b = bucket_of(raw);
    node->next = buckets[b];
    buckets[b] = node;
    return raw;
}

static mem_node *remove_node(void *p)
{
    mem_node **link = &buckets[bucket_of(p)];
    while (*link) {
        if ((*link)->ptr == p) {
            mem_node *node = *link;
            *link = node->next;
            return node;
        }
        link = &(*link)->next;
    }
    return NULL;
}

/* MemoryLeakFailure::fail — fails the current test using the test NAME as
 * the failure "file" and the TEST() line (upstream quirk), then longjmps.
 * Outside a test the message goes to stderr. */
static void mem_fail(const char *message)
{
    cu_test *t = cu_current_test();
    if (t) {
        cu_fail_with_message(t->name, t->line, message);
    } else {
        fputs(message, stderr);
        fputs("\n", stderr);
    }
}

static void report_dealloc_failure(const char *headline, const mem_node *node,
                                   const char *free_file, size_t free_line,
                                   int free_type)
{
    rb_clear();
    rb_add("%s", headline);
    rb_add("   allocated at file: %s line: %d size: %lu type: %s\n",
           node ? node->file : UNKNOWN, node ? (int)node->line : 0,
           node ? (unsigned long)node->size : 0UL,
           node ? alloc_names[node->type] : "unknown");
    rb_add("   deallocated at file: %s line: %d type: %s\n",
           free_file, (int)free_line, free_names[free_type]);
    mem_fail(rb.buf);
}

void cu_mem_free_tracked(void *p, const char *file, size_t line, int type)
{
    if (!p)
        return;
    if (!file)
        file = UNKNOWN;

    mem_node *node = remove_node(p);
    if (!node) {
        report_dealloc_failure("Deallocating non-allocated memory\n",
                               NULL, file, line, type);
        return; /* only reached outside a test run */
    }
    if (node->type != type) {
        report_dealloc_failure("Allocation/deallocation type mismatch\n",
                               node, file, line, type);
        free(node);
        return;
    }
    if (0 != memcmp((char *)p + node->size, GuardBytes, GUARD_SIZE)) {
        report_dealloc_failure("Memory corruption (written out of bounds?)\n",
                               node, file, line, type);
        free(node);
        return;
    }
    memset(p, 0xCD, node->size);
    free(p);
    free(node);
}

void *cu_mem_realloc_tracked(void *p, size_t size, const char *file, size_t line)
{
    if (!file)
        file = UNKNOWN;
    if (!p)
        return cu_mem_alloc_tracked(size, file, line, CU_MEM_MALLOC);

    mem_node *node = remove_node(p);
    if (!node) {
        report_dealloc_failure("Deallocating non-allocated memory\n",
                               NULL, file, line, CU_MEM_MALLOC);
        return NULL;
    }
    size_t old_size = node->size;
    int corrupt = 0 != memcmp((char *)p + old_size, GuardBytes, GUARD_SIZE);
    if (corrupt) {
        report_dealloc_failure("Memory corruption (written out of bounds?)\n",
                               node, file, line, CU_MEM_MALLOC);
        free(node);
        return NULL;
    }
    free(node);

    void *fresh = cu_mem_alloc_tracked(size, file, line, CU_MEM_MALLOC);
    if (fresh) {
        memcpy(fresh, p, old_size < size ? old_size : size);
        memset(p, 0xCD, old_size);
        free(p);
    }
    return fresh;
}

void cu_mem_tracking_set(int on)
{
    tracking_on = on;
}

int cu_mem_tracking(void)
{
    return tracking_on;
}

void cu_mem_save_and_disable_tracking(void)
{
    if (save_counter++ == 0) {
        saved_tracking_state = tracking_on;
        tracking_on = 0;
    }
}

void cu_mem_restore_tracking(void)
{
    if (save_counter > 0 && --save_counter == 0)
        tracking_on = saved_tracking_state;
}

void cu_mem_start_checking(void)
{
    checking_now = 1;
}

void cu_mem_stop_checking(void)
{
    checking_now = 0;
}

void cu_mem_mark_checking_as_global(void)
{
    for (size_t b = 0; b < BUCKETS; b++)
        for (mem_node *n = buckets[b]; n; n = n->next)
            n->checking = 0;
}

size_t cu_mem_leak_count(int checking_only)
{
    size_t count = 0;
    for (size_t b = 0; b < BUCKETS; b++)
        for (mem_node *n = buckets[b]; n; n = n->next)
            if (!checking_only || n->checking)
                count++;
    return count;
}

/* MemoryLeakDetector::report */
const char *cu_mem_leak_report(int checking_only)
{
    rb_clear();
    size_t total_leaks = 0;
    int warn_malloc = 0;

    /* startMemoryLeakReporting: reserve room for footer + malloc warning */
    size_t footer_reserve = sizeof(MEM_LEAK_FOOTER) + 10 + sizeof(MEM_LEAK_TOO_MUCH)
                          + sizeof(MEM_LEAK_ADDITION_MALLOC_WARNING);
    rb.write_limit = CU_MEM_BUFFER_LEN - footer_reserve;

    for (size_t b = 0; b < BUCKETS; b++) {
        for (mem_node *n = buckets[b]; n; n = n->next) {
            if (checking_only && !n->checking)
                continue;
            if (total_leaks == 0)
                rb_add("Memory leak(s) found.\n");
            total_leaks++;
            rb_add("Alloc num (%u) Leak size: %lu Allocated at: %s and line: %d. Type: \"%s\"\n\tMemory: <%p> Content:\n",
                   n->number, (unsigned long)n->size, n->file, (int)n->line,
                   alloc_names[n->type], n->ptr);
            rb_add_memory_dump(n->ptr, n->size);
            if (n->type == CU_MEM_MALLOC)
                warn_malloc = 1;
        }
    }

    if (total_leaks == 0) {
        rb_add("No memory leaks were detected.");
        return rb.buf;
    }

    int reached_capacity = rb.filled >= rb.write_limit;
    rb.write_limit = CU_MEM_BUFFER_LEN - 1;
    if (reached_capacity)
        rb_add("%s", MEM_LEAK_TOO_MUCH);
    rb_add("%s %d\n", MEM_LEAK_FOOTER, (int)total_leaks);
    if (warn_malloc)
        rb_add("%s", MEM_LEAK_ADDITION_MALLOC_WARNING);
    return rb.buf;
}

void cu_mem_destroy_all(void)
{
    for (size_t b = 0; b < BUCKETS; b++) {
        mem_node *n = buckets[b];
        while (n) {
            mem_node *next = n->next;
            free(n);
            n = next;
        }
        buckets[b] = NULL;
    }
}

/* --------------------- public C allocation API -------------------------- */

static void *malloc_maybe_tracked(size_t size, const char *file, size_t line)
{
    malloc_count++;
    if (malloc_out_of_memory)
        return NULL;
    if (malloc_countdown_active && --malloc_countdown <= 0) {
        malloc_out_of_memory = 1;
        return NULL;
    }
    if (tracking_on)
        return cu_mem_alloc_tracked(size, file, line, CU_MEM_MALLOC);
    return malloc(size);
}

void *cpputest_malloc_location(size_t size, const char *file, size_t line)
{
    return malloc_maybe_tracked(size, file, line);
}

void *cpputest_malloc(size_t size)
{
    return cpputest_malloc_location(size, UNKNOWN, 0);
}

void *cpputest_calloc_location(size_t count, size_t size,
                               const char *file, size_t line)
{
    void *p = malloc_maybe_tracked(count * size, file, line);
    if (p)
        memset(p, 0, count * size);
    return p;
}

void *cpputest_calloc(size_t count, size_t size)
{
    return cpputest_calloc_location(count, size, UNKNOWN, 0);
}

void *cpputest_realloc_location(void *p, size_t size,
                                const char *file, size_t line)
{
    if (tracking_on)
        return cu_mem_realloc_tracked(p, size, file, line);
    return realloc(p, size);
}

void *cpputest_realloc(void *p, size_t size)
{
    return cpputest_realloc_location(p, size, UNKNOWN, 0);
}

void cpputest_free_location(void *p, const char *file, size_t line)
{
    if (tracking_on)
        cu_mem_free_tracked(p, file, line, CU_MEM_MALLOC);
    else
        free(p);
}

void cpputest_free(void *p)
{
    cpputest_free_location(p, UNKNOWN, 0);
}

static char *strdup_n(const char *s, size_t n, const char *file, size_t line)
{
    char *p = malloc_maybe_tracked(n + 1, file, line);
    if (p) {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

char *cpputest_strdup_location(const char *s, const char *file, size_t line)
{
    return strdup_n(s, strlen(s), file, line);
}

char *cpputest_strdup(const char *s)
{
    return cpputest_strdup_location(s, UNKNOWN, 0);
}

char *cpputest_strndup_location(const char *s, size_t n,
                                const char *file, size_t line)
{
    size_t len = strlen(s);
    if (len > n)
        len = n;
    return strdup_n(s, len, file, line);
}

char *cpputest_strndup(const char *s, size_t n)
{
    return cpputest_strndup_location(s, n, UNKNOWN, 0);
}

void cpputest_malloc_set_out_of_memory(void)
{
    malloc_out_of_memory = 1;
}

void cpputest_malloc_set_not_out_of_memory(void)
{
    malloc_out_of_memory = 0;
    malloc_countdown_active = 0;
}

void cpputest_malloc_set_out_of_memory_countdown(int countdown)
{
    malloc_countdown_active = 1;
    malloc_countdown = countdown;
}

void cpputest_malloc_count_reset(void)
{
    malloc_count = 0;
}

int cpputest_malloc_get_count(void)
{
    return malloc_count;
}
