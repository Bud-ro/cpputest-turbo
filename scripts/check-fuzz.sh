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

CC="${CC:-gcc}"
CXX="${CXX:-g++}"
OUT=build/fuzz
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all"
OPT="${FUZZ_OPT:--O1}"
CFLAGS="-std=c11 $SAN $OPT -g -Iinclude"
CXXFLAGS="-std=c++11 $SAN $OPT -g -Iinclude"
ROUNDS="${FUZZ_ROUNDS:-20}"
echo "fuzz optimization level: $OPT"

# sanitizer-instrumented library (core + mock + shim — self-contained)
rm -f "$OUT"/*.o
for f in src/core/*.c src/mock/*.c; do
    $CC $CFLAGS -c "$f" -o "$OUT/$(basename "$f").o"
done
$CXX $CXXFLAGS -c src/shim/newdelete.cpp -o "$OUT/newdelete.o"
$CXX $CXXFLAGS -c src/shim/simplestring.cpp -o "$OUT/simplestring.o"
rm -f "$OUT/libasan.a"
ar rcs "$OUT/libasan.a" "$OUT"/*.o

# upstream library for the differential fuzzer
if [ ! -f .upstream-cache/libCppUTestUpstream.a ]; then
    mkdir -p .upstream-cache
    ( cd .upstream-cache && \
      g++ -std=c++11 -w -O2 -c ../third_party/cpputest/src/CppUTest/*.cpp \
          ../third_party/cpputest/src/CppUTestExt/*.cpp \
          ../third_party/cpputest/src/Platforms/Gcc/UtestPlatform.cpp \
          -I../third_party/cpputest/include && \
      ar rcs libCppUTestUpstream.a *.o )
fi

$CC $CFLAGS fuzz/fuzz_args.c "$OUT/libasan.a" -o "$OUT/fuzz_args"
$CC $CFLAGS fuzz/fuzz_strings.c "$OUT/libasan.a" -o "$OUT/fuzz_strings"
$CC $CFLAGS fuzz/fuzz_memleak.c "$OUT/libasan.a" -o "$OUT/fuzz_memleak"

# deliberate failure paths leak longjmp'd temporaries by design; memory
# SAFETY is what these hunt. poison_array_cookie=0: the leak tracker
# 0xCD-fills the whole tracked block (new[] cookie included) inside
# operator delete[], which clang's array-cookie poisoning would flag.
export ASAN_OPTIONS="detect_leaks=0:poison_array_cookie=0"

echo "== fuzz_args =="
FUZZ_SEED=1 "$OUT/fuzz_args"
echo "== fuzz_strings =="
FUZZ_SEED=1 "$OUT/fuzz_strings"
echo "== fuzz_memleak =="
FUZZ_SEED=1 "$OUT/fuzz_memleak"

echo "== fuzz_mock_diff (differential vs upstream, $ROUNDS rounds x 50 seqs) =="
# NOTE: link ONLY libasan.a (it contains the mock objects too) — putting the
# uninstrumented build/libCppUTestExt.a first would resolve all mock symbols
# from the plain -O2 library and silently strip sanitizer coverage from the
# primary fuzz target
$CXX $CXXFLAGS fuzz/fuzz_mock_diff.cpp "$OUT/libasan.a" \
    -o "$OUT/mockdiff_ours"
$CXX -std=c++11 -w -O1 -g -Ithird_party/cpputest/include fuzz/fuzz_mock_diff.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/mockdiff_upstream"

# differential mock_c fuzzer: C11 driver TU + C++ runner TU (the C-code-
# under-test / C++-test-binding split), same diff harness
$CC $CFLAGS -c fuzz/fuzz_mock_c_seq.c -o "$OUT/mock_c_seq_ours.o"
$CXX $CXXFLAGS fuzz/fuzz_mock_c_diff.cpp "$OUT/mock_c_seq_ours.o" \
    "$OUT/libasan.a" -o "$OUT/mockcdiff_ours"
$CC -std=c11 -w -O1 -g -Ithird_party/cpputest/include -c fuzz/fuzz_mock_c_seq.c \
    -o "$OUT/mock_c_seq_up.o"
$CXX -std=c++11 -w -O1 -g -Ithird_party/cpputest/include \
    fuzz/fuzz_mock_c_diff.cpp "$OUT/mock_c_seq_up.o" \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/mockcdiff_upstream"

# differential composition fuzzer: asserts + heap + leaks + mocks +
# SimpleString + UT_PTR_SET interleaved in one test body
$CXX $CXXFLAGS fuzz/fuzz_compose_diff.cpp "$OUT/libasan.a" \
    -o "$OUT/composediff_ours"
$CXX -std=c++11 -w -O1 -g -Ithird_party/cpputest/include \
    fuzz/fuzz_compose_diff.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/composediff_upstream"

norm() {
    # ms timings + library-INTERNAL failure locations (e.g. the type-check
    # assert in returnIntValue fires inside MockNamedValue.cpp upstream and
    # inside our MockSupport.h — same behavior, unmatchable source paths) +
    # leak-report heap addresses and allocation ordinals (the ordinal counts
    # INTERNAL allocations, which legitimately differ between the libs)
    sed -e 's/, [0-9]* ms)/, 0 ms)/' -e 's/ - [0-9]* ms$/ - 0 ms/' \
        -e 's|[^ ]*/MockNamedValue\.cpp:[0-9]*: error:|LIB_INTERNAL: error:|' \
        -e 's|include/CppUTestExt/MockSupport\.h:[0-9]*: error:|LIB_INTERNAL: error:|' \
        -e 's|[^ ]*/mock_support_c\.c:[0-9]*: error:|LIB_INTERNAL: error:|' \
        -e 's/Alloc num ([0-9]*)/Alloc num (N)/' \
        -e 's/Memory: <0x[0-9a-fA-F]*>/Memory: <0xADDR>/'
}

echo "== differential torture suites (vs upstream) =="
for SUITE in tests/mock_torture/torture.cpp tests/mock_torture/plugin_torture.cpp \
             tests/mock_torture/sstring_torture.cpp; do
    $CXX $CXXFLAGS "$SUITE" "$OUT/libasan.a" \
        -o "$OUT/t_ours"
    $CXX -std=c++11 -w -O1 -g -Ithird_party/cpputest/include "$SUITE" \
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
    for PAIR in mockdiff mockcdiff composediff; do
        rc_o=0; FUZZ_SEED=$round "$OUT/${PAIR}_ours" >"$OUT/o.txt" 2>&1 || rc_o=$?
        rc_u=0; FUZZ_SEED=$round "$OUT/${PAIR}_upstream" >"$OUT/u.txt" 2>&1 || rc_u=$?
        norm <"$OUT/o.txt" >"$OUT/o.norm"
        norm <"$OUT/u.txt" >"$OUT/u.norm"
        if [ "$rc_o" -ne "$rc_u" ] || ! cmp -s "$OUT/o.norm" "$OUT/u.norm"; then
            echo "DIVERGENCE ($PAIR) at FUZZ_SEED=$round (rc ours=$rc_o upstream=$rc_u)" >&2
            diff -u "$OUT/u.norm" "$OUT/o.norm" | head -40 >&2
            exit 1
        fi
    done
    round=$((round + 1))
done
echo "fuzz_mock_diff + fuzz_mock_c_diff + fuzz_compose_diff: $ROUNDS rounds identical to upstream"
echo "fuzz gate green"
