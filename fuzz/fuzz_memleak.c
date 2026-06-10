/* Seeded fuzzer for the leak tracker: random sequences of tracked
 * malloc/calloc/realloc/strdup/free with adversarial sizes (freelist class
 * boundaries, zero, large), occasional wrong-type frees and double-ish
 * patterns, all inside a real test context so failure longjmps are handled.
 * Run under ASan/UBSan. Reproduce with FUZZ_SEED=<n>. */

#include <cpputest_core/core.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long rng;

static unsigned r(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (unsigned)(rng >> 32);
}

static void drop_sink(const char *text, void *arg)
{
    (void)text;
    (void)arg;
}

#define LIVE_MAX 64
static void *live[LIVE_MAX];
static size_t live_size[LIVE_MAX];
static int live_count;

static size_t random_size(void)
{
    switch (r() % 6) {
    case 0:
        return 0;
    case 1:
        return r() % 8;
    case 2:
        return 61 + r() % 8; /* freelist class boundary */
    case 3:
        return 125 + r() % 8; /* another boundary */
    case 4:
        return r() % 256;
    default:
        return 900 + r() % 400; /* beyond freelist classes */
    }
}

static void *t_create(cu_test *t)
{
    (void)t;
    return NULL;
}
static void t_destroy(cu_test *t, void *f)
{
    (void)t;
    (void)f;
}
static void t_setup(cu_test *t, void *f)
{
    (void)t;
    (void)f;
}

static void t_body(cu_test *t, void *f)
{
    (void)t;
    (void)f;
    unsigned ops = 10 + r() % 40;
    for (unsigned i = 0; i < ops; i++) {
        unsigned op = r() % 100;
        if (op < 35 && live_count < LIVE_MAX) { /* alloc */
            size_t sz = random_size();
            void *p = NULL;
            switch (r() % 4) {
            case 0:
                p = cpputest_malloc(sz);
                break;
            case 1:
                p = cpputest_calloc(sz ? sz : 1, 1);
                break;
            case 2:
                p = cpputest_malloc_location(sz, "fuzz.c", r() % 1000);
                break;
            default:
                p = cpputest_strdup("fuzz-string-content");
                sz = 19;
                break;
            }
            if (p) {
                memset(p, 0xAB, sz);
                live[live_count] = p;
                live_size[live_count] = sz;
                live_count++;
            }
        } else if (op < 60 && live_count > 0) { /* free */
            int idx = (int)(r() % (unsigned)live_count);
            cpputest_free(live[idx]);
            live[idx] = live[--live_count];
            live_size[idx] = live_size[live_count];
        } else if (op < 80 && live_count > 0) { /* realloc */
            int idx = (int)(r() % (unsigned)live_count);
            size_t nsz = random_size();
            void *p = cpputest_realloc(live[idx], nsz);
            if (p) {
                live[idx] = p;
                live_size[idx] = nsz;
                if (nsz)
                    memset(p, 0xCE, nsz);
            } else {
                /* realloc failure path consumed/kept the node; drop it */
                live[idx] = live[--live_count];
                live_size[idx] = live_size[live_count];
            }
        } else if (op < 82) { /* free something foreign: must fail the test */
            char local[8];
            cpputest_free(local);               /* longjmps out of the body */
        } else if (op < 84 && live_count > 0) { /* genuine double free */
            int idx = (int)(r() % (unsigned)live_count);
            void *p = live[idx];
            live[idx] = live[--live_count];
            live_size[idx] = live_size[live_count];
            cpputest_free(p);
            cpputest_free(p); /* "Deallocating non-allocated" -> longjmp */
        } else if (op < 92 && live_count > 0) { /* wrong-type free */
            int idx = (int)(r() % (unsigned)live_count);
            void *p = live[idx];
            live[idx] = live[--live_count];
            live_size[idx] = live_size[live_count];
            cu_mem_free_tracked(p, "fuzz.c", 1,
                                CU_MEM_NEW); /* mismatch -> longjmp */
        } else { /* allocate-and-leak (cleaned between iterations) */
            void *p = cpputest_malloc(r() % 64);
            (void)p;
        }
    }
    /* free everything still tracked so iterations stay bounded */
    while (live_count > 0)
        cpputest_free(live[--live_count]);
}

static void t_teardown(cu_test *t, void *f)
{
    (void)t;
    (void)f;
}

static const cu_test_ops ops_tbl = {t_create, t_destroy, t_setup, t_body,
                                    t_teardown};
static cu_test the_test;

int main(void)
{
    const char *seed_env = getenv("FUZZ_SEED");
    const char *iter_env = getenv("FUZZ_ITERS");
    unsigned long long base_seed = seed_env ? strtoull(seed_env, NULL, 10) : 1;
    unsigned long iters = iter_env ? strtoul(iter_env, NULL, 10) : 20000;

    the_test.group = "Fuzz";
    the_test.name = "memleak";
    the_test.file = __FILE__;
    the_test.line = 1;
    the_test.ops = &ops_tbl;
    cu_registry_add(&the_test);
    cu_set_output_sink(drop_sink, NULL);

    for (unsigned long it = 0; it < iters; it++) {
        rng = base_seed + it * 2654435761ull + 1;
        live_count = 0;
        cu_run_stats stats;
        cu_run_registered_tests(&stats, 0);
        /* a longjmp may have left live[] entries tracked; reclaim them */
        while (live_count > 0)
            cpputest_free(live[--live_count]);
        cu_mem_stop_checking();
        cu_mem_destroy_all();
    }
    cu_set_output_sink(NULL, NULL);
    printf("fuzz_memleak: %lu iterations clean\n", iters);
    return 0;
}
