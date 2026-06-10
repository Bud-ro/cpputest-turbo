#!/bin/sh
# Static analysis gate: gcc -fanalyzer over the C core and mock core.
# Interprocedural path analysis (NULL derefs, leaks, use-after-free) on top
# of the warning set the normal build already enforces. Skips (successfully)
# when the compiler does not support -fanalyzer (e.g. clang).
set -eu
cd "$(dirname "$0")/.."

CC="${CC:-gcc}"
OUT=build/analyzer
mkdir -p "$OUT"

if ! $CC -fanalyzer -x c -c /dev/null -o "$OUT/probe.o" 2>/dev/null; then
    echo "analyzer: $CC does not support -fanalyzer, skipping"
    exit 0
fi

status=0
for f in src/core/*.c src/mock/*.c; do
    if ! $CC -fanalyzer -Werror -std=c11 -O1 -Iinclude \
            -c "$f" -o "$OUT/$(basename "$f").o" 2>"$OUT/err.txt"; then
        echo "ANALYZER FINDINGS in $f:" >&2
        cat "$OUT/err.txt" >&2
        status=1
    fi
done

[ "$status" -eq 0 ] && echo "analyzer gate green"
exit "$status"
