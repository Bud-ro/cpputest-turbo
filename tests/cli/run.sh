#!/bin/sh
# CLI behavior tests: flags, filters, listings, repeat, shuffle, exit codes.
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

# --- listings ---------------------------------------------------------------
expect_eq "-lg" "$("$BIN" -lg)" "GroupB GroupA"
expect_eq "-ln" "$("$BIN" -ln)" "GroupB.ig GroupB.b1 GroupA.a2 GroupA.a1"
expect_eq "-ln filtered" "$("$BIN" -ln -g GroupA)" "GroupA.a2 GroupA.a1"
expect_eq "-ll line count" "$("$BIN" -ll | wc -l | tr -d ' ')" "4"
expect_eq "-ll first line" "$("$BIN" -ll | head -1)" "GroupB.ig.tests/cli/cli_tests.cpp.30"
rc=0; "$BIN" -lg >/dev/null || rc=$?; expect_rc "-lg" "$rc" 0

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
expect_eq "-h prints help" "$(printf '%s' "$out" | head -1)" "Thanks for using CppUTest."
expect_eq "-h last line" "$(printf '%s' "$out" | tail -1)" \
    "  -ci               - continuous integration mode (equivalent to -e)"
rc=0; "$BIN" -t NoDotHere >/dev/null 2>&1 || rc=$?
expect_rc "-t without dot" "$rc" 1
rc=0; "$BIN" -obogus >/dev/null 2>&1 || rc=$?
expect_rc "-obogus" "$rc" 1
rc=0; "$BIN" -onormal >/dev/null 2>&1 || rc=$?
expect_rc "-onormal" "$rc" 0
rc=0; "$BIN" -s0 >/dev/null 2>&1 || rc=$?
expect_rc "-s0 parse error" "$rc" 1
rc=0; "$BIN" -pXXX >/dev/null 2>&1 || rc=$?
expect_rc "-pXXX plugin arg" "$rc" 1

# --- repeat / reverse / shuffle / run-ignored --------------------------------
out=$("$BIN" -r2 2>&1)
expect_eq "-r2 run headers" "$(printf '%s\n' "$out" | grep -c '^Test run ')" "2"
expect_eq "-r2 header text" "$(printf '%s\n' "$out" | grep '^Test run ' | head -1)" "Test run 1 of 2"
out=$("$BIN" -v 2>&1)
expect_eq "-v order" "$(printf '%s\n' "$out" | grep -oE '(IGNORE_)?TEST\([^)]*\)' | tr '\n' ' ')" \
    "IGNORE_TEST(GroupB, ig) TEST(GroupB, b1) TEST(GroupA, a2) TEST(GroupA, a1) "
out=$("$BIN" -v -b 2>&1)
expect_eq "-b reversed order" "$(printf '%s\n' "$out" | grep -oE '(IGNORE_)?TEST\([^)]*\)' | tr '\n' ' ')" \
    "TEST(GroupA, a1) TEST(GroupA, a2) TEST(GroupB, b1) IGNORE_TEST(GroupB, ig) "
out=$("$BIN" -s17 2>&1)
expect_eq "-s17 notice" "$(printf '%s\n' "$out" | head -1)" "Test order shuffling enabled with seed: 17"
rc=0; out=$("$BIN" -ri 2>&1) || rc=$?
expect_rc "-ri ignored test now fails" "$rc" 1
expect_eq "-ri summary" "$(printf '%s\n' "$out" | grep -aE '^Errors' | norm_ms)" \
    "Errors (1 failures, 4 tests, 4 ran, 4 checks, 0 ignored, 0 filtered out, 0 ms)"
out=$("$BIN" -ri -v 2>&1) || true
expect_eq "-ri verbose shows TEST not IGNORE_TEST" \
    "$(printf '%s\n' "$out" | grep -c 'IGNORE_TEST')" "0"

# --- crash on fail ------------------------------------------------------------
rc=0; "$BIN" -f -ri -sg GroupB -sn ig >/dev/null 2>&1 || rc=$?
if [ "$rc" -lt 128 ]; then
    echo "FAILED: -f should die by signal (rc >= 128), got $rc" >&2
    fail=1
else
    echo "ok: -f crashes on failure"
fi

# --- color -------------------------------------------------------------------
out=$("$BIN" -c -sg GroupA 2>&1 | grep -a 'OK (' | norm_ms)
expect_eq "-c green summary" "$out" \
    "$(printf '\033[32;1mOK (4 tests, 2 ran, 2 checks, 0 ignored, 2 filtered out, 0 ms)\033[m')"

exit "$fail"
