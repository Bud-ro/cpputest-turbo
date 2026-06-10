#!/bin/sh
# Compile-time and runtime benchmark: cpputest-turbo vs vendored upstream.
# Requires build/libCppUTest.a (ours) and .upstream-cache/libCppUTestUpstream.a.
set -eu
cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
SCRATCH="${TMPDIR:-/tmp}/cpputest-bench"
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH/src" "$SCRATCH/ours" "$SCRATCH/upstream"

sh bench/gen_bench.sh "$SCRATCH/src" 20 20

# build the upstream library once (cached across runs, wiped by make clean)
if [ \! -f .upstream-cache/libCppUTestUpstream.a ]; then
    mkdir -p .upstream-cache
    ( cd .upstream-cache && \
      g++ -std=c++11 -w -O2 -c ../third_party/cpputest/src/CppUTest/*.cpp \
          ../third_party/cpputest/src/Platforms/Gcc/UtestPlatform.cpp \
          -I../third_party/cpputest/include && \
      ar rcs libCppUTestUpstream.a *.o )
fi

now_ms() {
    date +%s%N | cut -c1-13
}

compile_all() { # include-dir out-dir
    for f in "$SCRATCH"/src/bench_*.cpp; do
        case "$f" in *bench_runtime.cpp) continue ;; esac
        $CXX -std=c++11 -w -O0 -I"$1" -c "$f" -o "$2/$(basename "$f").o"
    done
}

echo "== compile-time: 21 TUs, 400 TESTs total (g++ -O0) =="
start=$(now_ms); compile_all include "$SCRATCH/ours"; end=$(now_ms)
ours_compile=$((end - start))
echo "ours:     ${ours_compile} ms"
start=$(now_ms); compile_all third_party/cpputest/include "$SCRATCH/upstream"; end=$(now_ms)
up_compile=$((end - start))
echo "upstream: ${up_compile} ms"

echo "== runtime: 5M assertions + 500k new/delete, 8 groups (O2) =="
$CXX -std=c++11 -w -O2 -Iinclude "$SCRATCH/src/bench_runtime.cpp" \
    "$SCRATCH/src/bench_main.cpp" build/libCppUTest.a -o "$SCRATCH/rt_ours"
$CXX -std=c++11 -w -O2 -Ithird_party/cpputest/include "$SCRATCH/src/bench_runtime.cpp" \
    "$SCRATCH/src/bench_main.cpp" .upstream-cache/libCppUTestUpstream.a -o "$SCRATCH/rt_upstream"

start=$(now_ms); "$SCRATCH/rt_ours" >/dev/null; end=$(now_ms)
ours_rt=$((end - start))
echo "ours:     ${ours_rt} ms"
start=$(now_ms); "$SCRATCH/rt_upstream" >/dev/null; end=$(now_ms)
up_rt=$((end - start))
echo "upstream: ${up_rt} ms"

echo
echo "| metric | cpputest-turbo | upstream CppUTest |"
echo "|---|---|---|"
echo "| compile 21 TUs / 400 tests (-O0) | ${ours_compile} ms | ${up_compile} ms |"
echo "| run 5M assertions + 500k new/delete (8 groups) (-O2) | ${ours_rt} ms | ${up_rt} ms |"
