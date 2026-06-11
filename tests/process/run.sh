#!/bin/sh
# Process tests (lite): -jN parallel workers.
# $1 = process_tests binary. Run from repo root.
set -eu
BIN="$1"
fail=0

norm_ms() {
    sed 's/, [0-9]* ms)/, 0 ms)/'
}

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

# bare -j with a crashing test: the affected worker dies, is reported, and
# the run still completes with a nonzero exit
rc=0; out=$("$BIN" -j2 2>&1) || rc=$?
if [ "$rc" -ge 1 ] && printf '%s' "$out" | grep -qE "worker [0-9]+ terminated abnormally"; then
    echo "ok: -j2 reports dead worker"
else
    echo "FAILED: -j2 dead worker rc=$rc out:" >&2; printf '%s\n' "$out" >&2; fail=1
fi

exit "$fail"
