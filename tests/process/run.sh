#!/bin/sh
# Phase 8 tests: -p fork isolation (crash containment) and -jN parallel.
# $1 = process_tests binary. Run from repo root.
set -eu
BIN="$1"
fail=0

norm_ms() {
    sed 's/, [0-9]* ms)/, 0 ms)/'
}

# --- fork isolation: a SIGSEGV test cannot kill the run ----------------------
rc=0; out=$("$BIN" -p 2>&1) || rc=$?
if [ "$rc" -ne 2 ]; then
    echo "FAILED: -p run exit $rc, expected 2 (crash + failure)" >&2; fail=1
fi
if printf '%s' "$out" | grep -q "Failed in separate process - killed by signal 11" \
   && printf '%s' "$out" | grep -q "Failed in separate process" \
   && printf '%s' "$out" | grep -qE "Errors \(2 failures, 7 tests, 7 ran"; then
    echo "ok: -p contains crashes and failures"
else
    echo "FAILED: -p output unexpected:" >&2
    printf '%s\n' "$out" >&2
    fail=1
fi

# all-stable subset under -p stays green
rc=0; out=$("$BIN" -p -g Stable 2>&1) || rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q "OK (7 tests, 5 ran"; then
    echo "ok: -p green run"
else
    echo "FAILED: -p green run rc=$rc" >&2; printf '%s\n' "$out" >&2; fail=1
fi

# --- parallel workers ---------------------------------------------------------
rc=0; out=$("$BIN" -j3 -g Stable 2>&1 | norm_ms) || rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -q "OK (7 tests, 5 ran, 5 checks, 0 ignored, 2 filtered out, 0 ms)"; then
    echo "ok: -j3 parallel merges counts"
else
    echo "FAILED: -j3 rc=$rc out:" >&2; printf '%s\n' "$out" >&2; fail=1
fi

# parallel run is deterministic: same output twice
one=$("$BIN" -j2 -g Stable 2>&1 | norm_ms)
two=$("$BIN" -j2 -g Stable 2>&1 | norm_ms)
if [ "$one" = "$two" ]; then
    echo "ok: -j2 deterministic output"
else
    echo "FAILED: -j2 nondeterministic output" >&2; fail=1
fi

# -j composed with -p: crashes contained per test inside each worker
rc=0; out=$("$BIN" -j2 -p 2>&1) || rc=$?
if [ "$rc" -ge 1 ] && printf '%s' "$out" | grep -qE "Errors \(2 failures, 7 tests, 7 ran" \
   && printf '%s' "$out" | grep -q "killed by signal 11"; then
    echo "ok: -j2 -p full containment"
else
    echo "FAILED: -j2 -p rc=$rc out:" >&2; printf '%s\n' "$out" >&2; fail=1
fi

# bare -j with a crashing test: the affected worker dies, is reported, and
# the run still completes with a nonzero exit
rc=0; out=$("$BIN" -j2 2>&1) || rc=$?
if [ "$rc" -ge 1 ] && printf '%s' "$out" | grep -qE "worker [0-9]+ terminated abnormally"; then
    echo "ok: -j2 reports dead worker"
else
    echo "FAILED: -j2 dead worker rc=$rc out:" >&2; printf '%s\n' "$out" >&2; fail=1
fi

exit "$fail"
