#!/bin/sh
# Fuzzing gate.
# - fuzz_args / fuzz_strings: seeded random-op drivers built with ASan/UBSan;
#   any sanitizer report fails the run.
# - fuzz_mock_diff: DIFFERENTIAL — the same public-API driver compiled
#   against upstream CppUTest and against us, run with identical seeds; any
#   output divergence (after ms normalization) fails the run. The "ours"
#   side also runs under ASan/UBSan.
# FUZZ_ROUNDS scales the DIFFERENTIAL legs (mock/compose rounds and
# CLI combo count); FUZZ_ITERS scales only the three standalone sanitizer
# fuzzers. Defaults are CI-sized.
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
$CXX $CXXFLAGS -fno-exceptions -fno-rtti -c src/shim/simplestring.cpp \
    -o "$OUT/simplestring.o"
rm -f "$OUT/libasan.a"
ar rcs "$OUT/libasan.a" "$OUT"/*.o

# Upstream library for the differential fuzzer. Built with the SAME $CXX as
# the matrix leg —
# the clang CI leg must diff against a clang-built oracle — and the cache
# stamp records the compiler so switching CXX rebuilds it.
STAMP="fork-enabled-$(basename "$CXX")"
if [ ! -f .upstream-cache/libCppUTestUpstream.a ] ||
   [ ! -f ".upstream-cache/$STAMP" ]; then
    rm -rf .upstream-cache
    mkdir -p .upstream-cache
    ( cd .upstream-cache && \
      "$CXX" -std=c++11 -w -O2 -DCPPUTEST_HAVE_FORK=1 \
          -DCPPUTEST_HAVE_WAITPID=1 -DCPPUTEST_HAVE_KILL=1 \
          -c ../third_party/cpputest/src/CppUTest/*.cpp \
          ../third_party/cpputest/src/CppUTestExt/*.cpp \
          ../third_party/cpputest/src/Platforms/Gcc/UtestPlatform.cpp \
          -I../third_party/cpputest/include && \
      ar rcs libCppUTestUpstream.a *.o && touch "$STAMP" )
fi

$CC $CFLAGS fuzz/fuzz_args.c "$OUT/libasan.a" -o "$OUT/fuzz_args"
$CC $CFLAGS fuzz/fuzz_strings.c "$OUT/libasan.a" -o "$OUT/fuzz_strings"

# deliberate failure paths leak longjmp'd temporaries by design; memory
# SAFETY is what these hunt. poison_array_cookie=0: the leak tracker
# 0xCD-fills the whole tracked block (new[] cookie included) inside
# operator delete[], which clang's array-cookie poisoning would flag.
export ASAN_OPTIONS="detect_leaks=0:poison_array_cookie=0"

echo "== fuzz_args =="
FUZZ_SEED=1 "$OUT/fuzz_args"
echo "== fuzz_strings =="
FUZZ_SEED=1 "$OUT/fuzz_strings"

echo "== fuzz_mock_diff (differential vs upstream, $ROUNDS rounds x 50 seqs) =="
# NOTE: link ONLY libasan.a (it contains the mock objects too) — putting the
# uninstrumented build/libCppUTestExt.a first would resolve all mock symbols
# from the plain -O2 library and silently strip sanitizer coverage from the
# primary fuzz target
$CXX $CXXFLAGS fuzz/fuzz_mock_diff.cpp "$OUT/libasan.a" \
    -o "$OUT/mockdiff_ours"
$CXX -std=c++11 -w -O1 -g -Ithird_party/cpputest/include fuzz/fuzz_mock_diff.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/mockdiff_upstream"

# differential composition fuzzer: asserts + mocks + SimpleString
# interleaved in one test body
$CXX $CXXFLAGS fuzz/fuzz_compose_diff.cpp "$OUT/libasan.a" \
    -o "$OUT/composediff_ours"
$CXX -std=c++11 -w -O1 -g -Ithird_party/cpputest/include \
    fuzz/fuzz_compose_diff.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/composediff_upstream"

# differential CLI fuzzer: one fixed suite, seeded random runner-flag
# combinations; stdout and exit code compared
$CXX $CXXFLAGS fuzz/fuzz_cli_diff.cpp "$OUT/libasan.a" \
    -o "$OUT/clidiff_ours"
$CXX -std=c++11 -w -O1 -g -Ithird_party/cpputest/include \
    fuzz/fuzz_cli_diff.cpp \
    .upstream-cache/libCppUTestUpstream.a -o "$OUT/clidiff_upstream"

norm() {
    # ms timings + library-INTERNAL failure locations (e.g. the type-check
    # assert in returnIntValue fires inside MockNamedValue.cpp upstream and
    # inside our MockSupport.h — same behavior, unmatchable source paths) +
    # leak-report heap addresses and allocation ordinals (the ordinal counts
    # INTERNAL allocations, which legitimately differ between the libs)
    sed -e 's/, [0-9]* ms)/, 0 ms)/' -e 's/ - [0-9]* ms$/ - 0 ms/' \
        -e 's|[^ ]*/MockNamedValue\.cpp:[0-9]*: error:|LIB_INTERNAL: error:|' \
        -e 's|include/CppUTestExt/MockSupport\.h:[0-9]*: error:|LIB_INTERNAL: error:|' \
        -e 's/Alloc num ([0-9]*)/Alloc num (N)/' \
        -e 's/Memory: <0x[0-9a-fA-F]*>/Memory: <0xADDR>/' \
        -e 's/\([ :<(]\)0x[0-9a-fA-F]\{5,\}/\10xBIGADDR/g'
}
# the 5+-hex-digit scrub is ANCHORED to value positions (after space/colon/
# </paren) so it cannot eat a fuzzer-chosen fake constant; keep all fake
# pointers below 0x10000 regardless

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
    for PAIR in mockdiff composediff; do
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
echo "fuzz_mock_diff + fuzz_compose_diff: $ROUNDS rounds identical to upstream"

