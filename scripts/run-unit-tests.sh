#!/bin/sh
# Builds and runs our own test suites against the freshly built libs.
# Golden-output tests pin byte-for-byte parity with upstream formats; only
# timings are normalized (test-file line numbers are part of the contract).
set -eu
cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++11 -Wall -Wextra -Werror -O2 -g -Iinclude"
BUILD=build/tests
mkdir -p "$BUILD"

normalize() {
    sed -e 's/ - [0-9]* ms$/ - 0 ms/' -e 's/, [0-9]* ms)/, 0 ms)/'
}

fail=0

compare() { # name actual_file golden_file
    if ! diff -u "$3" "$2"; then
        echo "FAILED golden comparison: $1" >&2
        fail=1
    else
        echo "ok: $1"
    fi
}

# ---- smoke suite -----------------------------------------------------------
$CXX $CXXFLAGS tests/smoke/smoke_tests.cpp build/libCppUTest.a -o "$BUILD/smoke_tests"
$CXX $CXXFLAGS tests/smoke/pass_tests.cpp build/libCppUTest.a -o "$BUILD/pass_tests"

rc=0; "$BUILD/smoke_tests" >"$BUILD/normal.out.raw" 2>&1 || rc=$?
if [ "$rc" -ne 2 ]; then echo "FAILED: smoke_tests exit code $rc, expected 2 (failure count)" >&2; fail=1; fi
normalize <"$BUILD/normal.out.raw" >"$BUILD/normal.out"
compare "smoke normal output" "$BUILD/normal.out" tests/smoke/golden/normal.txt

rc=0; "$BUILD/smoke_tests" -v >"$BUILD/verbose.out.raw" 2>&1 || rc=$?
if [ "$rc" -ne 2 ]; then echo "FAILED: smoke_tests -v exit code $rc, expected 2" >&2; fail=1; fi
normalize <"$BUILD/verbose.out.raw" >"$BUILD/verbose.out"
compare "smoke verbose output" "$BUILD/verbose.out" tests/smoke/golden/verbose.txt

rc=0; "$BUILD/pass_tests" >"$BUILD/pass.out.raw" 2>&1 || rc=$?
if [ "$rc" -ne 0 ]; then echo "FAILED: pass_tests exit code $rc, expected 0" >&2; fail=1; fi
normalize <"$BUILD/pass.out.raw" >"$BUILD/pass.out"
compare "pass output" "$BUILD/pass.out" tests/smoke/golden/pass.txt

# ---- assertion suite (golden failure messages for every macro) -------------
$CXX $CXXFLAGS tests/asserts/assert_tests.cpp build/libCppUTest.a -o "$BUILD/assert_tests"

rc=0; "$BUILD/assert_tests" >"$BUILD/asserts_normal.out.raw" 2>&1 || rc=$?
if [ "$rc" -ne 25 ]; then echo "FAILED: assert_tests exit code $rc, expected 25" >&2; fail=1; fi
normalize <"$BUILD/asserts_normal.out.raw" >"$BUILD/asserts_normal.out"
compare "assert normal output" "$BUILD/asserts_normal.out" tests/asserts/golden/normal.txt

rc=0; "$BUILD/assert_tests" -v >"$BUILD/asserts_verbose.out.raw" 2>&1 || rc=$?
if [ "$rc" -ne 25 ]; then echo "FAILED: assert_tests -v exit code $rc, expected 25" >&2; fail=1; fi
normalize <"$BUILD/asserts_verbose.out.raw" >"$BUILD/asserts_verbose.out"
compare "assert verbose output" "$BUILD/asserts_verbose.out" tests/asserts/golden/verbose.txt

# ---- CLI suite -------------------------------------------------------------
$CXX $CXXFLAGS tests/cli/cli_tests.cpp build/libCppUTest.a -o "$BUILD/cli_tests"
if ! sh tests/cli/run.sh "$BUILD/cli_tests"; then
    fail=1
fi

# ---- output formats (JUnit / TeamCity / -vv) --------------------------------
if ! sh tests/outputs/run.sh "$BUILD"; then
    fail=1
fi

# ---- memory leak detection ---------------------------------------------------
$CXX $CXXFLAGS tests/leaks/leak_tests.cpp build/libCppUTest.a -o "$BUILD/leak_tests"
if ! sh tests/leaks/run.sh "$BUILD/leak_tests"; then
    fail=1
fi

