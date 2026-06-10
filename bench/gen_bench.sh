#!/bin/sh
# Generates the benchmark test sources:
# - compile-time: N translation units, each with a TEST_GROUP and M tests
# - runtime: one TU with heavy assertion counts
set -eu
OUT="$1"
FILES="${2:-20}"
TESTS_PER_FILE="${3:-20}"
mkdir -p "$OUT"

i=0
while [ "$i" -lt "$FILES" ]; do
    f="$OUT/bench_$i.cpp"
    {
        echo '#include "CppUTest/TestHarness.h"'
        echo "TEST_GROUP(BenchGroup$i)"
        echo "{"
        echo "    int value;"
        echo "    TEST_SETUP() { value = $i; }"
        echo "};"
        j=0
        while [ "$j" -lt "$TESTS_PER_FILE" ]; do
            echo "TEST(BenchGroup$i, test$j)"
            echo "{"
            echo "    LONGS_EQUAL($i, value);"
            echo "    CHECK(value >= 0);"
            echo "    STRCMP_EQUAL(\"bench\", \"bench\");"
            echo "}"
            j=$((j + 1))
        done
    } > "$f"
    i=$((i + 1))
done

cat > "$OUT/bench_main.cpp" <<'EOF'
#include "CppUTest/CommandLineTestRunner.h"

int main(int argc, char **argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
EOF

# runtime stress: 8 independent groups so -jN can spread them
cat > "$OUT/bench_runtime.cpp" <<'EOF'
#include "CppUTest/TestHarness.h"

volatile long vl = 7;

#define STRESS_GROUP(N) \
TEST_GROUP(Stress##N) {}; \
TEST(Stress##N, checks) \
{ \
    for (int i = 0; i < 250000; i++) { \
        LONGS_EQUAL(7, vl); \
        CHECK(vl >= 0); \
    } \
    const char *volatile s = "benchmark"; \
    for (int i = 0; i < 125000; i++) { \
        STRCMP_EQUAL("benchmark", s); \
    } \
    for (int i = 0; i < 62500; i++) { \
        char *p = new char[64]; \
        p[0] = (char)i; \
        delete [] p; \
    } \
}

STRESS_GROUP(0)
STRESS_GROUP(1)
STRESS_GROUP(2)
STRESS_GROUP(3)
STRESS_GROUP(4)
STRESS_GROUP(5)
STRESS_GROUP(6)
STRESS_GROUP(7)
EOF