# ---- CLI flag-combination differential ----
cli_norm() {
    norm
}

# hand-rolled LCG (not awk srand/rand: their sequences differ across awk
# implementations, so the explored combos would depend on the host's awk)
# Pool = the lite flag surface only; removed flags are usage errors here but
# valid upstream, so they cannot be diffed.
gen_flags() {
    awk -v seed="$1" 'BEGIN {
        s = seed * 2654435761 % 4294967296;
        pool[0]="-v";    pool[1]="-ri";       pool[2]="-gGroupA";  pool[3]="-npass";
        pool[4]="-sgGroupB"; pool[5]="-snfails"; pool[6]="-xgGroupC";
        pool[7]="-xnignored"; pool[8]="-tGroupA.pass"; pool[9]="-stGroupB.pass";
        pool[10]="-xtGroupA.fails"; pool[11]="-xstGroupC.mockPass";
        pool[12]="-xsgGroupA"; pool[13]="-xsnmockFails";
        s = (s * 1103515245 + 12345) % 2147483648;
        n = int(s / 65536) % 6;
        for (i = 0; i < n; i++) {
            s = (s * 1103515245 + 12345) % 2147483648;
            printf "%s ", pool[int(s / 65536) % 14];
        }
        print ""
    }'
}

CLI_ROUNDS=$((ROUNDS * 4))
round=0
while [ "$round" -lt "$CLI_ROUNDS" ]; do
    FLAGS=$(gen_flags "$round")
    rm -rf "$OUT/cli_o" "$OUT/cli_u"
    mkdir -p "$OUT/cli_o" "$OUT/cli_u"
    rc_o=0
    (cd "$OUT/cli_o" && ../clidiff_ours $FLAGS >stdout.raw 2>&1) || rc_o=$?
    rc_u=0
    (cd "$OUT/cli_u" && ../clidiff_upstream $FLAGS >stdout.raw 2>&1) || rc_u=$?
    if [ "$rc_o" -ne "$rc_u" ]; then
        echo "CLI DIVERGENCE (exit) at round=$round flags='$FLAGS': ours=$rc_o upstream=$rc_u" >&2
        exit 1
    fi
    for f in $(cd "$OUT/cli_u" && ls); do
        if [ ! -f "$OUT/cli_o/$f" ]; then
            echo "CLI DIVERGENCE (missing file $f) at round=$round flags='$FLAGS'" >&2
            exit 1
        fi
        cli_norm <"$OUT/cli_o/$f" >"$OUT/cli_f_o.norm"
        cli_norm <"$OUT/cli_u/$f" >"$OUT/cli_f_u.norm"
        if ! cmp -s "$OUT/cli_f_o.norm" "$OUT/cli_f_u.norm"; then
            echo "CLI DIVERGENCE ($f) at round=$round flags='$FLAGS'" >&2
            diff -u "$OUT/cli_f_u.norm" "$OUT/cli_f_o.norm" | head -30 >&2
            exit 1
        fi
    done
    for f in $(cd "$OUT/cli_o" && ls); do
        if [ ! -f "$OUT/cli_u/$f" ]; then
            echo "CLI DIVERGENCE (extra file $f) at round=$round flags='$FLAGS'" >&2
            exit 1
        fi
    done
    round=$((round + 1))
done
echo "fuzz_cli_diff: $CLI_ROUNDS flag combos identical to upstream"
echo "fuzz gate green"
