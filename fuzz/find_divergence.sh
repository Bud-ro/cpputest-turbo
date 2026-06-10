#!/bin/sh
# Finds the first divergent seed between the two mockdiff binaries and
# prints the diff. Usage: sh fuzz/find_divergence.sh [max_seed]
set -eu
cd "$(dirname "$0")/.."
OUT=build/fuzz
MAX="${1:-30}"

s=0
while [ "$s" -lt "$MAX" ]; do
    rc=0; FUZZ_SEED=$s ASAN_OPTIONS=detect_leaks=0 "$OUT/mockdiff_ours" >"$OUT/o.txt" 2>&1 || rc=$?
    rc=0; FUZZ_SEED=$s "$OUT/mockdiff_upstream" >"$OUT/u.txt" 2>&1 || rc=$?
    sed 's/, [0-9]* ms)/, 0 ms)/' "$OUT/o.txt" > "$OUT/o.norm"
    sed 's/, [0-9]* ms)/, 0 ms)/' "$OUT/u.txt" > "$OUT/u.norm"
    if cmp -s "$OUT/o.norm" "$OUT/u.norm"; then
        s=$((s + 1))
        continue
    fi
    echo "first divergent seed: $s"
    diff -u "$OUT/u.norm" "$OUT/o.norm" | head -30
    exit 0
done
echo "no divergence in seeds 0..$MAX"
