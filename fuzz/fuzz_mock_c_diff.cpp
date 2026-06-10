/* Differential mock_c fuzzer — C++ runner half.
 *
 * The random sequences live in fuzz_mock_c_seq.c, a plain C11 TU driving
 * CppUTestExt/MockSupport_c.h; this TU is the C++ side that binds the tests
 * into CppUTest. The pair compiles unchanged against upstream and against
 * cpputest-turbo; outputs are diffed by scripts/check-fuzz.sh exactly like
 * fuzz_mock_diff. 50 seeds per process: test N runs FUZZ_SEED*50+N. */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport_c.h"

#include <stdlib.h>

extern "C" {
void fz_mock_c_set_trace(int on);
void fz_mock_c_sequence(unsigned long long seed);
}

TEST_GROUP(MockCFuzz){TEST_TEARDOWN(){mock_c()->clear();
mock_c()->removeAllComparatorsAndCopiers();
}
}
;

static unsigned long long base_seed;

#define FZ_TEST(N)                                                             \
    TEST(MockCFuzz, seq##N)                                                    \
    {                                                                          \
        fz_mock_c_sequence(base_seed * 50 + N);                                \
    }

// clang-format off
FZ_TEST(0)  FZ_TEST(1)  FZ_TEST(2)  FZ_TEST(3)  FZ_TEST(4)
FZ_TEST(5)  FZ_TEST(6)  FZ_TEST(7)  FZ_TEST(8)  FZ_TEST(9)
FZ_TEST(10) FZ_TEST(11) FZ_TEST(12) FZ_TEST(13) FZ_TEST(14)
FZ_TEST(15) FZ_TEST(16) FZ_TEST(17) FZ_TEST(18) FZ_TEST(19)
FZ_TEST(20) FZ_TEST(21) FZ_TEST(22) FZ_TEST(23) FZ_TEST(24)
FZ_TEST(25) FZ_TEST(26) FZ_TEST(27) FZ_TEST(28) FZ_TEST(29)
FZ_TEST(30) FZ_TEST(31) FZ_TEST(32) FZ_TEST(33) FZ_TEST(34)
FZ_TEST(35) FZ_TEST(36) FZ_TEST(37) FZ_TEST(38) FZ_TEST(39)
FZ_TEST(40) FZ_TEST(41) FZ_TEST(42) FZ_TEST(43) FZ_TEST(44)
FZ_TEST(45) FZ_TEST(46) FZ_TEST(47) FZ_TEST(48) FZ_TEST(49)
// clang-format on

int main(int argc, char **argv)
{
    const char *seed_env = getenv("FUZZ_SEED");
    base_seed = seed_env ? strtoull(seed_env, NULL, 10) : 0;
    fz_mock_c_set_trace(getenv("FUZZ_TRACE") != NULL);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
