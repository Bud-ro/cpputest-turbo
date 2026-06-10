#!/bin/sh
# Output-format tests: JUnit XML files, TeamCity stream, -vv traces.
# $1 = build/tests dir (contains cli_tests and pass_tests). Run from repo root.
set -eu
BIN_DIR="$(cd "$1" && pwd)"
ROOT="$(pwd)"
SCRATCH="${TMPDIR:-/tmp}/cpputest-outputs-test"
fail=0

compare() { # label actual golden
    if ! diff -u "$ROOT/tests/outputs/golden/$3" "$2"; then
        echo "FAILED golden comparison: $1" >&2
        fail=1
    else
        echo "ok: $1"
    fi
}

norm_xml() {
    sed -e 's/time="[0-9]*\.[0-9]*"/time="0.000"/g' -e 's/timestamp="[^"]*"/timestamp="TIMESTAMP"/'
}

# --- JUnit -------------------------------------------------------------------
rm -rf "$SCRATCH"; mkdir -p "$SCRATCH"; cd "$SCRATCH"

"$BIN_DIR/cli_tests" -ojunit -ri >/dev/null 2>&1 || true
norm_xml <cpputest_GroupA.xml >GroupA.norm
norm_xml <cpputest_GroupB.xml >GroupB.norm
compare "junit GroupA xml" GroupA.norm junit_GroupA.xml
compare "junit GroupB xml (failure)" GroupB.norm junit_GroupB.xml

rm -f cpputest_*.xml
"$BIN_DIR/cli_tests" -ojunit -k mypkg >/dev/null 2>&1
[ -f cpputest_mypkg_GroupA.xml ] && echo "ok: junit -k filename" || { echo "FAILED: junit -k filename" >&2; fail=1; }
norm_xml <cpputest_mypkg_GroupB.xml >pkgB.norm
compare "junit package classname" pkgB.norm junit_pkg_GroupB.xml

# JUnit prints nothing to stdout without -v
out=$("$BIN_DIR/cli_tests" -ojunit 2>&1)
if [ -n "$out" ]; then echo "FAILED: -ojunit should be silent, got: $out" >&2; fail=1; else echo "ok: junit silent"; fi
# ... and composes with console under -v
out=$("$BIN_DIR/cli_tests" -ojunit -v 2>&1 | grep -c 'TEST(') || true
[ "$out" = "4" ] && echo "ok: junit -v composite" || { echo "FAILED: junit -v composite ($out)" >&2; fail=1; }

cd "$ROOT"

# --- TeamCity ----------------------------------------------------------------
"$BIN_DIR/cli_tests" -oteamcity 2>&1 | sed "s/duration='[0-9]*'/duration='0'/g; s/, [0-9]* ms)/, 0 ms)/" >"$SCRATCH/tc.out"
compare "teamcity output" "$SCRATCH/tc.out" teamcity.txt
"$BIN_DIR/cli_tests" -oteamcity -ri 2>&1 | sed "s/duration='[0-9]*'/duration='0'/g; s/, [0-9]* ms)/, 0 ms)/" >"$SCRATCH/tc_ri.out" || true
compare "teamcity failure output" "$SCRATCH/tc_ri.out" teamcity_ri.txt

# --- very verbose --------------------------------------------------------------
"$BIN_DIR/pass_tests" -vv 2>&1 | sed 's/, [0-9]* ms)/, 0 ms)/' >"$SCRATCH/vv.out"
compare "-vv traces" "$SCRATCH/vv.out" veryverbose.txt

exit "$fail"
