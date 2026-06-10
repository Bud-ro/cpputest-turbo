/* Seeded fuzzer for the command-line parser. Generates random argv arrays
 * mixing real flag prefixes with garbage and feeds them through
 * cu_args_parse/cu_args_free. Run under ASan/UBSan; any memory error or
 * crash is a bug. Reproduce a failure with FUZZ_SEED=<n>. */

#define _POSIX_C_SOURCE 200809L

#include "../src/core/internal.h"

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

static const char *const stems[] = {
    "-h", "-v", "-vv", "-c", "-p", "-b", "-lg", "-ln", "-ll", "-ri", "-f",
    "-e", "-ci", "-r", "-g", "-t", "-st", "-xt", "-xst", "-sg", "-xg",
    "-xsg", "-n", "-sn", "-xn", "-xsn", "-s", "-o", "-k", "-j",
    "TEST(", "IGNORE_TEST(", "", "-", "--",
};

static char *random_token(void)
{
    char buf[64];
    size_t pos = 0;

    if (r() % 4 != 0) {
        const char *stem = stems[r() % (sizeof stems / sizeof stems[0])];
        size_t len = strlen(stem);
        memcpy(buf, stem, len);
        pos = len;
    }
    unsigned extra = r() % 12;
    static const char tailchars[] =
        "abzAZ019.,()<>|*?%:\\\"/- \t" "\x01\x7f\xff";
    for (unsigned i = 0; i < extra && pos < sizeof buf - 1; i++)
        buf[pos++] = tailchars[r() % (sizeof tailchars - 1)];
    buf[pos] = '\0';
    return strdup(buf);
}

int main(void)
{
    const char *seed_env = getenv("FUZZ_SEED");
    const char *iter_env = getenv("FUZZ_ITERS");
    unsigned long long base_seed = seed_env ? strtoull(seed_env, NULL, 10) : 1;
    unsigned long iters = iter_env ? strtoul(iter_env, NULL, 10) : 200000;

    for (unsigned long it = 0; it < iters; it++) {
        rng = base_seed + it * 2654435761ull + 1;

        int argc = 1 + (int)(r() % 8);
        char *argv[10];
        argv[0] = strdup("fuzz");
        for (int i = 1; i < argc; i++)
            argv[i] = random_token();

        cu_args args;
        cu_args_parse(&args, argc, (const char *const *)argv);
        cu_args_free(&args);

        for (int i = 0; i < argc; i++)
            free(argv[i]);
    }
    printf("fuzz_args: %lu iterations clean\n", iters);
    return 0;
}
