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
CFLAGS="-std=c11 $SAN -O1 -g -Iinclude"
CXXFLAGS="-std=c++11 $SAN -O1 -g -Iinclude"
ROUNDS="${FUZZ_ROUNDS:-20}"

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

echo "== mock torture suite (differential vs upstream) =="
g++ $CXXFLAGS tests/mock_torture/torture.cpp build/libCppUTestExt.a "$OUT/libasan.a" \
    -o "$OUT/torture_ours"
g++ -std=c++11 -w -O1 -g -Ithird_party/cpputest/include tests/mock_torture/torture.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/torture_upstream"
rc_o=0; "$OUT/torture_ours" >"$OUT/to.txt" 2>&1 || rc_o=$?
rc_u=0; "$OUT/torture_upstream" >"$OUT/tu.txt" 2>&1 || rc_u=$?
norm <"$OUT/to.txt" >"$OUT/to.norm"
norm <"$OUT/tu.txt" >"$OUT/tu.norm"
if [ "$rc_o" -ne "$rc_u" ] || ! cmp -s "$OUT/to.norm" "$OUT/tu.norm"; then
    echo "TORTURE DIVERGENCE (rc ours=$rc_o upstream=$rc_u)" >&2
    diff -u "$OUT/tu.norm" "$OUT/to.norm" | head -40 >&2
    exit 1
fi
echo "mock torture: identical to upstream"

echo "== MockSupportPlugin torture (differential vs upstream) =="
g++ $CXXFLAGS tests/mock_torture/plugin_torture.cpp build/libCppUTestExt.a "$OUT/libasan.a" \
    -o "$OUT/ptorture_ours"
g++ -std=c++11 -w -O1 -g -Ithird_party/cpputest/include tests/mock_torture/plugin_torture.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/ptorture_upstream"
rc_o=0; "$OUT/ptorture_ours" >"$OUT/pto.txt" 2>&1 || rc_o=$?
rc_u=0; "$OUT/ptorture_upstream" >"$OUT/ptu.txt" 2>&1 || rc_u=$?
norm <"$OUT/pto.txt" >"$OUT/pto.norm"
norm <"$OUT/ptu.txt" >"$OUT/ptu.norm"
if [ "$rc_o" -ne "$rc_u" ] || ! cmp -s "$OUT/pto.norm" "$OUT/ptu.norm"; then
    echo "PLUGIN TORTURE DIVERGENCE (rc ours=$rc_o upstream=$rc_u)" >&2
    diff -u "$OUT/ptu.norm" "$OUT/pto.norm" | head -40 >&2
    exit 1
fi
echo "plugin torture: identical to upstream"

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
