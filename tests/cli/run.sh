#!/bin/sh
# CLI behavior tests (lite): flags, filters, exit codes.
# Run from repo root; $1 = path to cli_tests binary.
set -eu
BIN="$1"
fail=0

expect_eq() { # label actual expected
    if [ "$2" != "$3" ]; then
        printf 'FAILED %s:\n  actual:   %s\n  expected: %s\n' "$1" "$2" "$3" >&2
        fail=1
    else
        echo "ok: $1"
    fi
}

expect_rc() { # label rc expected
    expect_eq "$1 (exit code)" "$2" "$3"
}

norm_ms() {
    sed 's/, [0-9]* ms)/, 0 ms)/'
}

summary_of() { # args... -> prints the OK/Errors line, ms normalized
    "$BIN" "$@" 2>&1 | grep -aE '(OK|Errors) \(' | norm_ms || true
}

# --- filters ----------------------------------------------------------------
expect_eq "-g GroupA" "$(summary_of -g GroupA)" \
    "OK (4 tests, 2 ran, 2 checks, 0 ignored, 2 filtered out, 0 ms)"
expect_eq "-g glued" "$(summary_of -gGroupA)" \
    "OK (4 tests, 2 ran, 2 checks, 0 ignored, 2 filtered out, 0 ms)"
expect_eq "-n a1" "$(summary_of -n a1)" \
    "OK (4 tests, 1 ran, 1 checks, 0 ignored, 3 filtered out, 0 ms)"
expect_eq "-g substring" "$(summary_of -g roupA)" \
    "OK (4 tests, 2 ran, 2 checks, 0 ignored, 2 filtered out, 0 ms)"
expect_eq "-sg strict no match" "$(summary_of -sg roupA)" \
    "Errors (ran nothing, 4 tests, 0 ran, 0 checks, 0 ignored, 4 filtered out, 0 ms)"
expect_eq "-xg exclude" "$(summary_of -xg GroupA)" \
    "OK (4 tests, 1 ran, 1 checks, 1 ignored, 2 filtered out, 0 ms)"
expect_eq "-t group.name" "$(summary_of -t GroupA.a1)" \
    "OK (4 tests, 1 ran, 1 checks, 0 ignored, 3 filtered out, 0 ms)"
# -xt adds an inverted filter per dimension; both dimensions must match, so
# ALL GroupA tests and all *a1* names are excluded (upstream semantics)
expect_eq "-xt excludes whole group dimension" "$(summary_of -xt GroupA.a1)" \
    "OK (4 tests, 1 ran, 1 checks, 1 ignored, 2 filtered out, 0 ms)"
expect_eq "TEST() verbose form" "$(summary_of "TEST(GroupA, a2)")" \
    "OK (4 tests, 1 ran, 1 checks, 0 ignored, 3 filtered out, 0 ms)"
rc=0; "$BIN" -sg roupA >/dev/null 2>&1 || rc=$?
expect_rc "ran-nothing" "$rc" 1

# --- parse errors, usage, help ----------------------------------------------
rc=0; out=$("$BIN" -z 2>&1) || rc=$?
expect_rc "bad flag" "$rc" 1
expect_eq "bad flag prints usage" "$(printf '%s' "$out" | head -1)" "use -h for more extensive help"
rc=0; out=$("$BIN" -h 2>&1) || rc=$?
expect_rc "-h" "$rc" 1
expect_eq "-h prints help" "$(printf '%s' "$out" | head -1)" "Thanks for using CppUTest (turbo-lite)."
rc=0; "$BIN" -t NoDotHere >/dev/null 2>&1 || rc=$?
expect_rc "-t without dot" "$rc" 1
rc=0; "$BIN" -pXXX >/dev/null 2>&1 || rc=$?
expect_rc "-pXXX plugin arg" "$rc" 1

# removed flags must be rejected, not silently accepted
for flag in -p -vv -c -b -lg -ln -ll -f -e -ci -r2 -s17 -onormal -ojunit -kpkg; do
    rc=0; "$BIN" "$flag" >/dev/null 2>&1 || rc=$?
    expect_rc "removed flag $flag rejected" "$rc" 1
done

# --- verbose / run-ignored ----------------------------------------------------
out=$("$BIN" -v 2>&1)
expect_eq "-v order" "$(printf '%s\n' "$out" | grep -oE '(IGNORE_)?TEST\([^)]*\)' | tr '\n' ' ')" \
    "IGNORE_TEST(GroupB, ig) TEST(GroupB, b1) TEST(GroupA, a2) TEST(GroupA, a1) "
rc=0; out=$("$BIN" -ri 2>&1) || rc=$?
expect_rc "-ri ignored test now fails" "$rc" 1
expect_eq "-ri summary" "$(printf '%s\n' "$out" | grep -aE '^Errors' | norm_ms)" \
    "Errors (1 failures, 4 tests, 4 ran, 4 checks, 0 ignored, 0 filtered out, 0 ms)"
out=$("$BIN" -ri -v 2>&1) || true
expect_eq "-ri verbose shows TEST not IGNORE_TEST" \
    "$(printf '%s\n' "$out" | grep -c 'IGNORE_TEST')" "0"

exit "$fail"
