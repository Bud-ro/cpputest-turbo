#!/bin/sh
# Memory leak detection tests. $1 = leak_tests binary. Run from repo root.
set -eu
BIN="$1"
fail=0

norm() {
    sed -e 's/Alloc num ([0-9]*)/Alloc num (N)/' -e 's/<0x[0-9a-f]*>/<0xADDR>/' -e 's/, [0-9]* ms)/, 0 ms)/'
}

rc=0; "$BIN" 2>&1 | norm > "${TMPDIR:-/tmp}/leaks_all.out" || rc=$?
rc=0; "$BIN" >/dev/null 2>&1 || rc=$?
if [ "$rc" -ne 5 ]; then echo "FAILED: leak_tests exit $rc, expected 5" >&2; fail=1; fi
if ! diff -u tests/leaks/golden/all.txt "${TMPDIR:-/tmp}/leaks_all.out"; then
    echo "FAILED: leak goldens" >&2; fail=1
else
    echo "ok: leak failure formats"
fi

rc=0; "$BIN" -sn ignored 2>&1 | norm > "${TMPDIR:-/tmp}/leaks_final.out" || rc=$?
if ! diff -u tests/leaks/golden/final_report.txt "${TMPDIR:-/tmp}/leaks_final.out"; then
    echo "FAILED: FinalReport golden" >&2; fail=1
else
    echo "ok: FinalReport on green run"
fi

exit "$fail"