# ---- plugins -----------------------------------------------------------------
$CXX $CXXFLAGS tests/plugins/plugin_tests.cpp build/libCppUTest.a -o "$BUILD/plugin_tests"
rc=0; "$BUILD/plugin_tests" >/dev/null 2>&1 || rc=$?
if [ "$rc" -ne 0 ]; then echo "FAILED: plugin_tests exit $rc" >&2; fail=1; else echo "ok: plugins pre/post/UT_PTR_SET"; fi
rc=0; "$BUILD/plugin_tests" -pcustom >/dev/null 2>&1 || rc=$?
if [ "$rc" -ne 0 ]; then echo "FAILED: plugin_tests -pcustom exit $rc" >&2; fail=1; else echo "ok: plugin parseArguments"; fi
rc=0; "$BUILD/plugin_tests" -pnonsense >/dev/null 2>&1 || rc=$?
if [ "$rc" -ne 1 ]; then echo "FAILED: plugin_tests -pnonsense exit $rc, expected 1" >&2; fail=1; else echo "ok: unparsed plugin arg errors"; fi

# ---- C interface (TestHarness_c) ---------------------------------------------
CC="${CC:-gcc}"
CFLAGS_TEST="-std=c11 -Wall -Wextra -Werror -O2 -g -Iinclude"
$CC $CFLAGS_TEST -c tests/c_interface/c_tests.c -o "$BUILD/c_tests.o"
$CXX $CXXFLAGS tests/c_interface/c_wrappers.cpp "$BUILD/c_tests.o" build/libCppUTest.a -o "$BUILD/c_interface_tests"
rc=0; out=$("$BUILD/c_interface_tests" 2>&1) || rc=$?
if [ "$rc" -ne 1 ]; then echo "FAILED: c_interface_tests exit $rc, expected 1" >&2; fail=1; fi
if printf '%s' "$out" | grep -q "expected <1 (0x1)>" \
   && printf '%s' "$out" | grep -qE "Errors \(1 failures, 3 tests, 2 ran, 18 checks, 1 ignored, 0 filtered out, [0-9]+ ms\)"; then
    echo "ok: C interface (TestHarness_c)"
else
    echo "FAILED: c_interface output unexpected:" >&2
    printf '%s\n' "$out" >&2
    fail=1
fi

# ---- TestTestingFixture --------------------------------------------------------
$CXX $CXXFLAGS tests/fixture/fixture_tests.cpp build/libCppUTest.a -o "$BUILD/fixture_tests"
rc=0; "$BUILD/fixture_tests" >/dev/null 2>&1 || rc=$?
if [ "$rc" -ne 0 ]; then echo "FAILED: fixture_tests exit $rc" >&2; fail=1; else echo "ok: TestTestingFixture"; fi

# ---- CppUMock (basic slice) -----------------------------------------------------
$CXX $CXXFLAGS tests/mock/mock_tests.cpp build/libCppUTestExt.a build/libCppUTest.a -o "$BUILD/mock_tests"
rc=0; "$BUILD/mock_tests" 2>&1 | sed 's/, [0-9]* ms)/, 0 ms)/' > "$BUILD/mock.out" || rc=$?
rc=0; "$BUILD/mock_tests" >/dev/null 2>&1 || rc=$?
if [ "$rc" -ne 12 ]; then echo "FAILED: mock_tests exit $rc, expected 12" >&2; fail=1; fi
compare "mock failure messages" "$BUILD/mock.out" tests/mock/golden/all.txt

# ---- OrderedTest ---------------------------------------------------------------
$CXX $CXXFLAGS tests/ordered/ordered_tests.cpp build/libCppUTest.a -o "$BUILD/ordered_tests"
rc=0; out=$("$BUILD/ordered_tests" -v 2>&1) || rc=$?
order=$(printf '%s\n' "$out" | grep -oE 'TEST\(Ordered, [a-z]+\)' | tr '\n' ' ')
if [ "$rc" -eq 0 ] && [ "$order" = "TEST(Ordered, first) TEST(Ordered, second) TEST(Ordered, third) TEST(Ordered, fourth) TEST(Ordered, fifth) " ]; then
    echo "ok: TEST_ORDERED level ordering"
else
    echo "FAILED: ordered_tests rc=$rc order=$order" >&2
    fail=1
fi

# ---- C-only library build (last: it wipes build/) ---------------------------
if make -s clean >/dev/null && make -s CPPUTEST_C_ONLY=1 >/dev/null; then
    mkdir -p "$BUILD"
    if $CC $CFLAGS_TEST tests/c_interface/c_core_tests.c build/libCppUTest.a -o "$BUILD/c_core_tests" \
       && "$BUILD/c_core_tests" >/dev/null 2>&1; then
        echo "ok: C-only library + pure-C consumer (gcc only, no C++)"
    else
        echo "FAILED: pure-C consumer against C-only lib" >&2
        fail=1
    fi
    make -s clean >/dev/null && make -s >/dev/null
else
    echo "FAILED: C-only library build" >&2
    fail=1
fi

[ "$fail" -eq 0 ] && echo "unit tests: all green"
exit "$fail"
