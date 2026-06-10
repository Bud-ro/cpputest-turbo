#!/bin/sh
# ASan/UBSan sweep over the test suites (valgrind equivalent).
# Known & accepted: tests that exercise FAILING assertions leak the C++
# temporaries alive at longjmp time (longjmp skips destructors; inherent to
# the no-exceptions failure path, same as upstream's). Those suites run with
# leak detection off; everything else must be fully clean.
set -eu
cd "$(dirname "$0")/.."

CC="${CC:-gcc}"
CXX="${CXX:-g++}"
OUT=build/asan
mkdir -p "$OUT"
CFLAGS="-std=c11 -fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g -Iinclude"
CXXFLAGS="-std=c++11 -fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g -Iinclude"

rm -f "$OUT"/*.o
for f in src/core/*.c src/mock/*.c; do
    $CC $CFLAGS -c "$f" -o "$OUT/$(basename "$f").o"
done
$CXX $CXXFLAGS -c src/shim/newdelete.cpp -o "$OUT/newdelete.o"
$CXX $CXXFLAGS -c src/shim/simplestring.cpp -o "$OUT/simplestring.o"
rm -f "$OUT/libCppUTestAsan.a"
ar rcs "$OUT/libCppUTestAsan.a" "$OUT"/*.o

# ASan prints "ERROR: AddressSanitizer..."; standalone UBSan prints only
# "file:line: runtime error: ..." (no ERROR), so grep for both.
sanitizer_report_in() {
    grep -q -e "ERROR" -e "runtime error:" "$1"
}

# suites with deliberate failures exit with their failure count, so the rc
# check only rejects signal deaths (a non-recovering sanitizer abort()s,
# rc >= 128); report text is the primary detector
run_one() { # source leaks(0/1)
    $CXX $CXXFLAGS "$1" "$OUT/libCppUTestAsan.a" -o "$OUT/t.bin"
    rc=0
    ASAN_OPTIONS="detect_leaks=$2" "$OUT/t.bin" >/dev/null 2>"$OUT/err.txt" || rc=$?
    if sanitizer_report_in "$OUT/err.txt" || [ "$rc" -ge 128 ]; then
        echo "SANITIZER ISSUES in $1 (rc=$rc):" >&2
        cat "$OUT/err.txt" >&2
        exit 1
    fi
    echo "ok: $1 (leaks=$2)"
}

run_one tests/smoke/pass_tests.cpp 1
run_one tests/fixture/fixture_tests.cpp 1
run_one tests/cli/cli_tests.cpp 1
run_one tests/ordered/ordered_tests.cpp 1
# suites with deliberate failures: longjmp-over-temporaries leaks accepted
run_one tests/mock_torture/torture.cpp 0
run_one tests/mock/mock_tests.cpp 0
run_one tests/smoke/smoke_tests.cpp 0
run_one tests/asserts/assert_tests.cpp 0
# the leak detector itself: deliberate leaks/corruptions inside, so LSan off
run_one tests/leaks/leak_tests.cpp 0
# fork/parallel runner (deliberate SIGSEGV child + workers)
run_proc() { # binary args
    rc=0
    ASAN_OPTIONS=detect_leaks=0 "$@" >/dev/null 2>"$OUT/err.txt" || rc=$?
    if sanitizer_report_in "$OUT/err.txt" || [ "$rc" -ge 128 ]; then
        echo "SANITIZER ISSUES running $* (rc=$rc):" >&2
        cat "$OUT/err.txt" >&2
        exit 1
    fi
    echo "ok: $*"
}
$CXX $CXXFLAGS tests/process/process_tests.cpp "$OUT/libCppUTestAsan.a" -o "$OUT/proc.bin"
# the deliberate-SIGSEGV test cannot run under sanitizers (the sanitizer
# intercepts the controlled crash: UBSan null-store on Linux, ASan
# DEADLYSIGNAL on Darwin) — exclude it here; tests/process/run.sh covers
# crash containment unsanitized in the normal gate
run_proc "$OUT/proc.bin" -p -xg Hazard
run_proc "$OUT/proc.bin" -j2 -xg Hazard
# pure-C consumers (C ABI + mock_c)
$CC $CFLAGS tests/c_interface/c_core_tests.c "$OUT/libCppUTestAsan.a" -o "$OUT/ccore.bin" -lstdc++ 2>/dev/null || \
    $CC $CFLAGS tests/c_interface/c_core_tests.c "$OUT/libCppUTestAsan.a" -o "$OUT/ccore.bin"
run_proc "$OUT/ccore.bin"

echo "sanitizer sweep green"
