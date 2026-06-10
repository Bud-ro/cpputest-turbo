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

# runtime stress: lots of assertions
cat > "$OUT/bench_runtime.cpp" <<'EOF'
#include "CppUTest/TestHarness.h"

TEST_GROUP(Stress)
{
};

TEST(Stress, manyChecks)
{
    for (int i = 0; i < 1000000; i++) {
        LONGS_EQUAL(i, i);
        CHECK(i >= 0);
    }
}

TEST(Stress, manyStringChecks)
{
    for (int i = 0; i < 500000; i++) {
        STRCMP_EQUAL("benchmark", "benchmark");
    }
}

TEST(Stress, memoryChurn)
{
    for (int i = 0; i < 250000; i++) {
        char *p = new char[64];
        p[0] = (char)i;
        delete [] p;
    }
}
EOF
