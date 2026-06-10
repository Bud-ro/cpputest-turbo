#!/bin/sh
# Fuzzing gate.
# - fuzz_args / fuzz_strings / fuzz_memleak: seeded random-op drivers built
#   with ASan/UBSan; any sanitizer report fails the run.
# - fuzz_mock_diff: DIFFERENTIAL — the same public-API driver compiled
#   against upstream CppUTest and against us, run with identical seeds; any
#   output divergence (after ms normalization) fails the run. The "ours"
#   side also runs under ASan/UBSan.
# FUZZ_ROUNDS / FUZZ_ITERS scale the effort (defaults are CI-sized).
set -eu
cd "$(dirname "$0")/.."

OUT=build/fuzz
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all"
OPT="${FUZZ_OPT:--O1}"
CFLAGS="-std=c11 $SAN $OPT -g -Iinclude"
CXXFLAGS="-std=c++11 $SAN $OPT -g -Iinclude"
ROUNDS="${FUZZ_ROUNDS:-20}"
echo "fuzz optimization level: $OPT"

# sanitizer-instrumented library
for f in src/core/*.c src/mock/*.c; do
    gcc $CFLAGS -c "$f" -o "$OUT/$(basename "$f").o"
done
g++ $CXXFLAGS -c src/shim/newdelete.cpp -o "$OUT/newdelete.o"
g++ $CXXFLAGS -c src/shim/simplestring.cpp -o "$OUT/simplestring.o"
ar rcs "$OUT/libasan.a" "$OUT"/*.o

# upstream library for the differential fuzzer
if [ ! -f .upstream-cache/libCppUTestUpstream.a ]; then
    mkdir -p .upstream-cache
    ( cd .upstream-cache && \
      g++ -std=c++11 -w -O2 -c ../../third_party/cpputest/src/CppUTest/*.cpp \
          ../../third_party/cpputest/src/CppUTestExt/*.cpp \
          ../../third_party/cpputest/src/Platforms/Gcc/UtestPlatform.cpp \
          -I../../third_party/cpputest/include && \
      ar rcs libCppUTestUpstream.a *.o )
fi

gcc $CFLAGS fuzz/fuzz_args.c "$OUT/libasan.a" -o "$OUT/fuzz_args"
gcc $CFLAGS fuzz/fuzz_strings.c "$OUT/libasan.a" -o "$OUT/fuzz_strings"
gcc $CFLAGS fuzz/fuzz_memleak.c "$OUT/libasan.a" -o "$OUT/fuzz_memleak"

# deliberate failure paths leak longjmp'd temporaries by design; memory
# SAFETY is what these hunt
export ASAN_OPTIONS=detect_leaks=0

echo "== fuzz_args =="
FUZZ_SEED=1 "$OUT/fuzz_args"
echo "== fuzz_strings =="
FUZZ_SEED=1 "$OUT/fuzz_strings"
echo "== fuzz_memleak =="
FUZZ_SEED=1 "$OUT/fuzz_memleak"

echo "== fuzz_mock_diff (differential vs upstream, $ROUNDS rounds x 50 seqs) =="
g++ $CXXFLAGS fuzz/fuzz_mock_diff.cpp build/libCppUTestExt.a "$OUT/libasan.a" \
    -o "$OUT/mockdiff_ours"
g++ -std=c++11 -w -O1 -g -Ithird_party/cpputest/include fuzz/fuzz_mock_diff.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/mockdiff_upstream"

norm() {
    # ms timings + library-INTERNAL failure locations (e.g. the type-check
    # assert in returnIntValue fires inside MockNamedValue.cpp upstream and
    # inside our MockSupport.h — same behavior, unmatchable source paths)
    sed -e 's/, [0-9]* ms)/, 0 ms)/' -e 's/ - [0-9]* ms$/ - 0 ms/' \
        -e 's|[^ ]*/MockNamedValue\.cpp:[0-9]*: error:|LIB_INTERNAL: error:|' \
        -e 's|include/CppUTestExt/MockSupport\.h:[0-9]*: error:|LIB_INTERNAL: error:|' \
        -e 's|[^ ]*/mock_support_c\.c:[0-9]*: error:|LIB_INTERNAL: error:|'
}

echo "== differential torture suites (vs upstream) =="
for SUITE in tests/mock_torture/torture.cpp tests/mock_torture/plugin_torture.cpp \
             tests/mock_torture/sstring_torture.cpp; do
    g++ $CXXFLAGS "$SUITE" build/libCppUTestExt.a "$OUT/libasan.a" \
        -o "$OUT/t_ours"
    g++ -std=c++11 -w -O1 -g -Ithird_party/cpputest/include "$SUITE" \
        .upstream-cache/libCppUTestUpstream.a -o "$OUT/t_upstream"
    rc_o=0; "$OUT/t_ours" >"$OUT/to.txt" 2>&1 || rc_o=$?
    rc_u=0; "$OUT/t_upstream" >"$OUT/tu.txt" 2>&1 || rc_u=$?
    norm <"$OUT/to.txt" >"$OUT/to.norm"
    norm <"$OUT/tu.txt" >"$OUT/tu.norm"
    if [ "$rc_o" -ne "$rc_u" ] || ! cmp -s "$OUT/to.norm" "$OUT/tu.norm"; then
        echo "TORTURE DIVERGENCE in $SUITE (rc ours=$rc_o upstream=$rc_u)" >&2
        diff -u "$OUT/tu.norm" "$OUT/to.norm" | head -40 >&2
        exit 1
    fi
    echo "$SUITE: identical to upstream"
done

round=0
while [ "$round" -lt "$ROUNDS" ]; do
    rc_o=0; FUZZ_SEED=$round "$OUT/mockdiff_ours" >"$OUT/o.txt" 2>&1 || rc_o=$?
    rc_u=0; FUZZ_SEED=$round "$OUT/mockdiff_upstream" >"$OUT/u.txt" 2>&1 || rc_u=$?
    norm <"$OUT/o.txt" >"$OUT/o.norm"
    norm <"$OUT/u.txt" >"$OUT/u.norm"
    if [ "$rc_o" -ne "$rc_u" ] || ! cmp -s "$OUT/o.norm" "$OUT/u.norm"; then
        echo "DIVERGENCE at FUZZ_SEED=$round (rc ours=$rc_o upstream=$rc_u)" >&2
        diff -u "$OUT/u.norm" "$OUT/o.norm" | head -40 >&2
        exit 1
    fi
    round=$((round + 1))
done
echo "fuzz_mock_diff: $ROUNDS rounds identical to upstream"
echo "fuzz gate green"
